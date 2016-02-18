#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
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
void check_pointer_validity(const void *p);
void check_many_pointer_validity(const void *pmin, const void *pmax);
void sys_exit_helper(int status);
void sys_exit(struct intr_frame *f);
void sys_exec(struct intr_frame *f);
void sys_open(struct intr_frame *f);
void sys_filesize(struct intr_frame *f);
void sys_read(struct intr_frame *f);
void sys_write(struct intr_frame *f);
void sys_wait(struct intr_frame *f);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Exit with status -1 if |p| is an invalid user pointer. */
void check_pointer_validity(const void *p) {
    if (p == NULL || !is_user_vaddr(p) ||
            pagedir_get_page(thread_current()->pagedir, p) == NULL) {
        // TODO(agf): Make sure that we do not leak system resources, as
        // mentioned in the assignment writeup
        sys_exit_helper(-1);
    }
}

/* Exit with status -1 if p is an invalid user pointer, for any p with
   pmin <= p <= pmax. */
void check_many_pointer_validity(const void *pmin, const void *pmax) {
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
        sys_exec(f);
    } else if (syscall_num == SYS_WAIT) {
        sys_wait(f);
    } else if (syscall_num == SYS_CREATE) {

    } else if (syscall_num == SYS_REMOVE) {

    } else if (syscall_num == SYS_OPEN) {
        sys_open(f);
    } else if (syscall_num == SYS_FILESIZE) {
        sys_filesize(f);
    } else if (syscall_num == SYS_READ) {
        sys_read(f);
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


void sys_halt() {
    shutdown_power_off();
}

void sys_exit_helper(int status) {
    // TODO(agf): Process termination messages should be printed even if exit()
    // is not called. Maybe we should do this printing in process_exit().
    printf("%s: exit(%d)\n", thread_name(), status);
    thread_current()->exit_status = status;
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


void sys_exec(struct intr_frame *f) {
    // TODO(agf): "The parent process cannot return from `exec` until it knows
    // whether the child process successfully loaded its executable. You must
    // use appropriate synchronization to ensure this."
    const char ** cmd_line_p = (const char **) f->esp + 1;
    check_pointer_validity(cmd_line_p);
    tid_t tid = process_execute(*cmd_line_p);
    f->eax = tid;
    if (tid == TID_ERROR) {
        f->eax = -1;
    }
}

void sys_open(struct intr_frame *f) {
    struct thread *intr_trd = thread_current();
    int i;
    for (i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (intr_trd->open_files[i] == NULL) {
            char * file_name = (char *) *((int *) f->esp + 1);
            check_pointer_validity(file_name);
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
}

void sys_filesize(struct intr_frame *f) {
    int fd = *((int *) f->esp + 1);
    struct thread *intr_trd = thread_current();
    struct file *afile = intr_trd->open_files[fd - 2];
    f->eax = file_length(afile);

}

void sys_read(struct intr_frame *f) {
        int fd = *((int *) f->esp + 1);
        char * buf = (char *) *((int *) f->esp + 2);
        unsigned n = (unsigned) *((int *) f->esp + 3);
        /* Read from console */
        if (fd == 0) {
            unsigned i;
            for (i = 0; i < n; i++) {
                buf[i] = input_getc();
                /* TODO: what if input buffer has less than i keys? */
            }
            f->eax =  n;
        }
        /* Read from file */
        else {
            struct thread *intr_trd = thread_current();
            /* Subtract 2 because fd 0 and 1 are taken for IO */
            struct file *afile = intr_trd->open_files[fd - 2];
            f->eax = file_read(afile, buf, n);
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
