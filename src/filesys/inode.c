#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/*! Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

// Number of blocks directly accessible from an on-disk inode
#define NUM_DIRECT 123
// Number of blocks indirectly accessible from an on-disk inode
#define NUM_INDIRECT 1
// Number of blocks doubly-indirectly accessible from an on-disk inode
#define NUM_DINDIRECT 1

/*! On-disk inode.
    Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
    block_sector_t start;               /*!< First data sector. */
    off_t length;                       /*!< File size in bytes. */
    unsigned magic;                     /*!< Magic number. */
    // Array of sectors.
    // For simplicity, we should have blocks[0] == start.
    block_sector_t blocks[NUM_DIRECT + NUM_INDIRECT + NUM_DINDIRECT];
};

/*! Returns the number of sectors to allocate for an inode SIZE
    bytes long. */
static inline size_t bytes_to_sectors(off_t size) {
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/*! In-memory inode. */
struct inode {
    struct list_elem elem;              /*!< Element in inode list. */
    // This is how we will access the inode's content
    block_sector_t sector;              /*!< Sector number of disk location. */
    int open_cnt;                       /*!< Number of openers. */
    bool removed;                       /*!< True if deleted, false otherwise. */
    int deny_write_cnt;                 /*!< 0: writes ok, >0: deny writes. */
    // TODO(agf): Synchronize access to this once we allow file length to change
    off_t length;
    // Used to enforce the restriction that at most one process may extend
    // a file at one time.
    struct lock extend_lock;
};

// A structure for iterating through the blocks indexed by an inode.
struct inode_iter {
    struct inode_disk * idisk;
    struct inode * inode;
    block_sector_t * bottom;
    int bottom_offset;
    block_sector_t * first_level;
    int first_level_offset;
    block_sector_t * second_level;
    int second_level_offset;
    block_sector_t sector_num;
    block_sector_t value;
    block_sector_t num_allocated_sectors;
    block_sector_t max_num_allocated_sectors;
};

static void extend(block_sector_t existing_sec, char * existing,
                   block_sector_t * new) {
    bool success = free_map_allocate(1, new);
    ASSERT(success);
    bc_write_block(existing_sec, existing);
    bc_zero(*new);
}

// off_t is the length up to which we will extend the inode's data
// This does not update the inodes' length member; that is the responsiblity
// of other functions.
static bool inode_iter_init(struct inode_iter * iter,
                            struct inode_disk * idisk,
                            struct inode * inode,
                            off_t extend_up_to_length) {
    ASSERT(iter != NULL);
    ASSERT(idisk != NULL);
    iter->idisk = idisk;
    iter->inode = inode;
    iter->bottom = idisk->blocks;
    iter->bottom_offset = 0;
    iter->first_level = (block_sector_t *) malloc(BLOCK_SECTOR_SIZE);
    if (iter->first_level == NULL) {
        return false;
    }
    iter->first_level_offset = -1;
    iter->second_level = (block_sector_t *) malloc(BLOCK_SECTOR_SIZE);
    if (iter->second_level == NULL) {
        free(iter->first_level);
        return false;
    }
    iter->second_level_offset = -1;

    iter->num_allocated_sectors = idisk->length / BLOCK_SECTOR_SIZE;
    if (idisk->length % BLOCK_SECTOR_SIZE > 0) {
        iter->num_allocated_sectors++;
    }
    if (extend_up_to_length <= idisk->length) {
        iter->max_num_allocated_sectors = iter->num_allocated_sectors;
    } else {
        iter->max_num_allocated_sectors =
            extend_up_to_length / BLOCK_SECTOR_SIZE;
        if (extend_up_to_length % BLOCK_SECTOR_SIZE > 0) {
            iter->max_num_allocated_sectors++;
        }
    }

    if (iter->num_allocated_sectors == 0 &&
        iter->max_num_allocated_sectors > 0) {
        extend(iter->inode->sector, (char *) iter->idisk, &idisk->start);
        bc_zero(idisk->start);
        iter->bottom[0] = idisk->start;
    }
    // TODO(agf): Assert that there will always be at least one allocated

    iter->sector_num = 0;
    // TODO(agf): Check that idisk->start and bottom[0] are synched everywhere
    iter->value = iter->bottom[0];
    return true;
}

static void inode_iter_deinit(struct inode_iter * iter) {
    free(iter->first_level);
    free(iter->second_level);
}

static block_sector_t inode_iter_next(struct inode_iter * iter) {
    iter->sector_num++;
    ASSERT(iter->sector_num < iter->max_num_allocated_sectors);

    // Position all offsets
    if (iter->second_level_offset != -1) {
        iter->second_level_offset++;
    } else if (iter->first_level_offset != -1) {
        iter->first_level_offset++;
    } else {
        iter->bottom_offset++;
    }
    if (iter->second_level_offset == 128) {
        iter->second_level_offset = 0;
        iter->first_level_offset++;
    }
    if (iter->first_level_offset == 128) {
        iter->first_level_offset = 0;
        iter->bottom_offset++;
    }
    if (iter->bottom_offset == NUM_DIRECT + NUM_INDIRECT + NUM_DINDIRECT) {
        PANIC("inode_iter_next() iterated too far");
    }

    // If there are new levels, mark the offsets accordingly
    if (iter->first_level_offset == -1 && iter->bottom_offset == NUM_DIRECT) {
        iter->first_level_offset = 0;
    } else if (iter->second_level_offset == -1 &&
               iter->bottom_offset == NUM_DIRECT + NUM_INDIRECT) {
        iter->second_level_offset = 0;
    }

    // Reload levels if necessary
    if (iter->first_level_offset == 0 &&
        (iter->second_level_offset == -1 || iter->second_level_offset == 0)) {
        if (iter->sector_num >= iter->num_allocated_sectors) {
            extend(iter->inode->sector, (char *) iter->idisk,
                   iter->bottom + iter->bottom_offset);
        }
        bc_read_block(iter->bottom[iter->bottom_offset], iter->first_level);
    }
    if (iter->second_level == 0) {
        if (iter->sector_num >= iter->num_allocated_sectors) {
            extend(iter->bottom[iter->bottom_offset],
                   (char *) iter->first_level,
                   iter->first_level + iter->first_level_offset);
        }
        bc_read_block(iter->first_level[iter->first_level_offset],
                      iter->second_level);
    }

    // Set value
    if (iter->second_level_offset != -1) {
        if (iter->sector_num >= iter->num_allocated_sectors) {
            extend(iter->first_level[iter->first_level_offset],
                   (char *) iter->second_level,
                   iter->second_level + iter->second_level_offset);
        }
        iter->value = iter->second_level[iter->second_level_offset];
    } else if (iter->first_level_offset != -1) {
        if (iter->sector_num >= iter->num_allocated_sectors) {
            extend(iter->bottom[iter->bottom_offset],
                   (char *) iter->first_level,
                   iter->first_level + iter->first_level_offset);
        }
        iter->value = iter->first_level[iter->first_level_offset];
    } else {
        if (iter->sector_num >= iter->num_allocated_sectors) {
            extend(iter->inode->sector,
                   (char *) iter->idisk,
                   iter->bottom + iter->bottom_offset);
        }
        iter->value = iter->bottom[iter->bottom_offset];
    }
    return iter->value;
}

// TODO(agf): Make this much faster by doing a "direct seek" rather than
// repeated iterations.
static block_sector_t inode_iter_seek(struct inode_iter * iter,
                                      block_sector_t offset) {
    ASSERT(iter->bottom_offset == 0 &&
           iter->first_level_offset == -1 &&
           iter->second_level_offset == -1);
    while (offset >= BLOCK_SECTOR_SIZE) {
        inode_iter_next(iter);
        offset -= BLOCK_SECTOR_SIZE;
    }
    return iter->value;
}

/*! List of open inodes, so that opening a single inode twice
    returns the same `struct inode'. */
static struct list open_inodes;

/*! Initializes the inode module. */
void inode_init(void) {
    list_init(&open_inodes);
}

bool inode_create(block_sector_t sector, off_t length UNUSED) {
    struct inode_disk disk_inode;

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode.length = 0;
    disk_inode.magic = INODE_MAGIC;

    bc_write_block(sector, &disk_inode);
    return true;
}

/*! Reads an inode from SECTOR
    and returns a `struct inode' that contains it.
    Returns a null pointer if memory allocation fails. */
struct inode * inode_open(block_sector_t sector) {
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector) {
            inode_reopen(inode);
            return inode;
        }
    }

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL)
        return NULL;

    struct inode_disk data;
    bc_read_block(sector, (void *) &data);

    /* Initialize. */
    list_push_front(&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    inode->length = data.length;
    lock_init(&inode->extend_lock);
    return inode;
}

