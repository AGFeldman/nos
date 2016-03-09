#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/loader.h"

// Frame table entry
// TODO(agf): For now, these fields are just *guesses* at what we might want
struct ft_entry {
    // Kernel virtual address of page that occupies this frame
    void * kernel_vaddr;
    // User virtual address (if any) of the page that occupies this frame
    void * user_vaddr;
    // Thread associated with this user address
    struct thread * trd;
    struct lock lock;
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

void * frame_evict(void);

#endif  // vm/frame.h
