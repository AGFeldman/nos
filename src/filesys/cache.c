#include "filesys/cache.h"
#include "devices/block.h"
#include "threads/thread.h"

static void read_ahead(block_sector_t);
static thread_func read_ahead_helper;

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
