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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

#define BUCKETSIZE 10  // 每个桶最多可以缓存多少个块
#define NBUCKET 13  // 桶的数量

struct bucket{
  struct spinlock lock;
  struct buf buf[BUCKETSIZE];
};

struct 
{
  struct spinlock lock;
  struct bucket buckets[NBUCKET];
}bcache2;


// void
// binit(void)
// {
//   struct buf *b;

//   initlock(&bcache.lock, "bcache");

//   // Create linked list of buffers
//   bcache.head.prev = &bcache.head;  // bcache.head.prev 貌似从来没有进行更改？ 有的，第一次循环的bcache.head.next->prev就是
//   bcache.head.next = &bcache.head;
//   for(b = bcache.buf; b < bcache.buf+NBUF; b++){
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     initsleeplock(&b->lock, "buffer");
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
// }

void
binit(void)  // binit只会被调用一次，在cpu0的main中
{
  struct buf *b;

  initlock(&bcache2.lock, "bcache");

  for(int i = 0; i < NBUCKET; i++){
    initlock(&bcache2.buckets[i].lock, "bcache");
    for(b = bcache2.buckets[i].buf; b < bcache2.buckets[i].buf + BUCKETSIZE; b++){
      initsleeplock(&b->lock, "bcache");
    }
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// static struct buf*
// bget(uint dev, uint blockno)
// {
//   struct buf *b;

//   acquire(&bcache.lock);

//   // Is the block already cached?
//   for(b = bcache.head.next; b != &bcache.head; b = b->next){
//     if(b->dev == dev && b->blockno == blockno){
//       b->refcnt++;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }

//   // Not cached.
//   // Recycle the least recently used (LRU) unused buffer.
//   for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//     if(b->refcnt == 0) {
//       b->dev = dev;
//       b->blockno = blockno;
//       b->valid = 0;
//       b->refcnt = 1;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }
//   panic("bget: no buffers");
// }


// 因为bread会被外部调用，而bread会调用bget，相当于bget也被调用了
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // acquire(&bcache2.lock);

  int hash = blockno % NBUCKET;

  acquire(&bcache2.buckets[hash].lock);

  for (b = bcache2.buckets[hash].buf; b < bcache2.buckets[hash].buf + BUCKETSIZE; b++) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache2.buckets[hash].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  uint minTimestamp = -1;
  int index = 0;

  // 块没有被缓存，检查桶中空闲块的timestamp，取出最小的

  for(int i = 0; i < BUCKETSIZE; i++){
    b = &bcache2.buckets[hash].buf[i];
    if(b->refcnt == 0) {
      if(minTimestamp < 0){
        index = i;
        minTimestamp = b->timestamp;
      }else if(b->timestamp < minTimestamp){
        minTimestamp = b->timestamp;
        index = i;
      }
    }
  }

  if(minTimestamp == -1){
    panic("bget: no buffers");
  }

  b = &bcache2.buckets[hash].buf[index];
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  release(&bcache2.buckets[hash].lock);
  acquiresleep(&b->lock);
  return b;
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
// void
// brelse(struct buf *b)
// {
//   if(!holdingsleep(&b->lock))
//     panic("brelse");

//   releasesleep(&b->lock);

//   acquire(&bcache.lock);
//   b->refcnt--;
//   if (b->refcnt == 0) {
//     // no one is waiting for it.
//     b->next->prev = b->prev;
//     b->prev->next = b->next;
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
  
//   release(&bcache.lock);
// }

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);  // 为什么这里有一个释放睡眠锁
  // 如果有其他进程在等待着这个块的话，该进程就可以被唤醒了

  int hash = b->blockno % NBUCKET;

  acquire(&bcache2.buckets[hash].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    acquire(&tickslock);
    b->timestamp = ticks;
    release(&tickslock);
  }

  release(&bcache2.buckets[hash].lock);
}

// void
// bpin(struct buf *b) {
//   acquire(&bcache.lock);
//   b->refcnt++;
//   release(&bcache.lock);
// }

// void
// bunpin(struct buf *b) {
//   acquire(&bcache.lock);
//   b->refcnt--;
//   release(&bcache.lock);
// }


void
bpin(struct buf *b) {
  int hash = b->blockno % NBUCKET;

  acquire(&bcache2.buckets[hash].lock);

  b->refcnt++;

  release(&bcache2.buckets[hash].lock);
}

void
bunpin(struct buf *b) {
  int hash = b->blockno % NBUCKET;

  acquire(&bcache2.buckets[hash].lock);

  b->refcnt--;
  release(&bcache2.buckets[hash].lock);
}
