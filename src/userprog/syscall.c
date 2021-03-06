#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "vm/frame.h"
#include "vm/page.h"
#include <round.h>

// Page size, in bytes.
// This seems like it should be defined elsewhere, but apparently is not.
#define PAGE_SIZE_BYTES 4096

static struct lock filesys_lock;

static void syscall_handler(struct intr_frame *);

void sys_halt(void);
void check_pointer_validity(void *, struct intr_frame *);
void check_many_pointer_validity(void *, void *, struct intr_frame *);
void sys_exit(struct intr_frame *f);
void sys_exec(struct intr_frame *f);
void sys_open(struct intr_frame *f);
void sys_filesize(struct intr_frame *f);
void sys_read(struct intr_frame *f);
void sys_write(struct intr_frame *f);
void sys_seek(struct intr_frame *f);
void sys_wait(struct intr_frame *f);
void sys_create(struct intr_frame *f);
void sys_remove(struct intr_frame *f);
void sys_close(struct intr_frame *f);
void sys_tell(struct intr_frame *f);
void sys_mmap(struct intr_frame *f);
void sys_munmap(struct intr_frame *f);

void syscall_init(void) {
    lock_init(&filesys_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void filesys_lock_acquire(void) {
    lock_acquire(&filesys_lock);
}

bool filesys_lock_held(void) {
    return lock_held_by_current_thread(&filesys_lock);
}

void filesys_lock_release(void) {
    lock_release(&filesys_lock);
}

/* Exit with status -1 if |p| is an invalid user pointer. */
void check_pointer_validity(void *p, struct intr_frame *f) {
    if (p == NULL || !is_user_vaddr(p)) {
        sys_exit_helper(-1);
    }
    bool user_stack = false;
    // TODO(agf): Document max stack size of 2048 * PGSIZE
    // TODO(agf): This is very similar to the code in the pagefault handler
    if (p >= (void *)((uint8_t *) f->esp - 32) &&
            p >= PHYS_BASE - 2048 * PGSIZE) {
        user_stack = true;
        // TODO(agf): Updated this comment
        // We know that the pointer is in userspace, so if it is not any lower
        // than esp - 32 bytes, we assume that it is a stack access.
        // This stack access might cause a page fault, which is fine; the page
        // fault handler will grow the stack if needed.
    }
    struct thread * cur_thread = thread_current();
    uint32_t *pd = cur_thread->pagedir;
    // TODO(agf): A lot of this is repeated in the page fault handler
    if (pagedir_get_page(pd, p) == NULL && spt_entry_lookup(p, NULL) == NULL) {
        if (user_stack) {
            bool pin = false;
            if (cur_thread->pin_begin_page != NULL) {
                ASSERT(cur_thread->pin_end_page != NULL);
                ASSERT(pg_round_down(cur_thread->pin_begin_page) ==
                       cur_thread->pin_begin_page);
                ASSERT(pg_round_down(cur_thread->pin_end_page) ==
                       cur_thread->pin_end_page);
                unsigned char * round_addr = pg_round_down(p);
                if (round_addr >= cur_thread->pin_begin_page &&
                    round_addr <= cur_thread->pin_end_page) {
                    pin = true;
                }
            }
            if (allocate_and_install_blank_page(p, pin)) {
                return;
            }
        }
        sys_exit_helper(-1);
    }
}

/* Exit with status -1 if p is an invalid user pointer, for any p with
   pmin <= p <= pmax. */
void check_many_pointer_validity(void *pmin, void *pmax,
                                 struct intr_frame *f) {
    char * p;
    for (p = (char *)pmin; p < (char *)pmax; p += PAGE_SIZE_BYTES) {
        check_pointer_validity(p, f);
    }
    check_pointer_validity(pmax, f);
}

static void syscall_handler(struct intr_frame *f) {
    check_pointer_validity(f->esp, f);
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
        sys_create(f);
    } else if (syscall_num == SYS_REMOVE) {
        sys_remove(f);
    } else if (syscall_num == SYS_OPEN) {
        sys_open(f);
    } else if (syscall_num == SYS_FILESIZE) {
        sys_filesize(f);
    } else if (syscall_num == SYS_READ) {
        sys_read(f);
    } else if (syscall_num == SYS_WRITE) {
        sys_write(f);
    } else if (syscall_num == SYS_SEEK) {
        sys_seek(f);
    } else if (syscall_num == SYS_TELL) {
        sys_tell(f);
    } else if (syscall_num == SYS_CLOSE) {
        sys_close(f);
    } else if (syscall_num == SYS_MMAP) {
        sys_mmap(f);
    } else if (syscall_num == SYS_MUNMAP) {
        sys_munmap(f);
    } else {
        // TODO(agf)
        printf("system call: not handled!\n");
    }
}

void sys_halt() {
    shutdown_power_off();
}

// Return true if a mmapid was processed, false otherwise
static bool sys_munmap_helper(mapid_t mapping) {
    struct hash spt = thread_current()->spt;
    struct hash_iterator i;
    hash_first (&i, &spt);
    struct file *file = NULL;
    while (hash_next (&i)) {
        struct spt_entry *entry = hash_entry (hash_cur (&i), struct spt_entry,
               hash_elem);
        if (mapping == 0 && entry->mmapid != 0) {
            mapping = entry->mmapid;
        }
        if (entry->mmapid == mapping && mapping != 0) {
            if (pagedir_is_dirty(thread_current()->pagedir, entry->key.addr)) {
                // TODO(agf): Should not need this `if` statement. mmapid != 0
                // should mean that file != NULL.
                // But we do need this `if` statement, probably because
                // mmap updates to entry fields are getting interrupted.
                if (entry->file != NULL) {
                    filesys_lock_acquire();
                    file_write_at(entry->file, entry->key.addr,
                                  entry->file_read_bytes, entry->file_ofs);
                    filesys_lock_release();
                }
            }
            file = entry->file;
            entry->file = NULL;
            entry->mmapid = 0;
       }
    }
    file_close(file);
    return file != NULL;
}

void sys_exit_helper(int status) {
    if (filesys_lock_held()) {
        filesys_lock_release();
    }
    printf("%s: exit(%d)\n", thread_name(), status);
    while (sys_munmap_helper(0));
    thread_current()->exit_status = status;
    thread_exit();
}

void sys_exit(struct intr_frame *f) {
    int * status_p = (int *) f->esp + 1;
    check_pointer_validity(status_p, f);
    int status = *status_p;
    f->eax = status;
    sys_exit_helper(status);
}

void sys_wait(struct intr_frame *f) {
    int * pid_p = (int *) f->esp + 1;
    check_pointer_validity(pid_p, f);
    int pid = *pid_p;
    int status = process_wait(pid);
    f->eax = status;
}

void sys_exec(struct intr_frame *f) {
    const char ** cmd_line_p = (const char **) f->esp + 1;
    check_pointer_validity(cmd_line_p, f);
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
            // char ** file_name_p = (char
            char * file_name = (char *) *((int *) f->esp + 1);
            check_pointer_validity(file_name, f);
            filesys_lock_acquire();
            struct file * afile = filesys_open(file_name);
            filesys_lock_release();

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

void sys_close(struct intr_frame *f) {
    check_pointer_validity((int *) f->esp + 1, f);
    int fd = *((int *) f->esp + 1);
    int open_files_index = fd - 2;
    if (open_files_index < 0 || open_files_index >= MAX_FILE_DESCRIPTORS) {
        return;
    }
    struct thread * intr_trd = thread_current();
    struct file *file = intr_trd->open_files[open_files_index];
    if (file != NULL) {
        filesys_lock_acquire();
        file_close(file);
        filesys_lock_release();
        intr_trd->open_files[open_files_index] = NULL;
    }
}

void sys_tell(struct intr_frame *f) {
    check_pointer_validity((int *) f->esp + 1, f);
    int fd = *((int *) f->esp + 1);
    int open_files_index = fd - 2;
    if (open_files_index < 0 || open_files_index >= MAX_FILE_DESCRIPTORS) {
        f->eax = -1;
        return;
    }
    struct thread * intr_trd = thread_current();
    struct file *file = intr_trd->open_files[open_files_index];
    if (file == NULL) {
        f->eax = -1;
        return;
    }
    filesys_lock_acquire();
    f->eax = file_tell(file);
    filesys_lock_release();
}

void sys_filesize(struct intr_frame *f) {
    int fd = *((int *) f->esp + 1);
    struct thread *intr_trd = thread_current();
    struct file *afile = intr_trd->open_files[fd - 2];
    filesys_lock_acquire();
    f->eax = file_length(afile);
    filesys_lock_release();
}

void sys_read(struct intr_frame *f) {
    int fd = *((int *) f->esp + 1);
    char * buf = (char *) *((int *) f->esp + 2);
    unsigned n = (unsigned) *((int *) f->esp + 3);
    // TODO(agf): Decide what to do here
    // check_pointer_validity(buf, f);
    // check_pointer_validity(buf + n - 1, f);
    check_many_pointer_validity(buf, buf + n - 1, f);
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
        if (fd > MAX_FILE_DESCRIPTORS + 1 || fd < 2) {
            sys_exit_helper(-1);
        }
        struct thread *intr_trd = thread_current();
        /* Subtract 2 because fd 0 and 1 are taken for IO */
        struct file *afile = intr_trd->open_files[fd - 2];
        if (!afile) {
            sys_exit_helper(-1);
        }
        pin_pages_by_buffer((unsigned char *)buf, n);
        filesys_lock_acquire();
        f->eax = file_read(afile, buf, n);
        filesys_lock_release();
        unpin_pages_by_buffer((unsigned char *)buf, n);
    }
}

void sys_write(struct intr_frame *f) {
    int * fd_p = (int *) f->esp + 1;
    check_pointer_validity(fd_p, f);
    int fd = *fd_p;
    char ** buf_p = (char **) ((int *) f->esp + 2);
    check_pointer_validity(buf_p, f);
    char * buf = *buf_p;
    unsigned * n_p = (unsigned *) ((int *) f->esp + 3);
    check_pointer_validity(n_p, f);
    unsigned n = *n_p;

    /* Write to console */
    if (fd == 1) {
        // TODO(agf): Might need to break into chunks?
        putbuf(buf, n);
        f->eax = (uint16_t) n;
    }
    /* Write to file */
    else {
        struct thread *intr_trd = thread_current();
        /* Subtract 2 because fd 0 and 1 are taken for IO */
        struct file *afile = intr_trd->open_files[fd - 2];
        pin_pages_by_buffer((unsigned char *)buf, n);
        filesys_lock_acquire();
        f->eax = file_write(afile, buf, n);
        filesys_lock_release();
        unpin_pages_by_buffer((unsigned char *)buf, n);
    }
}

void sys_create(struct intr_frame *f) {
    char ** file_p = (char **) ((int *) f->esp + 1);
    check_pointer_validity(file_p, f);
    char * file = *file_p;
    unsigned int * initial_size_p = (unsigned int *) f->esp + 2;
    check_pointer_validity(initial_size_p, f);
    unsigned int initial_size = *initial_size_p;
    if (strlen(file) == 0) {
        f->eax = 0;
        return;
    }
    filesys_lock_acquire();
    int success = filesys_create(file, initial_size);
    filesys_lock_release();
    f->eax = success;
}

void sys_remove(struct intr_frame *f) {
    check_pointer_validity((int *) f->esp + 1, f);
    char * file = *((char **) ((int *) f->esp + 1));
    filesys_lock_acquire();
    int success = filesys_remove(file);
    filesys_lock_release();
    f->eax = success;
}

void sys_seek(struct intr_frame *f) {
    check_pointer_validity((int *) f->esp + 1, f);
    int fd = *((int *) f->esp + 1);
    check_pointer_validity((int *) f->esp + 2, f);
    off_t position = (off_t) *((int *) f->esp + 2);
    struct thread *intr_trd = thread_current();
    struct file *afile = intr_trd->open_files[fd - 2];

    filesys_lock_acquire();
    file_seek(afile, position);
    filesys_lock_release();
}

/*
 * mapid_t mmap (int fd, void *addr)
 * Maps the file open as fd into the process's virtual address space.
 * The entire file is mapped into consecutive virtual pages starting at addr.
 */
void sys_mmap(struct intr_frame *f) {
    check_pointer_validity((int *) f->esp + 1, f);
    int fd = *((int *) f->esp + 1);
    // fd can't be I/O
    if (fd == 0 || fd == 1) {
        f->eax = -1;
        return;
    }
    check_pointer_validity((int *) f->esp + 2, f);
    void *addr = (void *) *((int *) f->esp + 2);
    struct thread * intr_trd = thread_current();
    // Pintos assumes virtual page 0 is not mapped
    if (addr == 0 ||
            // Address must be page aligned
            (int) addr % PAGE_SIZE_BYTES != 0 ||
            // Address must not overlap previously mapped memory
            // TODO: should check the entire address range
            spt_entry_lookup(addr, &intr_trd->spt) != NULL
            // Address must not overlap stack
           /* addr >= f->esp*/) {
        f->eax = -1;
        return;
    }
    int open_files_index = fd - 2;
    if (open_files_index < 0 || open_files_index >= MAX_FILE_DESCRIPTORS) {
        f->eax = -1;
        return;
    }
    struct file *file = intr_trd->open_files[open_files_index];
    if (file == NULL) {
        sys_exit_helper(-1);
    }
    int len = file_length(file);
    if ((char *) addr + ROUND_UP(len, PAGE_SIZE_BYTES) >= f->esp) {
        f->eax = -1;
        return;
    }
    else {
        struct file * file_cp = file_reopen(file);
        if (len == 0) {
            f->eax = -1;
            return;
        }
        int i;
        for (i = 0; i < len; i += PAGE_SIZE_BYTES) {
            struct spt_entry * entry =
                spt_entry_allocate(addr + i, NULL);
            entry->file = file_cp;
            entry->file_ofs = i;
            entry->writable = true;
            entry->mmapid = pg_no(addr);
            entry->file_read_bytes = (PAGE_SIZE_BYTES < len - i) ?
                PAGE_SIZE_BYTES : len - i;
        }

        f->eax = pg_no(addr);
    }

};

/* void munmap (mapid_t mapping)
 * Unmaps the mapping designated by mapping, which must be a mapping ID returned
 * by a previous call to mmap by the same process that has not yet been
 * unmapped.
 */
void sys_munmap(struct intr_frame *f) {
    check_pointer_validity((int *) f->esp + 1, f);
    mapid_t mapping = (mapid_t) *((int *) f->esp + 1);
    sys_munmap_helper(mapping);
};
