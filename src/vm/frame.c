#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "random.h"


// The address where the frame table begins
static struct ft_entry * frame_table;

// Pointer to the user pool structure from palloc.c
static struct pool * user_pool;

// The page number for the base page of the user pool
static size_t user_pool_base_pg_no;


void frame_table_init(void) {
    frame_table = (struct ft_entry *) malloc(NUM_USER_FRAMES *
                                             sizeof(struct ft_entry));
    user_pool = get_user_pool();
    user_pool_base_pg_no = pg_no(user_pool->base);
}

struct ft_entry * ft_lookup(void *kernel_vaddr) {
    ASSERT(page_from_pool(user_pool, kernel_vaddr));
    return frame_table + (pg_no(kernel_vaddr) - user_pool_base_pg_no);
}

void ft_add_user_mapping(void *upage, void *kpage) {
    ft_lookup(kpage)->user_vaddr = upage;
}

void ft_init_entry(void *kpage) {
    // TODO(agf): Assert page offset is zero, and it is a kernel page, etc.
    struct ft_entry * entry = ft_lookup(kpage);
    entry->kernel_vaddr = kpage;
    entry->user_vaddr = NULL;
    entry->age = 0;
    lock_init(&entry->lock);
}

void ft_init_entries(void *kpage, size_t page_cnt) {
    uint8_t * cur_kpage = (uint8_t *) kpage;
    size_t i;
    for (i = 0; i < page_cnt; i++) {
        ft_init_entry((void *)(cur_kpage));
        cur_kpage += PGSIZE;
    }
}

void ft_deinit_entry(void *kpage) {
    // TODO(agf): Assert page offset is zero, and it is a kernel page, etc.
    struct ft_entry * entry = ft_lookup(kpage);
    entry->kernel_vaddr = NULL;
    entry->user_vaddr = NULL;
}

void ft_deinit_entries(void *kpage, size_t page_cnt) {
    uint8_t * cur_kpage = (uint8_t *) kpage;
    size_t i;
    for (i = 0; i < page_cnt; i++) {
        ft_deinit_entry((void *)(cur_kpage));
        cur_kpage += PGSIZE;
    }
}

// Find a physical page, write its contents to swap, and return its kernel
// virtual address
void * frame_evict(void) {
    unsigned long framenum = random_ulong() % NUM_USER_FRAMES;
    while (frame_table[framenum].kernel_vaddr == NULL) {
        framenum = random_ulong() % NUM_USER_FRAMES;
    }
    ASSERT(frame_table[framenum].user_vaddr != NULL);
    swap_dump_ft_entry(frame_table + framenum);
    frame_table[framenum].user_vaddr = NULL;
    return frame_table[framenum].kernel_vaddr;
}
