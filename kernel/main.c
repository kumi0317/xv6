#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
// largefile:kernel/fs.h（修改MAXFILE宏以支持二级间接块）、kernel/fs.c（修改bmap增加二级间接块映射逻辑，修改itrunc增加二级间接块释放逻辑）
// symbolic link:kernel/stat.h(新增T_SYMLINK文件类型)、kernel/fcntl.h(新增O_NOFOLLOW不追踪标志)、kernel/fs.c(在bmap中增加二级间接块映射逻辑并在itrunc中增加二级间接块的释放逻辑)、kernel/sysfile.c(新增sys_symlink和follow_symlink函数并在sys_open中增加软链接识别与追踪逻辑)、kernel/syscall.h(分配新系统调用号)、kernel/syscall.c(注册新系统调用)、user/usys.pl(生成用户态汇编存根)、user/user.h(声明symlink函数)以及Makefile(将symlinktest测试程序加入编译列表)。
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode cache
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
