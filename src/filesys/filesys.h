#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"

/*! Sectors of system file inodes. @{ */
#define FREE_MAP_SECTOR 0       /*!< Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /*!< Root directory file inode sector. */
/*! @} */

#define NUM_CACHE_BLOCKS 64

// Buffer cache block
struct bc_block {
    bool occupied;
    bool used;
    block_sector_t block_num;
    uint32_t data[BLOCK_SECTOR_SIZE];
};

/*! Block device that contains the file system. */
struct block *fs_device;

struct bc_block * bc_init(void);
void filesys_init(bool format);
void filesys_done(void);
bool filesys_create(const char *name, off_t initial_size);
struct file *filesys_open(const char *name);
bool filesys_remove(const char *name);

#endif /* filesys/filesys.h */

