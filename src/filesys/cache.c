#include "filesys/cache.h"

// Buffer cache, an array of 64 bc_blocks,
// each of which holds one block (512 bytes) of memory
struct bc_block * bc;

// For clock eviction algorithm
struct bc_block * hand;

struct block * filesys_block;

// TODO: call from thread
void bc_init(void) {
    void * data_block = malloc(NUM_CACHE_BLOCKS * BLOCK_SECTOR_SIZE);
    bc = (struct bc_block *) malloc(NUM_CACHE_BLOCKS * sizeof(struct bc_block));
    size_t i;
    for (i = 0; i < NUM_CACHE_BLOCKS; i++) {
        struct bc_block * block = bc + i;
        block->occupied = false;
        block->data = (void *) ((char *) data_block + i * BLOCK_SECTOR_SIZE);
    }
    hand = bc;
    filesys_block = block_get_role(BLOCK_FILESYS);
}

void read_block(block_sector_t sought_block_num, void * buffer) {
    struct bc_block * found_block = find_block(sought_block_num);
    if (!found_block) {
        found_block = load_block(sought_block_num);
    }
    found_block->accessed = true;
    memcpy(found_block->data, buffer, BLOCK_SECTOR_SIZE);
}

void write_block(block_sector_t sought_block_num, void * buffer) {
    struct bc_block * found_block = find_block(sought_block_num);
    if (!found_block) {
        found_block = load_block(sought_block_num);
    }
    found_block->accessed = true;
    found_block->dirty = true;
    memcpy(buffer, found_block->data, BLOCK_SECTOR_SIZE);
}


void write_back_block(struct bc_block * write_block) {
    check_sector(filesys_block, write_block->block_num);
    filesys_block->ops->write(filesys_block->aux, write_block->block_num, write_block->data);
    filesys_block->write_cnt++;
    write_block->dirty = false;
}

// Find block sector in the cache. If not present, return NULL.
struct bc_block * find_block(block_sector_t sought_block_num) {
    size_t i;
    for (i = 0; i < NUM_CACHE_BLOCKS; i++) {
        struct bc_block * block = bc + i;
        if (block->block_num == sought_block_num) {
            return block;
        }
    }
    return NULL;
}

struct bc_block * load_block(block_sector_t sought_block_num) {
    struct bc_block * dest;
    if (!hand->occupied) {
        dest = hand;
        advance_hand();
    }
    else {
        dest = find_victim();
        evict_block(dest);
    }
    check_sector(filesys_block, sought_block_num);
    filesys_block->ops->read(filesys_block->aux, sought_block_num, dest->data);
    filesys_block->read_cnt++;
    dest->occupied = true;
    dest->accessed = true;
    dest->dirty = false;
    dest->block_num = sought_block_num;
    return dest;
}

// Find a block to evict using the clock algorithm.
struct bc_block * find_victim(void) {
    while (hand->accessed) {
        hand->accessed = false;
        advance_hand();
    }
    return hand;
}

void evict_block(struct bc_block *evictee) {
    write_back_block(evictee);
    evictee->occupied = false;
}

void flush_cache(void) {
    size_t i;
    for (i = 0; i < NUM_CACHE_BLOCKS; i++) {
        struct bc_block * block = bc + i;
        write_back_block(block);
        block->dirty = false;
    }

}

void advance_hand(void) {
    hand++;
    if (hand >= bc + NUM_CACHE_BLOCKS) {
        hand = bc;
    }
}
