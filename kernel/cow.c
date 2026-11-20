#include "types.h"
#include "spinlock.h"
#include "param.h"
#include "memlayout.h"

// 每个物理页：引用计数 + 每页一把自旋锁
struct {
  uint8 ref_cnt;
  struct spinlock lock;
} cows[(PHYSTOP - KERNBASE) >> 12];

// lab6 内核启动调用，初始化全部cows数组的自旋锁
void
cow_init(void)
{
  int i;
  for(i = 0; i < (PHYSTOP - KERNBASE) >> 12; i++){
    initlock(&cows[i].lock, "cow_page");
  }
}

// 物理页pa（内核虚拟地址）引用计数 +1
void
increfcnt(uint64 pa)
{
  if(pa < KERNBASE)
    return;
  uint64 idx = (pa - KERNBASE) >> 12;
  acquire(&cows[idx].lock);
  cows[idx].ref_cnt ++;
  release(&cows[idx].lock);
}

// 引用计数‑1，返回递减之后的ref_cnt值
uint8
decrefcnt(uint64 pa)
{
  uint8 ret;
  if(pa < KERNBASE)
    return 0;
  uint64 idx = (pa - KERNBASE) >> 12;
  acquire(&cows[idx].lock);
  ret = -- cows[idx].ref_cnt;
  release(&cows[idx].lock);
  return ret;
}
