#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"

static void syscall_handler(struct intr_frame *);

void check_pointer_validity(void *p);
void sys_exit_helper(int status);
void sys_exit(struct intr_frame *f);
void sys_write(struct intr_frame *f);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_pointer_validity(void *p) {
    if (p == NULL || !is_user_vaddr(p) ||
            pagedir_get_page(thread_current()->pagedir, p) == NULL) {
        // TODO(agf): Make sure that we do not leak system resources, as
        // mentioned in the assignment writeup
        sys_exit_helper(-1);
    }
}

static void syscall_handler(struct intr_frame *f) {
    // TODO: Finish

    check_pointer_validity(f->esp);

    int syscall_num = *((int *) f->esp);
    if (syscall_num == SYS_HALT) {
        printf("system call: halt\n");
    } else if (syscall_num == SYS_EXIT) {
        sys_exit(f);
    } else if (syscall_num == SYS_WAIT) {
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
        printf("system call: read\n");
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

void sys_exit_helper(int status) {
    // TODO(agf): Process termination messages should be printed even if exit()
    // is not called. Maybe we should do this printing in process_exit().
    printf("%s: exit(%d)\n", thread_current()->name, status);
    // TODO(agf): Do we need to call process_exit()?
    thread_exit();
}

void sys_exit(struct intr_frame *f) {
    int status = *((int *) f->esp + 1);
    f->eax = status;
    sys_exit_helper(status);
}

void sys_write(struct intr_frame *f) {
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
