#include <bitmap.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "random.h"


// The address where the frame table begins
static struct ft_entry * frame_table;

// One address past the last entry in the frame table
static struct ft_entry * end_of_frame_table;

// Pointer to the user pool structure from palloc.c
static struct pool * user_pool;

// The page number for the base page of the user pool
static size_t user_pool_base_pg_no;

// The number of frames that can hold user pages.
// Excludes frames that store the user-pool bitmap.
static size_t num_user_frames;

// Used for clock eviction policy.
static struct ft_entry * clock_hand;

static struct lock eviction_lock;


bool eviction_lock_held(void) {
    return lock_held_by_current_thread(&eviction_lock);
}

void eviction_lock_acquire(void) {
    lock_acquire(&eviction_lock);
}

void eviction_lock_release(void) {
    lock_release(&eviction_lock);
}

void frame_table_init(void) {
    user_pool = get_user_pool();
    user_pool_base_pg_no = pg_no(user_pool->base);
    // num_user_frames = user_pool->used_map->bit_cnt;
    // struct bitmap * used_map = user_pool->used_map;
    // num_user_frames = used_map->bit_cnt;
    num_user_frames = bitmap_size(user_pool->used_map);
    frame_table = (struct ft_entry *) malloc(num_user_frames*
                                             sizeof(struct ft_entry));
    size_t i;
    for (i = 0; i < num_user_frames; i++) {
        struct ft_entry * entry = frame_table + i;
        entry->kernel_vaddr = NULL;
        entry->user_vaddr = NULL;
        entry->trd = NULL;
        lock_init(&entry->lock);
    }
    lock_init(&eviction_lock);
    clock_hand = frame_table;
    end_of_frame_table = frame_table + num_user_frames;
}

struct ft_entry * ft_lookup(void *kernel_vaddr) {
    ASSERT(page_from_pool(user_pool, kernel_vaddr));
    return frame_table + (pg_no(kernel_vaddr) - user_pool_base_pg_no);
}

void ft_add_user_mapping(void *upage, void *kpage, struct thread * trd) {
    if (trd == NULL) {
        trd = thread_current();
    }
    ASSERT(pg_round_down(upage) == upage);
    ASSERT(pg_round_down(kpage) == kpage);
    ASSERT(page_from_pool(user_pool, kpage));
    struct ft_entry * entry = ft_lookup(kpage);
    // TODO(agf): ASSERT(entry->kernel_vaddr == NULL) ?
    ASSERT(entry->kernel_vaddr == NULL || entry->kernel_vaddr == kpage);
    ASSERT(entry->user_vaddr == NULL);
    entry->kernel_vaddr = kpage;
    entry->user_vaddr = upage;
    entry->trd = trd;
}

void ft_init_entry(void *kpage) {
    // TODO(agf): Assert page offset is zero, and it is a kernel page, etc.
    ASSERT(pg_round_down(kpage) == kpage);
    struct ft_entry * entry = ft_lookup(kpage);
    entry->kernel_vaddr = kpage;
    entry->user_vaddr = NULL;
    entry->trd = NULL;
    lock_init(&entry->lock);
}

// Set kernel_vaddr for several frame table entries
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
    entry->trd = NULL;
}

// Reset several frame table entries
// TODO(agf): Rename this
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
    eviction_lock_acquire();
    for (; true; clock_hand++) {
        if (clock_hand == end_of_frame_table) {
            clock_hand = frame_table;
        }
        if (clock_hand->kernel_vaddr == NULL ||
            clock_hand->user_vaddr == NULL ||
            clock_hand->trd == NULL ||
            clock_hand->trd->pagedir == NULL) {
            // TODO(agf): Check that we really need conditions other than
            // just clock_hand->kernel_vaddr == NULL
            continue;
        }
        bool accessed = false;
        if (pagedir_is_accessed(clock_hand->trd->pagedir,
                                clock_hand->kernel_vaddr)) {
            accessed = true;
            pagedir_set_accessed(clock_hand->trd->pagedir,
                                 clock_hand->kernel_vaddr, false);
        }
        if (pagedir_is_accessed(clock_hand->trd->pagedir,
                                clock_hand->user_vaddr)) {
            accessed = true;
            pagedir_set_accessed(clock_hand->trd->pagedir,
                                 clock_hand->user_vaddr, false);
        }
        if (!accessed) {
            break;
        }
    }
    ASSERT(page_from_pool(user_pool, clock_hand->kernel_vaddr));

    struct spt_entry * spte = spt_entry_get_or_create(clock_hand->user_vaddr,
                                                      clock_hand->trd);

    // TODO(agf): If the frame is from a read-only part of an executable file,
    // we shouldn't use swap, because we can just read back from the
    // executable. The frame table should include data to describe this.
    // TODO(agf): The frame table could also describe whether the page in the
    // frame is currently pinned.
    // Should pin pages before reading into them from swap.
    int swap_slot = swap_dump_ft_entry(clock_hand);
    ASSERT(swap_slot != -1);
    // Update SPT
    spte->trd = clock_hand->trd;
    spte->swap_page_number = swap_slot;
    spte->file = NULL;

    // Unmap the page from user space
    ASSERT(clock_hand->trd->pagedir != NULL);
    pagedir_clear_page(clock_hand->trd->pagedir, clock_hand->user_vaddr);

    void * kernel_vaddr = clock_hand->kernel_vaddr;
    // TODO(agf): Could assert that kernel_vaddr matches up with framenum
    clock_hand->kernel_vaddr = NULL;
    clock_hand->user_vaddr = NULL;
    clock_hand->trd = NULL;

    eviction_lock_release();
    return kernel_vaddr;
}
