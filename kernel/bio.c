// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13 //bucket数量
#define HASH(blockno) (blockno%NBUCKET)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  int size;
  struct buf buckets[NBUCKET];  // lab8-2
  struct spinlock locks[NBUCKET];   // buckets' locks - lab8-2
  struct spinlock hashlock;     // the hash table's lock - lab8-2
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
} bcache;

void
binit(void)
{
  int i;
  struct buf *b;

  bcache.size = 0; // lab8-2
  initlock(&bcache.lock, "bcache");
  initlock(&bcache.hashlock, "bcache_hash"); // init hash lock - lab8-2
  // init all buckets' locks  - lab8-2
  for (i = 0; i < NBUCKET; ++i)
  {
    initlock(&bcache.locks[i], "bcache_bucket");
  }

  // lab8-2
  //  // Create linked list of buffers
  //  bcache.head.prev = &bcache.head;
  //  bcache.head.next = &bcache.head;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    // lab8-2
    //    b->next = bcache.head.next;
    //    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    //    bcache.head.next->prev = b;
    //    bcache.head.next = b;
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // lab8-2
  int idx = HASH(blockno);
  struct buf *pre, *minb = 0, *minpre;
  uint mintimestamp;
  int i;

  // 拿桶锁，找到了桶下的块，加完引用计数返回这个缓存块
  acquire(&bcache.locks[idx]); // lab8-2
  for (b = bcache.buckets[idx].next; b; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.locks[idx]); // lab8-2
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // check if there is a buf not used -lab8-2
  //没找到但是有空闲块没挂载到桶上，拿一个buf，写一下信息，把buf挂载到对应桶下
  acquire(&bcache.lock);
  if (bcache.size < NBUF)
  {
    b = &bcache.buf[bcache.size++];
    release(&bcache.lock);
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->next = bcache.buckets[idx].next;
    bcache.buckets[idx].next = b;
    release(&bcache.locks[idx]);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock);
  release(&bcache.locks[idx]);

  // select the last-recently used block int the bucket
  // based on the timestamp - lab8-2
  //拿驱逐锁，有锁才能踢buf
  acquire(&bcache.hashlock);
  for (i = 0; i < NBUCKET; ++i)
  {
    mintimestamp = -1;//最大的值
    acquire(&bcache.locks[idx]);
    //这个循环是循环idx桶里面的buf链表，如果找到了中间别的进程加中途加进来的buf，那就直接加引用计数拿buf的锁
    for (pre = &bcache.buckets[idx], b = pre->next; b; pre = b, b = b->next)
    {
      // research the block
      if (idx == HASH(blockno) && b->dev == dev && b->blockno == blockno)
      {
        b->refcnt++;
        release(&bcache.locks[idx]);
        release(&bcache.hashlock);
        acquiresleep(&b->lock);
        return b;
      }
      //没找到buf好吧，直接记录这个b对应的buf的timestamp，用于记录timestamps最小的
      if (b->refcnt == 0 && b->timestamp < mintimestamp)
      {
        minb = b;
        minpre = pre;
        mintimestamp = b->timestamp;
      }
    }
    // 找到这个buf就修改buf信息
    if (minb)
    {
      minb->dev = dev;
      minb->blockno = blockno;
      minb->valid = 0;
      minb->refcnt = 1;
      // if block in another bucket, we should move it to correct bucket
      //如果在别的桶找到的，就要把buf移到blockno对应的桶里面
      if (idx != HASH(blockno))
      {
        minpre->next = minb->next; // 上一个buf的下一个跳过minb连接
        release(&bcache.locks[idx]);
        idx = HASH(blockno); // 正确的桶编号
        acquire(&bcache.locks[idx]);
        minb->next = bcache.buckets[idx].next; // 头插法
        bcache.buckets[idx].next = minb;
      }
      release(&bcache.locks[idx]);
      release(&bcache.hashlock);
      acquiresleep(&minb->lock);
      return minb;
    }
    release(&bcache.locks[idx]);
    if (++idx == NBUCKET)
    {
      idx = 0;
    }
  }
  // lab8-2
  //  // Recycle the least recently used (LRU) unused buffer.
  //  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //    if(b->refcnt == 0) {
  //      b->dev = dev;
  //      b->blockno = blockno;
  //      b->valid = 0;
  //      b->refcnt = 1;
  //      release(&bcache.lock);
  //      acquiresleep(&b->lock);
  //      return b;
  //    }
  //  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  int idx;
  if (!holdingsleep(&b->lock))
    panic("brelse");
//释放睡眠锁，告诉其他进程我不用了
  releasesleep(&b->lock);

  // change the lock - lab8-2
  //找到这个缓存所在的桶
  idx = HASH(b->blockno);
  //获取桶锁
  acquire(&bcache.locks[idx]);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    // lab8-2
    //    b->next->prev = b->prev;
    //    b->prev->next = b->next;
    //    b->next = bcache.head.next;
    //    b->prev = &bcache.head;
    //    bcache.head.next->prev = b;
    //    bcache.head.next = b;
    //计数为零记录最近使用时间
    b->timestamp = ticks;
  }
//释放锁
  release(&bcache.locks[idx]);

}

void
bpin(struct buf *b) {
  int idx = HASH(b->blockno);
  acquire(&bcache.locks[idx]);
  b->refcnt++;
  release(&bcache.locks[idx]);
}

void
bunpin(struct buf *b) {
  int idx = HASH(b->blockno);
  acquire(&bcache.locks[idx]);
  b->refcnt--;
  release(&bcache.locks[idx]);
}


