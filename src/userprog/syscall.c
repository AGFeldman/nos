#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include "devices/input.h"

static void syscall_handler(struct intr_frame *);

void sys_halt(void);
void sys_exit(struct intr_frame *f);
void sys_open(struct intr_frame *f);
void sys_read(struct intr_frame *f);
void sys_write(struct intr_frame *f);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
    // TODO: Finish

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
        printf("system call: open\n");
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

void sys_exit(struct intr_frame *f) {
    int status = *((int *) f->esp + 1);
    f->eax = status;
    thread_exit();
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
        }
    }
    printf("Too many files open\n");
}

void sys_read(struct intr_frame *f) {
        int fd = *((int *) f->esp + 1);
        char * buf = (char *) *((int *) f->esp + 2);
        size_t n = (size_t) *((int *) f->esp + 3);
/* TODO: check buf is valid memory location */
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
        int fd = *((int *) f->esp + 1);
        char * buf = (char *) *((int *) f->esp + 2);
        size_t n = (size_t) *((int *) f->esp + 3);
/* TODO: check buf is valid memory location */

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
