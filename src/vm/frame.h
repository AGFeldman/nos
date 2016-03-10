#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "filesys/off_t.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/loader.h"

// Frame table entry
struct ft_entry {
    // Kernel virtual address of page that occupies this frame
    void * kernel_vaddr;
    // User virtual address (if any) of the page that occupies this frame
    void * user_vaddr;
    // Thread associated with this user address
    struct thread * trd;
    // Used to synchronize eviction and pinning
    struct lock lock;
    // Scratch space used during eviction
    bool acquired_during_eviction;
};

bool eviction_lock_held(void);

void eviction_lock_acquire(void);

void eviction_lock_release(void);

void frame_table_init(void);

struct ft_entry * ft_lookup(void *);

void ft_add_user_mapping(void *, void *, struct thread *);

void ft_init_entry(void *);

void ft_init_entries(void *, size_t);

void ft_deinit_entry(void *);

void ft_deinit_entries(void *, size_t);

void * frame_evict(size_t);

void pin_pages(unsigned char *, size_t);

void unpin_pages(unsigned char *, size_t);

void pin_pages_by_buffer(unsigned char *, off_t);

void unpin_pages_by_buffer(unsigned char *, off_t);

#endif  // vm/frame.h
