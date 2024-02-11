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

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// #define ARRLEN (PHYSTOP - PGROUNDUP(end) + 1) / PGSIZE 

int *pagerefcount;
int pagerefcountLen;
char* newEnd;  // 用于分配的内存的开始地址

struct spinlock pagerefcountLock;

int pa2index(void *pa){
  // 这里要不要使用PGROUNDUP呢？在本文件中的调用，传入的pa必定是PGSIZE的整数倍，所以先暂时不管
  int index = ((uint64)pa - (uint64)newEnd) / PGSIZE;
  return index;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // freerange(end, (void*)PHYSTOP);

  initlock(&pagerefcountLock, "pagerefcountLock");
  int pageNum = (PHYSTOP - PGROUNDUP((uint64)end) + 1) / PGSIZE ;
  pagerefcount = (int*)end;
  newEnd = (char*)PGROUNDUP((uint64)end);
  int count = 0;
  while(((uint64)newEnd -(uint64)pagerefcount) / sizeof(int) < pageNum - count){
    newEnd += PGSIZE;
    count++;
  }

  pagerefcountLen = ((uint64)newEnd -(uint64)pagerefcount) / sizeof(int);
  // printf("pagerefcountLen: %d\n", pagerefcountLen);

  for(int i = 0; i < pagerefcountLen; i++){
    pagerefcount[i] = 1;
  }

  freerange(newEnd, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
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
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&pagerefcountLock);
  int index = pa2index(pa);
  if(--pagerefcount[index] > 0){
    release(&pagerefcountLock);
    return;
  }else if(pagerefcount[index] < 0){
    panic("kfree: error");
  }
  release(&pagerefcountLock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  // printf("kalloc\n");
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  else{
    printf("error\n");
  }

  int index = pa2index(r);
  pagerefcount[index] = 1;
  
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
