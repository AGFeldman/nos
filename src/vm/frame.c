#include <bitmap.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
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
        entry->acquired_during_eviction = false;
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

// TODO(agf): Rename to something like set_kernel_vaddr()
void ft_init_entry(void *kpage) {
    // TODO(agf): Assert page offset is zero, and it is a kernel page, etc.
    ASSERT(pg_round_down(kpage) == kpage);
    struct ft_entry * entry = ft_lookup(kpage);
    entry->kernel_vaddr = kpage;
    entry->user_vaddr = NULL;
    entry->trd = NULL;
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
    entry->acquired_during_eviction = false;
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
// TODO(agf): This actually evicts multiple pages, because palloc_get_multiple
// seems to need this. Is this really necessary?
void * frame_evict(size_t page_cnt) {
    /// printf("AGF: Eviction is happening\n");
    eviction_lock_acquire();
    // Find page_cnt contiguous pages that we can evict
    for (; true; clock_hand++) {
        if (clock_hand == end_of_frame_table) {
            clock_hand = frame_table;
        }
        bool can_evict = true;
        size_t i;
        for (i = 0; i < page_cnt; i++) {
            struct ft_entry * fte = clock_hand + i;
            if (fte == end_of_frame_table) {
                can_evict = false;
                break;
            }
            if (fte->kernel_vaddr == NULL ||
                fte->user_vaddr == NULL ||
                fte->trd == NULL ||
                fte->trd->pagedir == NULL) {
                // TODO(agf): Check that we really need conditions other than
                // just fte->kernel_vaddr == NULL
                can_evict = false;
                continue;
            }
            if (pagedir_is_accessed(fte->trd->pagedir, fte->kernel_vaddr)) {
                can_evict = false;
                if (i == 0) {
                    pagedir_set_accessed(fte->trd->pagedir,
                                         fte->kernel_vaddr, false);
                }
            }
            if (pagedir_is_accessed(fte->trd->pagedir, fte->user_vaddr)) {
                can_evict = false;
                if (i == 0) {
                    pagedir_set_accessed(fte->trd->pagedir,
                                         fte->user_vaddr, false);
                }
            }
            if (!fte->acquired_during_eviction) {
                // TODO(agf): This is racy. This is a big problem.
                if (!lock_held_by_current_thread(&fte->lock)) {
                    bool acquired = lock_try_acquire(&fte->lock);
                    if (acquired) {
                        fte->acquired_during_eviction = true;
                    } else {
                        can_evict = false;
                    }
                }
            }
        }
        if (can_evict) {
            break;
        } else if (clock_hand->acquired_during_eviction) {
            clock_hand->acquired_during_eviction = false;
            lock_release(&clock_hand->lock);
        }
        // TODO(agf): Panic if this is infinite-looping
    }

    // TODO(agf): Could assert that kernel_vaddr matches up with framenum
    // Kernel virtual address to return
    void * kernel_vaddr = clock_hand->kernel_vaddr;

    // Evict page_cnt pages, starting at clock_hand
    size_t i;
    for (i = 0; i < page_cnt; i++) {
        struct ft_entry * fte = clock_hand + i;
        ASSERT(fte < end_of_frame_table);
        ASSERT(page_from_pool(user_pool, fte->kernel_vaddr));

        struct spt_entry * spte = spt_entry_get_or_create(fte->user_vaddr,
                                                          fte->trd);

        // TODO(agf): If the frame is from a read-only part of an executable
        // file, we shouldn't use swap, because we can just read back from the
        // executable. The frame table should include data to describe this.
        // TODO(agf): The frame table could also describe whether the page in
        // the frame is currently pinned.
        // Should pin pages before reading into them from swap.
        int swap_slot = swap_dump_ft_entry(fte);
        ASSERT(swap_slot != -1);
        // Update SPT
        spte->trd = fte->trd;
        spte->swap_page_number = swap_slot;
        spte->file = NULL;

        // Unmap the page from user space
        ASSERT(fte->trd->pagedir != NULL);
        pagedir_clear_page(fte->trd->pagedir, fte->user_vaddr);

        fte->kernel_vaddr = NULL;
        fte->user_vaddr = NULL;
        fte->trd = NULL;

        fte->acquired_during_eviction = false;
        lock_release(&fte->lock);
    }

    eviction_lock_release();
    return kernel_vaddr;
}

void pin_pages(unsigned char * uaddr, size_t num_pages) {
    ASSERT(is_user_vaddr(uaddr));
    ASSERT(pg_round_down(uaddr) == uaddr);
    ASSERT(num_pages > 0);
    bool need_pgfault_handler = false;
    size_t i = 0;
    struct thread * cur_thread = thread_current();
    uint32_t * pagedir = cur_thread->pagedir;
    eviction_lock_acquire();
    for (; i < num_pages; i++) {
        void * kaddr = pagedir_get_page(pagedir, uaddr + i * PGSIZE);
        if (kaddr == NULL) {
            need_pgfault_handler = true;
        } else {
            bool acquired = lock_try_acquire(&ft_lookup(kaddr)->lock);
            ASSERT(acquired);
        }
        if (need_pgfault_handler) {
            cur_thread->pin_begin_page = uaddr;
            cur_thread->pin_end_page = uaddr + (num_pages - 1) * PGSIZE;
        }
    }
    eviction_lock_release();
    if (need_pgfault_handler) {
        // The messy code with "throwaway" is intended to force the compiler
        // to perform a pointer dereference, which causes a pagefault.
        unsigned char volatile throwaway = 0;
        for (i = 0; i < num_pages; i++) {
            throwaway = *((unsigned char *)(uaddr + i * PGSIZE));
            if (throwaway == 0) {
                throwaway += 1;
            }
            // Use throwaway to assert something that is always true so that
            // the compiler does not get rid of throwaway.
            ASSERT(throwaway >= 1);
        }
    }
    cur_thread->pin_begin_page = NULL;
    cur_thread->pin_end_page = NULL;
}

void unpin_pages(unsigned char * uaddr, size_t num_pages) {
    ASSERT(is_user_vaddr(uaddr));
    ASSERT(pg_round_down(uaddr) == uaddr);
    uint32_t * pagedir = thread_current()->pagedir;
    size_t i;
    for (i = 0; i < num_pages; i++) {
        void * kaddr = pagedir_get_page(pagedir, uaddr + i * PGSIZE);
        ASSERT(kaddr != NULL);
        lock_release(&ft_lookup(kaddr)->lock);
    }
}

void pin_pages_by_buffer(unsigned char * buffer, off_t num_bytes) {
    unsigned char * start = pg_round_down(buffer);
    int num_pages = pg_no(buffer + num_bytes) - pg_no(buffer) + 1;
    pin_pages(start, num_pages);
}
void unpin_pages_by_buffer(unsigned char * buffer, off_t num_bytes) {
    unsigned char * start = pg_round_down(buffer);
    int num_pages = pg_no(buffer + num_bytes) - pg_no(buffer) + 1;
    unpin_pages(start, num_pages);
}
