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

#define NBUCKET 13
extern uint ticks;
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
  struct spinlock headlock[NBUCKET];
} bcache;

int
bid(uint dev, uint blockno) {
  return (dev*131 + blockno)%NBUCKET;
}

int bfindid(struct buf *b) {
  return bid(b->dev, b->blockno);
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKET; i++) {
    // Create linked list of buffers
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
    initlock(&bcache.headlock[i], "bcache_b");
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int id = bid(dev, blockno);

  acquire(&bcache.headlock[id]);
  // Is the block already cached?
  for(b = bcache.head[id].next; b != &bcache.head[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.headlock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock); // still can have deadlock. but it's fine for this lab
  //for (int i = 0; i < NBUCKET; i++) {
  int i = id; 
  do {
    if (i != id)
      acquire(&bcache.headlock[i]);
    int minticks = 1e9;
    for (b = bcache.head[i].next; b != &bcache.head[i]; b = b->next) {
      if(b->refcnt == 0) {
        minticks = minticks < b->timestamp ? minticks : b->timestamp;
      }
    }
    for (b = bcache.head[i].next; b != &bcache.head[i]; b = b->next) {
      if(b->refcnt == 0 && b->timestamp == minticks) {
        b->prev->next = b->next;
        b->next->prev = b->prev;

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        b->prev = &bcache.head[id];
        b->next = bcache.head[id].next;
        b->next->prev = b;
        b->prev->next = b;

        if (i != id)
          release(&bcache.headlock[i]);
        release(&bcache.lock);
        release(&bcache.headlock[id]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    if (i != id)
      release(&bcache.headlock[i]);
    i = (i+1)%NBUCKET;
  } while(i != id);
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

  int id = bfindid(b);
  acquire(&bcache.headlock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->timestamp = ticks;
  }
  release(&bcache.headlock[id]);
}

void
bpin(struct buf *b) {
  int id = bfindid(b);
  acquire(&bcache.headlock[id]);
  b->refcnt++;
  release(&bcache.headlock[id]);
}

void
bunpin(struct buf *b) {
  int id = bfindid(b);
  acquire(&bcache.headlock[id]);
  b->refcnt--;
  release(&bcache.headlock[id]);
}


