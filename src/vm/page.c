#include "vm/page.h"


static struct hash supp_page_table;


void supp_page_table_init(void) {
    hash_init(&supp_page_table, spt_entry_hash, spt_entry_less, NULL);
}

// Returns a hash value for spt_entry e.
unsigned spt_entry_hash(const struct hash_elem *e_, void *aux UNUSED) {
    const struct spt_entry *e = hash_entry(e_, struct spt_entry, hash_elem);
    return hash_bytes(&e->addr, sizeof e->addr);
}

// Returns true if spt_entry a proceeds spt_entry b.
bool spt_entry_less(const struct hash_elem *a_, const struct hash_elem *b_,
                    void *aux UNUSED) {
    const struct spt_entry *a = hash_entry(a_, struct spt_entry, hash_elem);
    const struct spt_entry *b = hash_entry(b_, struct spt_entry, hash_elem);

    return a->addr < b->addr;
}

// Attempts to insert an spt_entry into the supplemental page table. If the
// supplemental already had an entry with the same address, then the insertion
// fails and we return a pointer to the prexisting entry with the same address.
// Otherwise, the insertion succeeds and we return NULL.
struct spt_entry * spt_entry_insert(struct spt_entry *entry) {
    struct hash_elem * elem = hash_insert(&supp_page_table,
                                          &entry->hash_elem);
    return elem == NULL ? NULL : hash_entry(elem, struct spt_entry, hash_elem);
}

// Returns the supplemental page table entry for the given vi
struct spt_entry * spt_entry_lookup(void *address) {
    struct spt_entry entry;
    struct hash_elem *elem;

    entry.addr = address;
    elem = hash_find(&supp_page_table, &entry.hash_elem);
    return elem != NULL ? hash_entry(elem, struct spt_entry, hash_elem) : NULL;
}
