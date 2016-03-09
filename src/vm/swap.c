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

static struct lock swap_block_lock;

static void swap_lock_acquire(void) {
    printf("AGF: Thread %p trying to acquire swap lock\n", thread_current());
    lock_acquire(&swap_block_lock);
    printf("AGF: Thread %p acquired swap lock\n", thread_current());
}

static void swap_lock_release(void) {
    printf("AGF: Thread %p trying to release swap lock\n", thread_current());
    lock_release(&swap_block_lock);
    printf("AGF: Thread %p released swap lock\n", thread_current());
}

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
    lock_init(&swap_block_lock);
}

// Write a page from buffer into swap.
// buffer must be page-aligned.
void swap_write_page(int swap_page_number, const char *buffer) {
    printf("AGF: Thread %p beginning swap_write\n", thread_current());
    swap_lock_acquire();
    bool already_held = filesys_lock_held();
    ASSERT(!already_held);
    if (!already_held) {
        filesys_lock_acquire();
    }
    ASSERT(swap_page_number < num_swap_pages);
    ASSERT(pg_round_down(buffer) == buffer);
    block_sector_t sector = swap_page_number * sectors_needed_for_a_page;
    int i;
    for (i = 0; i < sectors_needed_for_a_page; i++) {
        block_write(swap_block, sector, buffer);
        sector++;
        buffer += BLOCK_SECTOR_SIZE;
    }
    if (!already_held) {
        filesys_lock_release();
    }
    swap_lock_release();
    printf("AGF: Thread %p ending swap_write\n", thread_current());
}

// Read a page from swap into buffer.
// Buffer is rounded down to the nearest page boundary.
void swap_read_page(int swap_page_number, char *buffer) {
    printf("AGF: Thread %p beginning swap_read\n", thread_current());
    swap_lock_acquire();
    bool already_held = filesys_lock_held();
    ASSERT(!already_held);
    if (!already_held) {
        filesys_lock_acquire();
    }
    ASSERT(swap_page_number >= 0);
    ASSERT(swap_page_number < num_swap_pages);
    buffer = pg_round_down(buffer);
    block_sector_t sector = swap_page_number * sectors_needed_for_a_page;
    int i;
    for (i = 0; i < sectors_needed_for_a_page; i++) {
        printf("AGF: Thread %p swap_read iteration %d\n", thread_current(), i);
        block_read(swap_block, sector, buffer);
        sector++;
        buffer += BLOCK_SECTOR_SIZE;
    }
    if (!already_held) {
        filesys_lock_release();
    }
    swap_lock_release();
    printf("AGF: Thread %p ending swap_read\n", thread_current());
}

// Returns a free swap slot number, or -1 if there are no free slots
int swap_find_free_slot(void) {
    int i;
    for (i = 0; i < num_swap_pages; i++) {
        if (swapt[i].uaddr == NULL) {
            return i;
        }
    }
    return -1;
}

void mark_slot_unused(int swap_num) {
    swapt[swap_num].uaddr = NULL;
}

// TODO(agf): Might need a synchronized function that finds a free slot and
// writes to it

// Write a frame table entry to an empty swap slot, and return the number of
// the swap slot.
// If there are no free swap slots, then do not perform any writing, and return
// -1.
int swap_dump_ft_entry(struct ft_entry * f) {
    ASSERT(f->user_vaddr != NULL);
    ASSERT(f->trd != NULL);
    int swap_slot = swap_find_free_slot();
    if (swap_slot == -1) {
        return swap_slot;
    }
    // There were problems with swap_write_page(swap_slot, f->user_vaddr)
    swap_write_page(swap_slot, f->kernel_vaddr);
    swapt[swap_slot].kaddr = f->kernel_vaddr;
    swapt[swap_slot].uaddr = f->user_vaddr;
    return swap_slot;
}
