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
struct kmem{
  struct spinlock lock;
  struct run *freelist;
};
struct kmem kmems[NCPU];

char * lock_names[NCPU] = {
  "kmem_cpu_0",
  "kmem_cpu_1",
  "kmem_cpu_2",
  "kmem_cpu_3",
  "kmem_cpu_4",
  "kmem_cpu_5",
  "kmem_cpu_6",
  "kmem_cpu_7",
};

void
kinit()
{
  
  for(int i=0;i<NCPU;i++)
    initlock(&kmems[i].lock, lock_names[i]);
  freerange(end, (void*)PHYSTOP);

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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  
  push_off();
  int cpu_id = cpuid();//获取cpuid
  r = (struct run*) pa;
  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;//每个cpu独占一个freelist
  kmems[cpu_id].freelist = r;
  release(&(kmems[cpu_id].lock));

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{ 
  struct run *r;

  push_off();
  int cpu_id = cpuid();
  
  acquire(&(kmems[cpu_id].lock));
  
  if(!kmems[cpu_id].freelist) {
    
    for(int i = 0; i < NCPU; i++) {
      if(i == cpu_id) continue;
      acquire(&kmems[i].lock);
      struct run *rr = kmems[i].freelist;
      if(rr) {
        kmems[i].freelist = rr->next;
        rr->next = kmems[cpu_id].freelist;
        kmems[cpu_id].freelist = rr;
        release(&kmems[i].lock); 
        break;
        }
      release(&kmems[i].lock);
    }
  }
    r = kmems[cpu_id].freelist;
    if(r)
      kmems[cpu_id].freelist = r->next;
    release(&(kmems[cpu_id].lock));
    
    pop_off();

    if(r)
      memset((char*)r, 5, PGSIZE); // fill with junk

    return (void*) r;//所有cpu都没有空闲块,则返回空指针
  
}
