/*! \file synch.h
 *
 * Data structures and function declarations for thread synchronization
 * primitives.
 */

#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/*! A counting semaphore. */
struct semaphore {
    unsigned value;             /*!< Current value. */
    struct list waiters;        /*!< List of waiting threads. */
};

void sema_init(struct semaphore *, unsigned value);
void sema_down(struct semaphore *);
bool sema_try_down(struct semaphore *);
void sema_up(struct semaphore *);
void sema_self_test(void);

/*! Lock. */
struct lock {
    /*! Thread holding lock. Useful for priority donation and debugging. */
    struct thread *holder;
    struct semaphore semaphore; /*!< Binary semaphore controlling access. */
    /*! List element for a thread's list of locks held */
    struct list_elem elem;
};

void lock_init(struct lock *);
void lock_acquire(struct lock *);
bool lock_try_acquire(struct lock *);
void lock_release(struct lock *);
bool lock_held_by_current_thread(const struct lock *);
int lock_get_priority(struct lock *);

/*! Condition variable. */
struct condition {
    struct list waiters;        /*!< List of waiting threads. */
};

void cond_init(struct condition *);
void cond_wait(struct condition *, struct lock *);
void cond_signal(struct condition *, struct lock *);
void cond_broadcast(struct condition *, struct lock *);

/*! Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

/*! Read-write lock. */
struct rwlock {
    struct lock mutex;
    struct condition can_read;
    struct condition can_write;
    int nreaders;
    bool writing;
};

void rwlock_init(struct rwlock *);
void rwlock_racquire(struct rwlock *);
void rwlock_rrelease(struct rwlock *);
void rwlock_wacquire(struct rwlock *);
void rwlock_wrelease(struct rwlock *);

#endif /* threads/synch.h */

