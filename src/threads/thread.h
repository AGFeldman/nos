/*! \file thread.h
 *
 * Declarations for the kernel threading functionality in PintOS.
 */

#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <hash.h>
#include <stdint.h>
#include "threads/synch.h"

/*! States in a thread's life cycle. */
enum thread_status {
    THREAD_RUNNING,     /*!< Running thread. */
    THREAD_READY,       /*!< Not running but ready to run. */
    THREAD_BLOCKED,     /*!< Waiting for an event to trigger. */
    THREAD_SLEEPING,    /*!< Waiting for a set length of time. */
    THREAD_DYING        /*!< About to be destroyed. */
};

#define MAX_FILE_DESCRIPTORS 16

/*! Thread identifier type.
    You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /*!< Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /*!< Lowest priority. */
#define PRI_DEFAULT 31                  /*!< Default priority. */
#define PRI_MAX 63                      /*!< Highest priority. */

/*! A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

\verbatim
        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+
\endverbatim

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion.

   The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list.
*/
struct thread {
    /*! Owned by thread.c. */
    /**@{*/
    tid_t tid;                          /*!< Thread identifier. */
    enum thread_status status;          /*!< Thread state. */
    char name[16];                      /*!< Name (for debugging purposes). */
    uint8_t *stack;                     /*!< Saved stack pointer. */
    int priority;                       /*!< Priority. */
    int nice;                           /*!< Niceness value. */
    /*! A measure of how much recent time this thread has spent on the CPU,
        expressed as a fixed-point real number.
        TODO(agf): It is messy to use `int` here instead of FPNUM. */
    int recent_cpu;
    struct list_elem allelem;           /*!< List element for all threads list. */
    /**@}*/

    /*! Shared between thread.c and synch.c. */
    /**@{*/
    struct list_elem elem;              /*!< List element. */
    int64_t wake_time;                      /*!< For sleeping, when to wake. */
    /**@}*/

    struct list locks_held;             /*!< List of locks held. */

    /*! List element for the parent's child_list */
    struct list_elem child_list_elem;
    /*! List of children */
    struct list child_list;
    /*! Lock released when thread dies */
    struct lock life_lock;

    /*! Used to store exit status upon exit() syscall invocation. */
    int exit_status;

    /*! The executable file for this process, if applicable. */
    struct file * executable;

#ifdef USERPROG
    /*! Owned by userprog/process.c. */
    /**@{*/
    uint32_t *pagedir;                  /*!< Page directory. */
    /**@{*/

    // Supplemental page table
    struct hash spt;
#endif

    /* Fixed-size array of pointers to open file structs */
    struct file *open_files[16];

    // Page-aligned pointers or NULL pointers
    // If both are not NULL, then they indicate to the page fault handler that
    // user memory accessses that fall in the pages between pin_begin_page
    // and pin_end_page, inclusive, should be pinned to physical memory
    // after being loaded into physical memory and before being mapped to
    // userspace.
    unsigned char * pin_begin_page;
    unsigned char * pin_end_page;

    /*! Owned by thread.c. */
    /**@{*/
    unsigned magic;                     /* Detects stack overflow. */
    /**@}*/
};

/*! If false (default), use round-robin scheduler.
    If true, use multi-level feedback queue scheduler.
    Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

/*! System-wide load average.
    Should be updated once per second. */
// TODO(agf): It is very messy to use `int` here instead of FPNUM
extern int system_load_avg;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

void thread_sleep(int64_t);
void thread_wake(struct thread *);

struct thread *thread_current (void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

/*! Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread *t, void *aux);

void thread_foreach(thread_action_func *, void *);

int thread_get_other_priority(struct thread *t);
int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
void thread_update_load_avg(void);
void thread_update_priorities(void);
void thread_update_recent_cpus(void);
int thread_get_load_avg(void);

#endif /* threads/thread.h */

