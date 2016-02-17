#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
    // TODO: Finish

    int syscall_num = *((int *) f->esp);
    if (syscall_num == SYS_HALT) {
        printf("system call: halt\n");
    } else if (syscall_num == SYS_EXIT) {
        printf("system call: exit\n");
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
        printf("system call: write\n");
    } else if (syscall_num == SYS_SEEK) {
        printf("system call: seek\n");
    } else if (syscall_num == SYS_TELL) {
        printf("system call: tell\n");
    } else if (syscall_num == SYS_CLOSE) {
        printf("system call: close\n");
    } else {
        printf("system call: not handled!\n");
    }
    thread_exit();
}
