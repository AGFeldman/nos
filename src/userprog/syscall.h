#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdio.h>

void syscall_init(void);

void filesys_lock_acquire(void);
bool filesys_lock_held(void);
void filesys_lock_release(void);
void sys_exit_helper(int status);

#endif /* userprog/syscall.h */

