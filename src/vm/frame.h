#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/synch.h"
#include "threads/loader.h"

// An upper bound on the number of frames that can hold user pages
#define NUM_USER_FRAMES init_ram_pages / 2

// Frame table entry
// TODO(agf): For now, these fields are just *guesses* at what we might want
struct ft_entry {
    // User virtual address of page that occupies this frame
    void * vaddr;
    int age;
    struct lock lock;
};

void frame_table_init(void);

#endif  // vm/frame.h
