#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include "devices/input.h"

static void syscall_handler(struct intr_frame *);

void sys_halt(void);
void check_pointer_validity(void *p);
void check_many_pointer_validity(void *pmin, void *pmax);
void sys_exit_helper(int status);
void sys_exit(struct intr_frame *f);
void sys_open(struct intr_frame *f);
void sys_read(struct intr_frame *f);
void sys_write(struct intr_frame *f);

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
        sys_halt();
    } else if (syscall_num == SYS_EXIT) {
        sys_exit(f);
    } else if (syscall_num == SYS_EXEC) {

    } else if (syscall_num == SYS_EXIT) {
        printf("system call: wait\n");
    } else if (syscall_num == SYS_CREATE) {
        printf("system call: create\n");
    } else if (syscall_num == SYS_REMOVE) {
        printf("system call: remove\n");
    } else if (syscall_num == SYS_OPEN) {
        sys_open(f);
    } else if (syscall_num == SYS_FILESIZE) {
        printf("system call: filesize\n");
    } else if (syscall_num == SYS_READ) {
        sys_read(f);
    } else if (syscall_num == SYS_WRITE) {
        sys_write(f);
    } else if (syscall_num == SYS_SEEK) {
        printf("system call: seek\n");
    } else if (syscall_num == SYS_TELL) {
        printf("system call: tell\n");
    } else if (syscall_num == SYS_CLOSE) {
        printf("system call: close\n");
    } else {
        printf("system call: not handled!\n");
    }
}


void sys_halt() {
    shutdown_power_off();
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

void sys_open(struct intr_frame *f) {
    struct thread *intr_trd = thread_current();
    int i;
    for (i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (intr_trd->open_files[i] == NULL) {

            char * file_name = (char *) *((int *) f->esp + 1);
            struct file * afile = filesys_open(file_name);

            /* Check file successfully opened */
            if (!afile) {
                f->eax = -1;
                return;
            }

            intr_trd->open_files[i] = afile;
            f->eax = i + 2;
            return;
        }
    }
    printf("Too many files open\n");
}

void sys_read(struct intr_frame *f) {
        int fd = *((int *) f->esp + 1);
        char * buf = (char *) *((int *) f->esp + 2);
        size_t n = (size_t) *((int *) f->esp + 3);

        /* Read from console */
        if (fd == 0) {
            size_t i;
            for (i = 0; i < n; i++) {
                buf[i] = input_getc();
                /* TODO: what if input buffer has less than i keys? */
            }
            f->eax = (uint16_t) n;
        }
        /* Read from file */
        else {
            struct thread *intr_trd = thread_current();
            /* Subtract 2 because fd 0 and 1 are taken for IO */
            struct file *afile = intr_trd->open_files[fd - 2];
            f->eax = (uint16_t) file_read(afile, (void *) buf, (off_t) n);
        }
}

void sys_write(struct intr_frame *f) {
        check_many_pointer_validity((int *)f->esp + 1, (int *)f->esp + 3);

        int fd = *((int *) f->esp + 1);
        char * buf = (char *) *((int *) f->esp + 2);
        size_t n = (size_t) *((int *) f->esp + 3);

        /* Write to console */
        if (fd == 1) {
            putbuf(buf, n);
            f->eax = (uint16_t) n;
        }
        /* Write to file */
        else {
            struct thread *intr_trd = thread_current();
            /* Subtract 2 because fd 0 and 1 are taken for IO */
            struct file *afile = intr_trd->open_files[fd - 2];
            f->eax = (uint16_t) file_write(afile, (void *) buf, (off_t) n);
        }
}
