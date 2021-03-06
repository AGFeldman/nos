#include "vm/page.h"
#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <stdio.h>

void spt_init(struct hash *spt) {
    hash_init(spt, spt_entry_hash, spt_entry_less, NULL);
}

// Returns a hash value for spt_entry e.
unsigned spt_entry_hash(const struct hash_elem *e_, void *aux UNUSED) {
    const struct spt_entry *e = hash_entry(e_, struct spt_entry, hash_elem);
    ASSERT(pg_round_down(e->key.addr) == e->key.addr);
    return hash_bytes(&e->key, sizeof e->key);
}

// Returns true if spt_entry a proceeds spt_entry b.
bool spt_entry_less(const struct hash_elem *a_, const struct hash_elem *b_,
                    void *aux UNUSED) {
    const struct spt_entry *a = hash_entry(a_, struct spt_entry, hash_elem);
    const struct spt_entry *b = hash_entry(b_, struct spt_entry, hash_elem);

    return  a->key.addr < b->key.addr;
}

// Attempts to insert an spt_entry into the supplemental page table. If the
// supplemental already had an entry with the same address, then the insertion
// fails and we return a pointer to the prexisting entry with the same address.
// Otherwise, the insertion succeeds and we return NULL.
struct spt_entry * spt_entry_insert(struct spt_entry *entry,
                                    struct hash * spt) {
    if (spt == NULL) {
        spt = &thread_current()->spt;
    }
    ASSERT(pg_round_down(entry->key.addr) == entry->key.addr);
    struct hash_elem * elem = hash_insert(spt, &entry->hash_elem);
    return elem == NULL ? NULL : hash_entry(elem, struct spt_entry, hash_elem);
}

// Allocates memory for an spt_entry and inserts into the hash table.
// Should only be called from the kernel, since this uses kernel malloc().
// Returns the address of the newly allocated spt_entry, or NULL if the table
// already had an entry with that combination of page directory and address.
struct spt_entry * spt_entry_allocate(void * address, struct thread * trd) {
    if (trd == NULL) {
        trd = thread_current();
    }
    struct spt_entry * entry = (struct spt_entry *) malloc(sizeof(
                                                           struct spt_entry));
    entry->key.addr = pg_round_down(address);

    // It is important that some fields be initialized
    entry->file = NULL;
    entry->mmapid = 0;
    entry->swap_page_number = -1;
    entry->trd = trd;

    struct spt_entry * result = spt_entry_insert(entry, &trd->spt);
    if (result == NULL) {
        // Successfully inserted
        return entry;
    }
    // Insertion failed because the table already had an entry with the same
    // key
    free((void *) entry);
    return NULL;
}

// Returns an spt_entry from a hash table. If there is not yet an entry in
// |spt| with user virtual address |address|, then allocate memory for a new
// spt_entry, insert it into the hash table, and return a pointer to it.
// See also spt_entry_allocate() and spt_entry_lookup().
// TODO(agf): Combine this with spt_entry_allocate().
struct spt_entry * spt_entry_get_or_create(void * address,
                                           struct thread * trd) {
    if (trd == NULL) {
        trd = thread_current();
    }
    struct spt_entry * entry = (struct spt_entry *) malloc(sizeof(
                                                           struct spt_entry));
    entry->key.addr = pg_round_down(address);

    // It is important that some fields be initialized
    entry->file = NULL;
    entry->swap_page_number = -1;
    entry->trd = trd;

    struct spt_entry * result = spt_entry_insert(entry, &trd->spt);
    if (result == NULL) {
        return entry;
    }
    // The table already had an entry with the same key
    free((void *) entry);
    return result;
}

// Returns the supplemental page table entry for the given combination of
// page directory and virtual address.
// Returns NULL if key not found.
struct spt_entry * spt_entry_lookup(void *address, struct hash * spt) {
    if (spt == NULL) {
        spt = &thread_current()->spt;
    }

    struct spt_entry entry;
    struct hash_elem *elem;

    entry.key.addr = pg_round_down(address);
    elem = hash_find(spt, &entry.hash_elem);
    return elem != NULL ? hash_entry(elem, struct spt_entry, hash_elem) : NULL;
}
