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
#define NBUCKETS 13


struct {
  //每个哈希队列一个lock
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.

  //struct buf head;
// 每个哈希队列一个linked list 以及一个 lock
  struct buf hashbucket[NBUCKETS];
  // 全局锁
  struct spinlock global_lock;

} bcache;

void
binit(void)
{
  struct buf *b;

//初始化每个哈希list的锁
  for (int i = 0;i < NBUCKETS;i++){
    initlock(&bcache.lock[i], "bcache_"+i);
    // Create linked list of buffers
    // 将每个哈希表首尾连接起来
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }

// 初始化全局锁
  initlock(&bcache.global_lock, "bcache_global");


  // Create linked list of buffers
  /*bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;*/
    // 将所有 buf 按哈希值插入到对应的哈希桶
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    int hash_index = b - bcache.buf; // 使用简单的计算方式给每个 `buf` 一个唯一的索引
    int bucket_index = hash_index % NBUCKETS;

    b->next = bcache.hashbucket[bucket_index].next;
    b->prev = &bcache.hashbucket[bucket_index];

    initsleeplock(&b->lock, "buffer");
    //bcache.head.next->prev = b;
    //bcache.head.next = b;
    bcache.hashbucket[bucket_index].next->prev = b;
    bcache.hashbucket[bucket_index].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint BlockNo2 = blockno % NBUCKETS;
  acquire(&bcache.lock[blockno%NBUCKETS]);

  // Is the block already cached?
  //观察当前的哈希列表是否有，是否cached？
  //注意要查找当前所有的bucket
  for(b = bcache.hashbucket[BlockNo2].next; b != &bcache.hashbucket[BlockNo2]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[BlockNo2]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 优先查看当前哈希桶有无buffer
  for(b = bcache.hashbucket[BlockNo2].prev; b != &bcache.hashbucket[BlockNo2]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[BlockNo2]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.lock[BlockNo2]);
  
  //获取全局锁
  acquire(&bcache.global_lock);
  //获取当前哈希桶的锁
  acquire(&bcache.lock[blockno%NBUCKETS]);

  //查看其他的哈希桶有无buffer
  for (int i = 0;i < NBUCKETS;i++){

    if (i == BlockNo2) continue;

    acquire(&bcache.lock[i]);

    for(b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // 采取头插法将当前的数据块放入哈希桶
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.hashbucket[BlockNo2].next;
        b->prev = &bcache.hashbucket[BlockNo2];
        bcache.hashbucket[BlockNo2].next->prev = b;
        bcache.hashbucket[BlockNo2].next = b;


        release(&bcache.lock[i]);
        release(&bcache.lock[BlockNo2]);
        release(&bcache.global_lock);
        acquiresleep(&b->lock);
        return b;
      }
    }

    release(&bcache.lock[i]);

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

  //获取当前buffer对应的blockno
  uint blockno = b->blockno;
  uint BlockNo2 = blockno % NBUCKETS;
  acquire(&bcache.lock[BlockNo2]);

// acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    //b->next = bcache.head.next;
    //b->prev = &bcache.head;
    //bcache.head.next->prev = b;
    //bcache.head.next = b;
    b->next = bcache.hashbucket[BlockNo2].next;
    b->prev = &bcache.hashbucket[BlockNo2];
    bcache.hashbucket[BlockNo2].next->prev = b;
    bcache.hashbucket[BlockNo2].next = b;
  }
  
  release(&bcache.lock[BlockNo2]);
}

void
bpin(struct buf *b) {
  //acquire(&bcache.lock);
  uint blockno = b->blockno;
  uint BlockNo2 = blockno % NBUCKETS;
  acquire(&bcache.lock[BlockNo2]);
  b->refcnt++;
  //release(&bcache.lock);
  release(&bcache.lock[BlockNo2]);

}

void
bunpin(struct buf *b) {
  //acquire(&bcache.lock);
  uint blockno = b->blockno;
  uint BlockNo2 = blockno % NBUCKETS;
  acquire(&bcache.lock[BlockNo2]);
  b->refcnt--;
  //release(&bcache.lock);
  release(&bcache.lock[BlockNo2]);

}


