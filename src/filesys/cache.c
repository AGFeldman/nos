#include "filesys/cache.h"
#include "threads/thread.h"

// Buffer cache, an array of 64 bc_blocks,
// each of which holds one block (512 bytes) of memory
struct bc_block * bc;

// For clock eviction algorithm
struct bc_block * hand;

struct block * filesys_block;

static void read_ahead(block_sector_t);
static thread_func read_ahead_helper;
static void write_behind(void);
static thread_func write_behind_helper;

void bc_init(void) {
    void * data_block = malloc(NUM_CACHE_BLOCKS * BLOCK_SECTOR_SIZE);
    bc = (struct bc_block *) malloc(NUM_CACHE_BLOCKS *
                                    sizeof(struct bc_block));
    size_t i;
    for (i = 0; i < NUM_CACHE_BLOCKS; i++) {
        struct bc_block * block = bc + i;
        block->occupied = false;
        block->data = (void *) ((char *) data_block + i * BLOCK_SECTOR_SIZE);
    }
    hand = bc;
    filesys_block = block_get_role(BLOCK_FILESYS);
    if (filesys_block == NULL) {
        PANIC("bc_init: No file system device found, can't init file system.");
    }
    write_behind();
}

// Reads sector SECTOR from BLOCK into BUFFER, which must have room for
// BLOCK_SECTOR_SIZE bytes. Uses filesystem cache; does not necessarily hit
// disk.
void bc_read_block(block_sector_t sought_block_num, void * buffer) {
    ASSERT(filesys_block != NULL);
    check_sector(filesys_block, sought_block_num);
    filesys_block->ops->read(filesys_block->aux, sought_block_num, buffer);
    filesys_block->read_cnt++;

    // struct bc_block * found_block = find_block(sought_block_num);
    // if (!found_block) {
    //     found_block = load_block(sought_block_num);
    // }
    // found_block->accessed = true;
    // memcpy(found_block->data, buffer, BLOCK_SECTOR_SIZE);
}

// Write sector SECTOR to BLOCK from BUFFER, which must contain
// BLOCK_SECTOR_SIZE bytes. The write occurs in the filesystem cache; this does
// not necessarily hit disk.
void bc_write_block(block_sector_t sought_block_num, void * buffer) {
    ASSERT(filesys_block != NULL);
    check_sector(filesys_block, sought_block_num);
    filesys_block->ops->write(filesys_block->aux, sought_block_num, buffer);
    filesys_block->write_cnt++;

    // struct bc_block * found_block = find_block(sought_block_num);
    // if (!found_block) {
    //     // TODO(agf): We should try to avoid reading from disk here
    //     found_block = load_block(sought_block_num);
    // }
    // found_block->accessed = true;
    // found_block->dirty = true;
    // memcpy(buffer, found_block->data, BLOCK_SECTOR_SIZE);
}


void write_back_block(struct bc_block * write_block) {
    // This code block is from devices/block.c : block_write()
    check_sector(filesys_block, write_block->block_num);
    filesys_block->ops->write(filesys_block->aux,
                              write_block->block_num, write_block->data);
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

    // This code block is from devices/block.c : block_read()
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
        if (block->occupied && block->dirty) {
            write_back_block(block);
        }
    }
}

void advance_hand(void) {
    hand++;
    if (hand >= bc + NUM_CACHE_BLOCKS) {
        hand = bc;
    }
}

// Creates a background thread to fetch the block indicated by |sector| into
// the filesystem cache.
static void read_ahead(block_sector_t sector) {
    tid_t tid = thread_create("read_ahead", PRI_DEFAULT, read_ahead_helper,
                              (void *) sector);
    ASSERT(tid != TID_ERROR);
}

static void read_ahead_helper(void * aux) {
    block_sector_t sector = (block_sector_t) aux;
    // TODO(agf): Fetch into filesystem cache
}

static void write_behind(void) {
    tid_t tid = thread_create("write_behind", PRI_DEFAULT, write_behind_helper,
                              NULL);
    ASSERT(tid != TID_ERROR);
}

static void write_behind_helper(void * aux UNUSED) {
    while(true) {
        // TODO: no idea what a reasonable sleep time is
        timer_sleep(1000);
        flush_cache();
    }
}
