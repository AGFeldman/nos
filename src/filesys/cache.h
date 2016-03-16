#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include <stdbool.h>
#include <string.h>
#include "threads/malloc.h"

#define NUM_CACHE_BLOCKS 64

// Buffer cache block
struct bc_block {
    bool occupied;
    bool accessed;
    bool dirty;
    block_sector_t block_num;
    void * data;
};

void bc_init(void);

void read_block(block_sector_t, void *);
void write_block(block_sector_t, void *);

struct bc_block * find_block(block_sector_t);
struct bc_block * find_victim(void);
void evict_block(struct bc_block *);
struct bc_block * load_block(block_sector_t);
void write_back_block(struct bc_block *);

void flush_cache(void);

void advance_hand(void);

#endif /* filesys/cache.h */
