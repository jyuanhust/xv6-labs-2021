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

#define PA2PID(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)
struct {
  struct spinlock lock;
  // 物理地址最大是 PHYSTOP
  int refcount[PA2PID(PHYSTOP) + 8]; 
} pagerefcount;



int P(int id) 
{
  acquire(&pagerefcount.lock);
  if(id < 0 || id > PA2PID(PHYSTOP)) panic("P: id");
  int t = --pagerefcount.refcount[id];
  release(&pagerefcount.lock);
  return t;
}

int V(int id)
{
  acquire(&pagerefcount.lock);
  if(id < 0 || id > PA2PID(PHYSTOP)) panic("V: id");
  int t = ++pagerefcount.refcount[id];
  release(&pagerefcount.lock);
  return t;
}

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  
  initlock(&kmem.lock, "kmem");
  
  freerange(end, (void*)PHYSTOP);
  // memset(pgcount, 0, (int)sizeof(pgcount) * sizeof(int));
  /* for(int i = 0; i < 557056; i++){
    pgcount[i] = 0;
  } */

  printf("kinit\n");

  initlock(&pagerefcount.lock, "pagerefcount");
}

void
freerange(void *pa_start, void *pa_end)
{
  // printf("max page: %p %d\n", PGROUNDDOWN((uint64)pa_end), PGROUNDDOWN((uint64)pa_end)/4096);
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);

  for(int i = 0; i < sizeof pagerefcount.refcount / sizeof(int); ++ i)
    pagerefcount.refcount[i] = 1;

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

  // cow
  /* int index = (uint64)pa / 4096;
  pgcount[index] -= 1;

  if(pgcount[index] > 0){
    return;
  } */

  int refcount = P(PA2PID(pa));
  if(refcount < 0) {
    printf("refcount : %d\n", refcount);
    panic("kfree");
  }
  if(refcount > 0) return; 
  
  // cow

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
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    pagerefcount.refcount[PA2PID(r)] = 1;
  }
    

  
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  // cow
  /* int index = (uint64)r / 4096;
  pgcount[index] += 1; */
  
  return (void*)r;
}

int handleCOWfault(pagetable_t pagetable, uint64 va) 
{
  // 1.判断 va 合法性
  if(va >= MAXVA) 
    return -1;
  pte_t *pte = walk(pagetable, va, 0);
  if(pte == 0 || (*pte & (PTE_V)) == 0 || (*pte & PTE_U) == 0) 
    return -1;
  if(*pte & PTE_W) return 0;
  // 没有写权限如果不是 COW 页，就代表出错了
  if((*pte & PTE_COW) == 0) 
    return -1;

  // 2. 添加新的映射
  uint64 pa = PTE2PA(*pte);
  void *newPa = kalloc();
  if(newPa == 0) {
    printf("handleCOWfault kalloc out of memory \n");
    return -1;
  }
  *pte = (PTE_FLAGS(*pte) & ~PTE_COW) | PTE_W | PA2PTE(newPa);
  memmove((char*)newPa, (char*)pa, PGSIZE);
  // 3.减少 pa 映射
  kfree((void*)pa);
  return 0;
}