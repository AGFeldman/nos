#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"

static void syscall_handler(struct intr_frame *);

void check_pointer_validity(void *p);
void check_many_pointer_validity(void *pmin, void *pmax);
void sys_exit_helper(int status);
void sys_exit(struct intr_frame *f);
void sys_write(struct intr_frame *f);
void sys_wait(struct intr_frame *f);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Exit with status -1 if |p| is an invalid user pointer. */
void check_pointer_validity(void *p) {
    if (p == NULL || !is_user_vaddr(p) ||
            pagedir_get_page(thread_current()->pagedir, p) == NULL) {
        // TODO(agf): Make sure that we do not leak system resources, as
        // mentioned in the assignment writeup
        sys_exit_helper(-1);
    }
}

/* Exit with status -1 if p is an invalid user pointer, for any p with
   pmin <= p <= pmax. */
void check_many_pointer_validity(void *pmin, void *pmax) {
    char * p;
    for (p = (char *)pmin; p <= (char *)pmax; p++) {
        check_pointer_validity(p);
    }
}

static void syscall_handler(struct intr_frame *f) {
    // TODO: Finish

    check_pointer_validity(f->esp);

    int syscall_num = *((int *) f->esp);
    if (syscall_num == SYS_HALT) {

    } else if (syscall_num == SYS_EXIT) {
        sys_exit(f);
    } else if (syscall_num == SYS_WAIT) {
        sys_wait(f);
    } else if (syscall_num == SYS_CREATE) {

    } else if (syscall_num == SYS_REMOVE) {

    } else if (syscall_num == SYS_OPEN) {

    } else if (syscall_num == SYS_FILESIZE) {

    } else if (syscall_num == SYS_READ) {

    } else if (syscall_num == SYS_WRITE) {
        sys_write(f);
    } else if (syscall_num == SYS_SEEK) {

    } else if (syscall_num == SYS_TELL) {

    } else if (syscall_num == SYS_CLOSE) {

    } else {
        // TODO(agf)
        printf("system call: not handled!\n");
    }
}

void sys_exit_helper(int status) {
    // TODO(agf): Process termination messages should be printed even if exit()
    // is not called. Maybe we should do this printing in process_exit().
    printf("%s: exit(%d)\n", thread_current()->name, status);
    // TODO(agf): Do we need to call process_exit()?
    thread_exit();
}

void sys_exit(struct intr_frame *f) {
    int * status_p = (int *) f->esp + 1;
    check_pointer_validity(status_p);
    int status = *status_p;
    f->eax = status;
    sys_exit_helper(status);
}

// TODO(agf): Finish. Tests seem to be failing.
void sys_wait(struct intr_frame *f) {
    // TODO(agf): Maybe use pid_t instead of int
    int * pid_p = (int *) f->esp + 1;
    check_pointer_validity(pid_p);
    int pid = *pid_p;
    // TODO(agf): Waits for a child process pid and retrieves the child's exit
    // status.
    int status = process_wait(pid);
    // TODO(agf): Check
    f->eax = status;
}

void sys_write(struct intr_frame *f) {
    check_many_pointer_validity((int *)f->esp + 1, (int *)f->esp + 3);
    int fd = *((int *) f->esp + 1);
    char * buf = (char *) *((int *) f->esp + 2);
    size_t n = (size_t) *((int *) f->esp + 3);

    if (fd == 1) {
        putbuf(buf, n);
        f->eax = n;
    }
    else {
        /*
        f->eax = file_write(, (void *) buf, (off_t) n);
        */
    }
}
