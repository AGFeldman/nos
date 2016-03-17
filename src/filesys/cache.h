#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "devices/timer.h"
#include <stdbool.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/synch.h"

#define NUM_CACHE_BLOCKS 64

// Buffer cache block
struct bc_block {
    bool occupied;
    bool accessed;
    bool dirty;
    block_sector_t block_num;
    char * data;
    struct rwlock rwlock;
};

void bc_init(void);

void bc_read_block_bytes(block_sector_t, void *, int, size_t);
void bc_read_block(block_sector_t, void *);
void bc_write_block_bytes(block_sector_t, void *, int, size_t, bool);
void bc_zero(block_sector_t);
void bc_write_block(block_sector_t, void *);

struct bc_block * find_block(block_sector_t);
struct bc_block * find_victim(void);
void evict_block(struct bc_block *);
struct bc_block * load_block(block_sector_t);
void write_back_block(struct bc_block *);

void flush_cache(void);

void advance_hand(void);

#endif /* filesys/cache.h */
