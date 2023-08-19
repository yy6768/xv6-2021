// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk bblock
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk bblock used by multiple processes.
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
  struct spinlock lock[NBUCKET];  

  struct buf buf[NBUF];
  // Hash table
  struct buf buckets[NBUCKET];
} bcache;


void
binit(void)
{
  struct buf *b;
  for(int i = 0; i < NBUCKET; ++ i){
    char lname[10];
    snprintf(lname, 10, "bcache%d", i);
    initlock(&bcache.lock[i], lname);
  }

  // Init hash table
  for(int i = 0; i < NBUCKET; ++ i){
    bcache.buckets[i].next = &bcache.buckets[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].next;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *victim = 0;
  int bid = blockno % NBUCKET;
  acquire(&bcache.lock[bid]);
  // Is the block already cached?
  for(b = bcache.buckets[bid].next; b != &bcache.buckets[bid]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->ts = ticks; // update ticks
      release(&bcache.lock[bid]);
      acquiresleep(&b->lock);
      return b;
    }
    if(b->refcnt == 0) { // trick case
      victim = b; 
    }
  }

  if(victim) {
      victim->dev = dev;
      victim->blockno = blockno;
      victim->valid = 0;
      victim->refcnt = 1;
      victim->ts = ticks;

      release(&bcache.lock[bid]);
      acquiresleep(&victim->lock);
      return victim;
  }  
  // Not cached.
  // Look up for the block which has the least timestamp and recycle it.
  uint mints = -1;
  struct buf *prev = 0, *vprev = 0; 
  int vid = -1;
  for(int i = 0; i < NBUCKET; i++) {
    if(i == bid) 
      continue;
    acquire(&bcache.lock[i]);
    for(prev = &bcache.buckets[i], b = bcache.buckets[i].next; b != &bcache.buckets[i]; prev = prev->next, b = b->next){
      if(b->refcnt == 0 && b->ts < mints) {
        mints = b->ts;
        victim = b;
        vprev = prev; 
        if(vid != -1 && vid != i) {
          release(&bcache.lock[vid]);
        }
        vid = i;
      }
    }
    if(vid != i) {
      release(&bcache.lock[i]); 
    }
  }
  if(victim) {
      vprev->next = victim->next;
      victim->next = bcache.buckets[bid].next;
      release(&bcache.lock[vid]);

      victim->dev = dev;
      victim->blockno = blockno;
      victim->valid = 0;
      victim->refcnt = 1;
      victim->ts = ticks;
      bcache.buckets[bid].next = victim;
      release(&bcache.lock[bid]);
      acquiresleep(&victim->lock);
      return victim;
  }
  panic("bget: no buffers");
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
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bid = b->blockno % NBUCKET;
  acquire(&bcache.lock[bid]);
  b->refcnt--;
  release(&bcache.lock[bid]);
}

void
bpin(struct buf *b) {
  int lockid = b->blockno % NBUCKET;
  acquire(&bcache.lock[lockid]);
  b->refcnt++;
  release(&bcache.lock[lockid]);
}

void
bunpin(struct buf *b) {
  int lockid = b->blockno % NBUCKET;
  acquire(&bcache.lock[lockid]);
  b->refcnt--;
  release(&bcache.lock[lockid]);
}
