#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include <stdio.h>


static struct block * swap_block;
static unsigned int sectors_needed_for_a_page;
static unsigned int num_swap_pages;
static struct swapt_entry * swapt;

void swap_init(void) {
    swap_block = block_get_role(BLOCK_SWAP);
    sectors_needed_for_a_page = PGSIZE / BLOCK_SECTOR_SIZE;
    if (PGSIZE % BLOCK_SECTOR_SIZE > 0) {
        sectors_needed_for_a_page++;
    }
    num_swap_pages = block_size(swap_block) / sectors_needed_for_a_page;
    swapt = malloc(num_swap_pages * sizeof(struct swapt_entry));
    unsigned int i;
    for (i = 0; i < num_swap_pages; i++) {
        swapt[i].uaddr = NULL;
    }
    // It seems like we want to use block_read() and block_write()
}

// Write a page from buffer into swap.
void swap_write_page(unsigned int swap_page_number, const char *buffer) {
    ASSERT(swap_page_number < num_swap_pages);
    block_sector_t sector = swap_page_number * sectors_needed_for_a_page;
    unsigned int i;
    for (i = 0; i < sectors_needed_for_a_page; i++) {
        block_write(swap_block, sector, buffer);
        sector++;
        buffer += BLOCK_SECTOR_SIZE;
    }
}

// Read a page from swap into buffer
void swap_read_page(unsigned int swap_page_number, char *buffer) {
    ASSERT(swap_page_number < num_swap_pages);
    block_sector_t sector = swap_page_number * sectors_needed_for_a_page;
    unsigned int i;
    for (i = 0; i < sectors_needed_for_a_page; i++) {
        block_read(swap_block, sector, buffer);
        sector++;
        buffer += BLOCK_SECTOR_SIZE;
    }
}
