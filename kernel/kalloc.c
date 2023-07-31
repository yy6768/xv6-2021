// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"


#define PA2PID(pa) ((uint64)(pa) - KERNBASE) / PGSIZE

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

struct 
{
  struct spinlock lock;
  int count[PA2PID(PHYSTOP) + 1];
} refcount;

inline void
acquire_refcount()
{
  acquire(&refcount.lock);  
}

inline void 
release_refcount()
{
  release(&refcount.lock);
}

inline void
incr_refcount(uint64 p, uint64 incr)
{
  refcount.count[PA2PID(p)] += incr;
}

inline int
get_refcount(uint64 p) 
{
  return refcount.count[PA2PID(p)];
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcount.lock, "refcount");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  // for(int i = 0; i < sizeof refcount.count / sizeof(int); ++ i)
  //   refcount.count[i] = 1;
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    refcount.count[PA2PID(p)] = 1;
    kfree(p);
  }
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

  acquire_refcount();
  refcount.count[PA2PID(pa)] --;
  if(refcount.count[PA2PID(pa)] > 0) { // don't need to free
    release_refcount();
    return;
  }
    
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  release_refcount();
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
  if(r)
    kmem.freelist = r->next;

  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    refcount.count[PA2PID(r)] = 1;
  }
  
  return (void*)r;
}
