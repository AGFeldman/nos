#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/frame.h"

void swap_init(void);

struct swapt_entry {
    // Kernel virtual address
    void * kaddr;
    // User virtual address
    void * uaddr;
};

void swap_write_page(int, const char *);

void swap_read_page(int, char *);

int swap_find_free_slot(void);

void mark_slot_unused(int);

int swap_dump_ft_entry(struct ft_entry *);

#endif  // vm/swap.h
