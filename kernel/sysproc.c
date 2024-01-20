#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
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
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
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

  if (argint(0, &pid) < 0)
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


uint64
sys_trace(void)
{
  int x = 0;
  if (argint(0, &x) < 0)
    return -1;
  myproc()->trace = x;
  // printf("%d\n", x);

  return 0;
}



uint64
sys_sysinfo(void)
{
  // 这部分是在内核中进行操作，使用的是内核的地址空间
  // 使用使用用户空间中的指针能行么？

  struct sysinfo info;
  
  info.freemem = countfreemem();
  info.nproc = countunusedproc();

  struct proc* p = myproc();


  uint64 infoptr;
  if (argaddr(0, &infoptr) < 0)
    return -1;

  // 那传入的这个参数是用户空间的虚拟地址么？

  if (copyout(p->pagetable, infoptr, (char *)&info, sizeof(struct sysinfo))) {
    return -1;
  }



  // 理由在上面
  // p->freemem = 0;
  // p->nproc = 0;

  // 怎么使用kalloc.c里面的东西呢？


  return 0;
}
