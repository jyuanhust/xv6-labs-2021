// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// struct {
//   struct spinlock lock;
//   struct run *freelist;
// } kmem;

struct {
  struct spinlock lock;
  struct run *freelist;
  int count;
} kmems[NCPU];

void
kinit()  // 这个只会运行一次
{
  for(int i = 0; i < NCPU; i++){
    initlock(&kmems[i].lock, "kmem");
    kmems[i].freelist = 0;
    kmems[i].count = 0;
  }

  // initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)  // 这个只会运行一次，即被上面的kinit调用
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
/* void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
} */

void
kfree(void *pa)  // 还会在其他地方被调用
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  int hart = cpuid();  // 基于之前的调用可知，cpuid是从0开始编号的

  acquire(&kmems[hart].lock);

  r->next = kmems[hart].freelist;
  kmems[hart].freelist = r;

  kmems[hart].count++;

  release(&kmems[hart].lock);

  pop_off();

  // acquire(&kmem.lock);
  // r->next = kmem.freelist;
  // kmem.freelist = r;
  // release(&kmem.lock);
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
/* void *
kalloc(void)  // 还会在其他地方被调用
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
} */


void *
kalloc(void)  // 还会在其他地方被调用
{
  // 需要处理本CPU上内存不够的问题
  

  struct run *r;

  push_off();

  int hart = cpuid();  // 基于之前的调用可知，cpuid是从0开始编号的

  // printf("hello %d \n", hart);

  acquire(&kmems[hart].lock);

  if(kmems[hart].count == 0){

    for(int i = 0; i < NCPU; i++){

      if(i == hart){
        continue;
      }

      acquire(&kmems[i].lock);

      if(kmems[i].count > 0){
        // 执行内存迁移

        // printf("kmems[i].count: %d\n", kmems[i].count);

        int count = kmems[i].count;

        if(count == 1){
          kmems[hart].freelist = kmems[i].freelist;
          kmems[i].freelist = 0;
          kmems[i].count = 0;
          kmems[hart].count = 1;

        }else{
          r = kmems[i].freelist;
          int j = 0;
          for(; j < count / 2 - 1; j++){
            r = r->next;
          }
          kmems[hart].freelist = r->next;
          r->next = 0;

          kmems[hart].count = kmems[i].count - (j+1);
          kmems[i].count = j + 1;

        }
        release(&kmems[i].lock);
        break;
      }else{
        release(&kmems[i].lock);
      }
    }
  }

  r = kmems[hart].freelist;
  if(r){
    kmems[hart].freelist = r->next;
    kmems[hart].count--;
  }
    
  release(&kmems[hart].lock);

  pop_off();


  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// void *
// kalloc(void)  // 还会在其他地方被调用
// {
//   struct run *r;

//   acquire(&kmems[0].lock);
//   r = kmems[0].freelist;
//   if(r)
//     kmems[0].freelist = r->next;
//   release(&kmems[0].lock);

//   if(r)
//     memset((char*)r, 5, PGSIZE); // fill with junk
//   return (void*)r;
// }

