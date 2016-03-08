#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
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

static struct lock vm_lock;


bool vm_lock_held(void) {
    return lock_held_by_current_thread(&vm_lock);
}

void vm_lock_acquire(void) {
    lock_acquire(&vm_lock);
}

void vm_lock_release(void) {
    lock_release(&vm_lock);
}

void frame_table_init(void) {
    frame_table = (struct ft_entry *) malloc(NUM_USER_FRAMES *
                                             sizeof(struct ft_entry));
    user_pool = get_user_pool();
    user_pool_base_pg_no = pg_no(user_pool->base);
    lock_init(&vm_lock);
}

struct ft_entry * ft_lookup(void *kernel_vaddr) {
    ASSERT(page_from_pool(user_pool, kernel_vaddr));
    return frame_table + (pg_no(kernel_vaddr) - user_pool_base_pg_no);
}

void ft_add_user_mapping(void *upage, void *kpage, struct thread * trd) {
    if (trd == NULL) {
        trd = thread_current();
    }
    struct ft_entry * entry = ft_lookup(kpage);
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
    entry->trd = NULL;
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
    printf("Thread %p frame_evict 1\n", thread_current());
    bool already_held = vm_lock_held();
    if (!already_held) {
        vm_lock_acquire();
    }
    unsigned long framenum = random_ulong() % NUM_USER_FRAMES;
    struct ft_entry * fte = frame_table + framenum;
    printf("Thread %p frame_evict 2\n", thread_current());
    while (fte->kernel_vaddr == NULL || fte->user_vaddr == NULL) {
        // TODO(agf): It should be the case that kernel_vadr != NULL =>
        // user_vaddr != NULL, but it isn't.
        framenum = random_ulong() % NUM_USER_FRAMES;
        fte = frame_table + framenum;
    }
    printf("Thread %p frame_evict 3\n", thread_current());
    ASSERT(page_from_pool(user_pool, fte->kernel_vaddr));

    struct spt_entry * spte = spt_entry_get_or_create(fte->user_vaddr,
                                                      fte->trd);
    printf("Thread %p frame_evict 4\n", thread_current());

    // TODO(agf): If the frame is from a read-only part of an executable file,
    // we shouldn't use swap, because we can just read back from the
    // executable.
    int swap_slot = swap_dump_ft_entry(fte);

    printf("Thread %p frame_evict 5\n", thread_current());

    ASSERT(swap_slot != -1);
    // Update SPT
    spte->trd = fte->trd;
    spte->swap_page_number = swap_slot;
    spte->file = NULL;

    // Unmap the page from user space
    pagedir_clear_page(fte->trd->pagedir, fte->user_vaddr);

    printf("Thread %p frame_evict 6\n", thread_current());
    void * kernel_vaddr = fte->kernel_vaddr;
    // TODO(agf): Could assert that kernel_vaddr matches up with framenum
    fte->kernel_vaddr = NULL;
    fte->user_vaddr = NULL;
    fte->trd = NULL;
    if (!already_held) {
        vm_lock_release();
    }

    printf("Thread %p frame_evict 7\n", thread_current());
    return kernel_vaddr;
}