/*! Reopens and returns INODE. */
struct inode * inode_reopen(struct inode *inode) {
    if (inode != NULL)
        inode->open_cnt++;
    return inode;
}

/*! Returns INODE's inode number. */
block_sector_t inode_get_inumber(const struct inode *inode) {
    return inode->sector;
}

/*! Closes INODE and writes it to disk.
    If this was the last reference to INODE, frees its memory.
    If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode) {
    /* Ignore null pointer. */
    if (inode == NULL)
        return;

    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0) {
        /* Remove from inode list and release lock. */
        list_remove(&inode->elem);

        /* Deallocate blocks if removed. */
        if (inode->removed) {
            // TODO(agfe): Free the rest of the blocks
            free_map_release(inode->sector, 1);
            // free_map_release(inode->data.start,
            //                  bytes_to_sectors(inode->data.length));
        }

        free(inode);
    }
}

/*! Marks INODE to be deleted when it is closed by the last caller who
    has it open. */
void inode_remove(struct inode *inode) {
    ASSERT(inode != NULL);
    inode->removed = true;
}


/*! Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
// TODO(agf): Update this for the new inode structure
// TODO(agf): What happens if we read from a file of length zero?
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset) {
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;
    // uint8_t *bounce = NULL;

    // TODO(agf): Instead of using malloc, pin the data in the filesystem cache
    // TODO(agf): Could take one small thing from the VM project: Implement
    // user stack growth!
    struct inode_disk * data = malloc(sizeof (struct inode_disk));
    ASSERT(data != NULL);
    bc_read_block(inode->sector, (void *) data);

    struct inode_iter iter;
    inode_iter_init(&iter, data, inode, 0);
    block_sector_t sector = inode_iter_seek(&iter, offset);

    while (size > 0) {
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        // Bytes left in inode, bytes left in sector, lesser of the two.
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        // Number of bytes to actually copy out of this sector.
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0) {
            break;
        }

        bc_read_block_bytes(sector, buffer + bytes_read, sector_ofs,
                            chunk_size);

        // Advance
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
        if (size > 0) {
            block_sector_t next_sector = inode_iter_next(&iter);
            ASSERT(next_sector != sector);
            sector = next_sector;
        }
    }
    inode_iter_deinit(&iter);
    free(data);

    return bytes_read;
}

/*! Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
    Returns the number of bytes actually written, which may be
    less than SIZE if end of file is reached or an error occurs.
    (Normally a write at end of file would extend the inode, but
    growth is not yet implemented.) */
