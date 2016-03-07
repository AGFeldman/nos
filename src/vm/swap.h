#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_init(void);

struct swapt_entry {
    void * uaddr;
};

void swap_write_page(unsigned int, const char *);

void swap_read_page(unsigned int, char *);

#endif  // vm/swap.h
