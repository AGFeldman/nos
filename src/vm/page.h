#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <debug.h>

// Supplemental page table entry
// TODO(agf): For now, these fields are just *guesses* at what we might want
// TODO(agf): Figure out where to get the memory for each spt_entry
struct spt_entry {
    // Hash table element
    struct hash_elem hash_elem;

    // User virtual address, which we will use as the input to a hash function.
    // This address should be the start of a page. TODO(agf): Assert this
    // in the appropriate functions, such as spt_entry_hash() and
    // spt_entry_lookup().
    void *addr;

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

struct spt_entry * spt_entry_lookup(void *address);

#endif  // vm/page.h