// TODO(agf): Update this for the new inode structure
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size, off_t offset) {
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;

    off_t orig_offset = offset;

    if (inode->deny_write_cnt)
        return 0;

    // New algorithm:
    // Pass inode_iter_init and inode_iter_next a number that says how
    // many sectors they should allocate, if need be

    struct inode_disk * data = malloc(sizeof (struct inode_disk));
    ASSERT(data != NULL);
    bc_read_block(inode->sector, (void *) data);
    struct inode_iter iter;
    inode_iter_init(&iter, data, inode, offset + size);
    block_sector_t sector = inode_iter_seek(&iter, offset);

    off_t length = inode_length(inode);
    if (size + offset > length) {
        length = size + offset;
        lock_acquire(&inode->extend_lock);
    }

    while (size > 0) {
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        // Bytes left in inode, bytes left in sector, lesser of the two.
        off_t inode_left = length - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually write into this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0) {
            ASSERT(false);
            break;
        }

        bc_write_block_bytes(sector, (void *) (buffer + bytes_written),
                             sector_ofs, chunk_size, false);

        // Advance
        size -= chunk_size;
        offset += chunk_size;
        bytes_written += chunk_size;
        if (size > 0) {
            block_sector_t next_sector = inode_iter_next(&iter);
            ASSERT(next_sector != sector);
            sector = next_sector;
        }
    }
    inode_iter_deinit(&iter);

    if (orig_offset + bytes_written > inode_length(inode)) {
        inode->length = orig_offset + bytes_written;
        data->length = orig_offset + bytes_written;
        bc_write_block(inode->sector, (void *) data);
        ASSERT(orig_offset + bytes_written == inode_length(inode));
        lock_release(&inode->extend_lock);
    }
    free(data);

    return bytes_written;
}

/*! Disables writes to INODE.
    May be called at most once per inode opener. */
void inode_deny_write (struct inode *inode) {
    inode->deny_write_cnt++;
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/*! Re-enables writes to INODE.
    Must be called once by each inode opener who has called
    inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write (struct inode *inode) {
    ASSERT(inode->deny_write_cnt > 0);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
}

/*! Returns the length, in bytes, of INODE's data. */
/*! Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode) {
    return inode->length;
}

