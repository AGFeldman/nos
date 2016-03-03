#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <debug.h>

// A combination of page directory and user virtual address used as a hash
// table key for the supplemental page table.
struct spt_key {
    // Page directory that the user virtual address is associated with
    uint32_t *pd;

    // User virtual address, which we will use as the input to a hash function.
    // This address should be the start of a page. TODO(agf): Assert this
    // in the appropriate functions, such as spt_entry_hash() and
    // spt_entry_lookup().
    void *addr;
};

// Supplemental page table entry
// TODO(agf): For now, these fields are just *guesses* at what we might want
// TODO(agf): Figure out where to get the memory for each spt_entry
struct spt_entry {
    // The page-directory and user-virtual-address combination that uniquely
    // identifies this spt_entry.
    struct spt_key key;

    // Hash table element
    struct hash_elem hash_elem;

    // Physical address
    void *physical_addr;

    // True if the page is read-only
    bool read_only;

    // True if the page is an all-zero page
    bool anonymous;

    // TODO(agf): If the data is from a memory mapped file, then we need:
    // - The file where the data is from
    // - Starting offset in file
    // - Number of bytes to read

    // TODO(agf): If the data is stored in swap, then we need info about that
};

void supp_page_table_init(void);

unsigned spt_entry_hash(const struct hash_elem *e_, void *aux UNUSED);

bool spt_entry_less(const struct hash_elem *a_, const struct hash_elem *b_,
                    void *aux UNUSED);

struct spt_entry * spt_entry_insert(struct spt_entry *entry);

struct spt_entry * spt_entry_allocate(uint32_t *, void *);

struct spt_entry * spt_entry_lookup(uint32_t *pd, void *);

#endif  // vm/page.h
