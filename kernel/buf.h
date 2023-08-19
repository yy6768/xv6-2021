struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  uint ts;    // timestamp to judge LRU victim
  // struct buf *prev; // LRU cache list
  struct buf *next;    // Hash Table 
  uchar data[BSIZE];
};

