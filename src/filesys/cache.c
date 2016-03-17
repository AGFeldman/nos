#include "filesys/filesys.h"
#include "filesys/cache.h"
#include "threads/thread.h"
#include "userprog/syscall.h"

// Buffer cache, an array of 64 bc_blocks,
// each of which holds one block (512 bytes) of memory
struct bc_block * bc;

// For clock eviction algorithm
struct bc_block * hand;

static void read_ahead(block_sector_t);
static thread_func read_ahead_helper;
static void write_behind(void);
static thread_func write_behind_helper;

void bc_init(void) {
    char * data_block = (char *) malloc(NUM_CACHE_BLOCKS * BLOCK_SECTOR_SIZE);
    bc = (struct bc_block *) malloc(NUM_CACHE_BLOCKS *
                                    sizeof(struct bc_block));
    size_t i;
    for (i = 0; i < NUM_CACHE_BLOCKS; i++) {
        struct bc_block * block = bc + i;
        block->occupied = false;
        block->data = data_block + i * BLOCK_SECTOR_SIZE;
    }
    hand = bc;
    write_behind();
}

void bc_read_block_bytes(block_sector_t sought_block_num, void * buffer,
                         int offset, size_t bytes) {
    ASSERT(offset + bytes <= BLOCK_SECTOR_SIZE);
    struct bc_block * found_block = find_block(sought_block_num);
    if (!found_block) {
        found_block = load_block(sought_block_num);
    }
    found_block->accessed = true;
    memcpy(buffer, found_block->data + offset, bytes);
}

// Reads sector SECTOR from BLOCK into BUFFER, which must have room for
// BLOCK_SECTOR_SIZE bytes. Uses filesystem cache; does not necessarily hit
// disk.
void bc_read_block(block_sector_t sought_block_num, void * buffer) {
    bc_read_block_bytes(sought_block_num, buffer, 0, BLOCK_SECTOR_SIZE);
}

void bc_write_block_bytes(block_sector_t sought_block_num, void * buffer,
                          int offset, size_t bytes, bool zero) {
    struct bc_block * found_block = find_block(sought_block_num);
    if (!found_block) {
        found_block = load_block(sought_block_num);
    }
    found_block->accessed = true;
    found_block->dirty = true;
    if (zero) {
        memset(found_block->data, 0, BLOCK_SECTOR_SIZE);
    }
    if (buffer == NULL) {
        ASSERT(zero && bytes == BLOCK_SECTOR_SIZE && offset == 0);
        return;
    }
    memcpy(found_block->data + offset, buffer, bytes);
}

void bc_zero(block_sector_t block_num) {
    bc_write_block_bytes(block_num, NULL, 0, BLOCK_SECTOR_SIZE, true);
}

// Write sector SECTOR to BLOCK from BUFFER, which must contain
// BLOCK_SECTOR_SIZE bytes. The write occurs in the filesystem cache; this does
// not necessarily hit disk.
void bc_write_block(block_sector_t sought_block_num, void * buffer) {
    bc_write_block_bytes(sought_block_num, buffer, 0, BLOCK_SECTOR_SIZE,
                         false);
}


void write_back_block(struct bc_block * write_block) {
    // This code block is from devices/block.c : block_write()
    check_sector(fs_device, write_block->block_num);
    fs_device->ops->write(fs_device->aux,
                          write_block->block_num, write_block->data);
    fs_device->write_cnt++;

    write_block->dirty = false;
}

// Find block sector in the cache. If not present, return NULL.
struct bc_block * find_block(block_sector_t sought_block_num) {
    size_t i;
    for (i = 0; i < NUM_CACHE_BLOCKS; i++) {
        struct bc_block * block = bc + i;
        if (block->occupied && block->block_num == sought_block_num) {
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
    check_sector(fs_device, sought_block_num);
    fs_device->ops->read(fs_device->aux, sought_block_num, dest->data);
    fs_device->read_cnt++;

    dest->occupied = true;
    dest->accessed = true;
    dest->dirty = false;
    dest->block_num = sought_block_num;
    rwlock_init(&dest->rwlock);
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
        // Sleep for 30 seconds
        timer_msleep(30 * 1000);
        flush_cache();
    }
}
