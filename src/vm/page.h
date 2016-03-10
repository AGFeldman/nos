#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <debug.h>
#include "threads/thread.h"
#include "filesys/file.h"
#include "lib/user/syscall.h"

struct spt_key {
    // User virtual address, which we will use as the input to a hash function.
    // This address should be the start of a page. TODO(agf): Assert this
    // in the appropriate functions, such as spt_entry_hash() and
    // spt_entry_lookup().
    void *addr;
};

// Supplemental page table entry
struct spt_entry {
    // The page-directory and user-virtual-address combination that uniquely
    // identifies this spt_entry.
    struct spt_key key;

    // Hash table element
    struct hash_elem hash_elem;

    // Not used
    // Physical address
    void *physical_addr;

    // True if the page is an all-zero page
    // TODO(agf): This is not being used yet. Instead, when loading an
    // executable, we have a non-null "file" field with file_read_bytes == 0.
    bool anonymous;

    // Fields used to lazily load executable files
    struct file *file;
    off_t file_ofs;
    size_t file_read_bytes;
    bool writable;

    // ID for mmap and munmap
    mapid_t mmapid;

    // TODO(agf): If the data is stored in swap, then we need info about that
    // Fields used for loading from swap files
    int swap_page_number;

    struct thread * trd;
};

void spt_init(struct hash *spt);

unsigned spt_entry_hash(const struct hash_elem *e_, void *aux UNUSED);

bool spt_entry_less(const struct hash_elem *a_, const struct hash_elem *b_,
                    void *aux UNUSED);

struct spt_entry * spt_entry_insert(struct spt_entry *, struct hash *);

struct spt_entry * spt_entry_allocate(void *, struct thread *);

struct spt_entry * spt_entry_get_or_create(void *, struct thread *);

struct spt_entry * spt_entry_lookup(void *, struct hash *);

#endif  // vm/page.h
