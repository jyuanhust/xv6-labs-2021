#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } 
  else if(r_scause() == 13 || r_scause() == 15) {
    // printf("hello\n");
    uint64 va = r_stval();
    if(handleCOWfault(p->pagetable, va) == -1){
      p->killed = 1;
    }
      
  }
  
  else if((which_dev = devintr()) != 0){
    // ok
  } 
  /* else if(r_scause() == 0x000000000000000f){
    // 新添加的
    uint64 pa, va;
    pte_t* pte;
    va = r_stval();
    va = PGROUNDDOWN(va); // 这里PGROUNDDOWN或者没有都是没问题的，因为pte不涉及低12位

    if ((pte = walk(p->pagetable, va, 0)) == 0)
      panic("usertrap: pte should exist");

    if ((*pte & PTE_V) == 0)
      panic("usertrap: page not present");

    // if(*pte & PTE_W){
    //   printf("can be writed\n"); // 这里是正常的
    // }

    pa = PTE2PA(*pte);

    // printf("pte: %p\t pa: %p\n", *pte, pa); // 这两个是有发生变化的，说明这里的执行没有问题

    // printf("%p\t", *pte);

    char* mem;
    uint flags;
    flags = PTE_FLAGS(*pte);

    if ((mem = kalloc()) == 0)
      panic("usertrap: page is short for use");

    *pte = PA2PTE(mem) | flags | PTE_W;

    // printf("%p\n", *pte);

    memmove(mem, (char*)pa, PGSIZE);

    extern int *pgcount;
    int index = pa / 4096;
    pgcount[index] -= 1;
  } */
  
  else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;

    // 上面是原始代码
    // 现在是假设进入到这里都是因为发生页错误，stval寄存器保存发生错误的虚拟地址
    // 这种假设是合理的，因此未修改的代码不会进入这里，而修改之后只会发生
    /* uint64 pa, va;
    pte_t* pte;
    va = r_stval();
    va = PGROUNDDOWN(va);

    if((pte = walk(p->pagetable, va, 0)) == 0)
      panic("usertrap: pte should exist");
    
    if((*pte & PTE_V) == 0)
      panic("usertrap: page not present");

    // if(*pte & PTE_W){
    //   printf("can be writed\n"); // 这里是正常的
    // }

    pa = PTE2PA(*pte);

    // printf("pte: %p\t pa: %p\n", *pte, pa); // 这两个是有发生变化的，说明这里的执行没有问题

    // printf("%p\t", *pte);

    char *mem;
    uint flags;
    flags = PTE_FLAGS(*pte);
    
    if((mem = kalloc()) == 0)
      panic("usertrap: page is short for use");

    *pte = PA2PTE(mem) | flags | PTE_W;

    // printf("%p\n", *pte);

    memmove(mem, (char*)pa, PGSIZE); */
    // 这里会不会发生问题，出现在读取pa的时候，不对，读取不会出现问题的
    
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

