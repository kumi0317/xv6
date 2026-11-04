#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}
//用来调整进程堆的大小(n>0扩大，n<0缩小)
uint64
sys_sbrk(void)
{
  int addr;
  int n;
  if (argint(0, &n) < 0)
    return -1;
  struct proc *p = myproc();
  addr = p->sz;
  // 如果新va大于极限va或者把堆缩到负数了则不给调整大小返回原值
  if (addr + n >= MAXVA || addr + n <= 0)
    return addr;
  //直接更改sz的大小，如果是增加的话，页面之后再分配
  p->sz = addr + n;
  // if(growproc(n) < 0)
  //   return -1;
  //要缩堆了，调用uvmdealloc回收一下
  if (n < 0)
    uvmdealloc(p->pagetable, addr, p->sz);
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
