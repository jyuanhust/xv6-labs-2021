struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};  // 这整个结构都会保存到磁盘中么？还是仅仅data保存到磁盘中呢？
// 根据fs.h中IPB等几个宏的计算，说明磁盘上保存的一个块是BSIZE个字节
// 因此这里只有data字段才会保存到磁盘上

