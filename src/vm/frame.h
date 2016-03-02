#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/synch.h"
#include "threads/loader.h"

// An upper bound on the number of frames that can hold user pages
#define NUM_USER_FRAMES init_ram_pages / 2

// Frame table entry
// TODO(agf): For now, these fields are just *guesses* at what we might want
struct ft_entry {
    // Kernel virtual address of page that occupies this frame
    void * kernel_vaddr;
    // User virtual address (if any) of the page that occupies this frame
    void * user_vaddr;
    int age;
    struct lock lock;
};

void frame_table_init(void);

struct ft_entry * ft_lookup(void *);

void ft_add_user_mapping(void *, void *);

void ft_init_entry(void *);

void ft_init_entries(void *, size_t);

void ft_deinit_entry(void *);

void ft_deinit_entries(void *, size_t);

#endif  // vm/frame.h
