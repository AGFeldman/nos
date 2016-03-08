#include "vm/swap.h"
#include "vm/page.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include <stdio.h>


static struct block * swap_block;
static int sectors_needed_for_a_page;
static int num_swap_pages;
static struct swapt_entry * swapt;

void swap_init(void) {
    swap_block = block_get_role(BLOCK_SWAP);
    sectors_needed_for_a_page = PGSIZE / BLOCK_SECTOR_SIZE;
    if (PGSIZE % BLOCK_SECTOR_SIZE > 0) {
        sectors_needed_for_a_page++;
    }
    num_swap_pages = block_size(swap_block) / sectors_needed_for_a_page;
    swapt = malloc(num_swap_pages * sizeof(struct swapt_entry));
    int i;
    for (i = 0; i < num_swap_pages; i++) {
        swapt[i].uaddr = NULL;
    }
    // It seems like we want to use block_read() and block_write()
}

// Write a page from buffer into swap.
// buffer must be page-aligned.
void swap_write_page(int swap_page_number, const char *buffer) {
    printf("Thread %p swap_write_page 0\n", thread_current());
    bool already_held = filesys_lock_held();
    if (!already_held) {
        filesys_lock_acquire();
    }
    printf("Thread %p swap_write_page 1\n", thread_current());
    ASSERT(swap_page_number < num_swap_pages);
    ASSERT(pg_round_down(buffer) == buffer);
    block_sector_t sector = swap_page_number * sectors_needed_for_a_page;
    int i;
    printf("Thread %p swap_write_page 2\n", thread_current());
    for (i = 0; i < sectors_needed_for_a_page; i++) {
        // TODO(agf): This runs into a problem
        block_write(swap_block, sector, buffer);
        sector++;
        buffer += BLOCK_SECTOR_SIZE;
    }
    printf("Thread %p swap_write_page 3\n", thread_current());
    if (!already_held) {
        filesys_lock_release();
    }
}

// Read a page from swap into buffer.
// Buffer is rounded down to the nearest page boundary.
void swap_read_page(int swap_page_number, char *buffer) {
    bool already_held = filesys_lock_held();
    if (!already_held) {
        filesys_lock_acquire();
    }
    ASSERT(swap_page_number < num_swap_pages);
    buffer = pg_round_down(buffer);
    block_sector_t sector = swap_page_number * sectors_needed_for_a_page;
    int i;
    for (i = 0; i < sectors_needed_for_a_page; i++) {
        block_read(swap_block, sector, buffer);
        sector++;
        buffer += BLOCK_SECTOR_SIZE;
    }
    if (!already_held) {
        filesys_lock_release();
    }
}

// Returns a free swap slot number, or -1 if there are no free slots
int swap_find_free_slot(void) {
    bool already_held = filesys_lock_held();
    if (!already_held) {
        filesys_lock_acquire();
    }
    int i;
    for (i = 0; i < num_swap_pages; i++) {
        if (swapt[i].uaddr == NULL) {
            if (!already_held) {
                filesys_lock_release();
            }
            return i;
        }
    }
    if (!already_held) {
        filesys_lock_release();
    }
    return -1;
}

void mark_slot_unused(int swap_num) {
    bool already_held = filesys_lock_held();
    if (!already_held) {
        filesys_lock_acquire();
    }
    swapt[swap_num].uaddr = NULL;
    if (!already_held) {
        filesys_lock_release();
    }
}

// TODO(agf): Might need a synchronized function that finds a free slot and
// writes to it

// Write a frame table entry to an empty swap slot, and return the number of
// the swap slot.
// If there are no free swap slots, then do not perform any writing, and return
// -1.
int swap_dump_ft_entry(struct ft_entry * f) {
    printf("Thread %p swap_dump 0\n", thread_current());
    bool already_held = filesys_lock_held();
    if (!already_held) {
        filesys_lock_acquire();
    }
    printf("Thread %p swap_dump 1\n", thread_current());
    ASSERT(f->user_vaddr != NULL);
    ASSERT(f->trd != NULL);
    int swap_slot = swap_find_free_slot();
    printf("Thread %p swap_dump 2\n", thread_current());
    if (swap_slot == -1) {
        if (!already_held) {
            filesys_lock_release();
        }
        return swap_slot;
    }
    printf("Thread %p swap_dump 3\n", thread_current());
    swap_write_page(swap_slot, f->kernel_vaddr);
    printf("Thread %p swap_dump 4\n", thread_current());
    swapt[swap_slot].kaddr = f->kernel_vaddr;
    swapt[swap_slot].uaddr = f->user_vaddr;
    if (!already_held) {
        filesys_lock_release();
    }
    return swap_slot;
}
