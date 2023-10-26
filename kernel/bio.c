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
struct{
  struct spinlock lock;
}Anobject;

#define NBUCKETS 13
struct {
    struct spinlock lock[NBUCKETS];
    struct buf buf[NBUF];
    struct buf hashbucket[NBUCKETS]; //每个哈希队列一个linked list及一个lock
} bcache;

uint hashcode(int x)
{
  return x%NBUCKETS;
}

void
binit(void)
{
  struct buf *b;
  for(int i=0;i<NBUCKETS;i++)
  {
    initlock(&bcache.lock[i], "bcache.bucket");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  int cur = 0;
  for(b = bcache.buf;b<bcache.buf+NBUF;b++)//内存块分布到哈希桶中
  {
    int key = hashcode(cur);
    b->refcnt = 0;
    //b->blockno = key;
    b->next = bcache.hashbucket[key].next;
    b->prev = &bcache.hashbucket[key];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[key].next->prev = b;
    bcache.hashbucket[key].next = b;
    cur++;
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int key = hashcode(blockno);
  acquire(&bcache.lock[key]);
  //recycle
  struct buf* walk;
  for(walk=bcache.hashbucket[key].next;walk != &bcache.hashbucket[key];walk = walk->next)
  {
    if(walk->dev == dev && walk->blockno == blockno)
    {
      walk->refcnt++;
      release(&bcache.lock[key]);
      acquiresleep(&walk->lock);
      return walk;
    }
  }
  //Not cached.
  for(walk=bcache.hashbucket[key].next;walk != &bcache.hashbucket[key];walk = walk->next)
  {
    if(walk->refcnt == 0)
    {
      walk->dev = dev;
      walk->blockno = blockno;
      walk->valid = 0;
      walk->refcnt = 1;
      release(&bcache.lock[key]);
      acquiresleep(&walk->lock);
      return walk;
    }
  }
  release(&bcache.lock[key]);
  //若此时还未返回则说明当前链表中没有一个从未被引用过的块，寻找其他链表。
  acquire(&Anobject.lock);
  for(int cur=key+1;hashcode(cur)!=key;cur++)
  {
    int hash_cur = hashcode(cur);
    acquire(&bcache.lock[hash_cur]);
    struct buf* b;
    for(b=bcache.hashbucket[hash_cur].next;b!=&bcache.hashbucket[hash_cur];b = b->next)
    {
      if(b->refcnt == 0)//还未被引用过
      {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.lock[hash_cur]);//将buf块从目前访问到的链表断开，加入所需要的链表。

        acquire(&bcache.lock[key]);
        b->next = bcache.hashbucket[key].next;
        b->prev = &bcache.hashbucket[key];
        bcache.hashbucket[key].next->prev = b;
        bcache.hashbucket[key].next = b;
        

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock[key]);
        acquiresleep(&b->lock);
        release(&Anobject.lock);      
        return b;
      }
      
    }
    release(&bcache.lock[hash_cur]);
  }
  release(&Anobject.lock);

  
  panic("bget: no buffers"); //找不到任何一个符合的块。
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
  int hash_code = hashcode(b->blockno);
  acquire(&bcache.lock[hash_code]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[hash_code].next;
    b->prev = &bcache.hashbucket[hash_code];
    bcache.hashbucket[hash_code].next->prev = b;
    bcache.hashbucket[hash_code].next = b;
  }
  
  release(&bcache.lock[hash_code]);
}

void
bpin(struct buf *b) {
  int cur = hashcode(b->blockno);
  acquire(&bcache.lock[cur]);
  b->refcnt++;
  release(&bcache.lock[cur]);
}

void
bunpin(struct buf *b) {
  int cur = hashcode(b->blockno);
  acquire(&bcache.lock[cur]);
  b->refcnt--;
  release(&bcache.lock[cur]);
}


