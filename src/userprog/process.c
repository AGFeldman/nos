#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

static thread_func start_process NO_RETURN;
static bool load(const char *cmdline, void (**eip)(void), void **esp);

/*! Starts a new thread running a user program loaded from FILENAME.  The new
    thread may be scheduled (and may even exit) before process_execute()
    returns.  Returns the new process's thread id, or TID_ERROR if the thread
    cannot be created. */
tid_t process_execute(const char *file_name) {
    char *fn_copy;
    tid_t tid;

    /* Make a copy of FILE_NAME.
       Otherwise there's a race between the caller and load(). */
    fn_copy = palloc_get_page(0);
    if (fn_copy == NULL)
        return TID_ERROR;
    strlcpy(fn_copy, file_name, PGSIZE);

    /* Extract the first token, `file_name_only`, from `file_name`, since
       `file_name` really contains the file name followed by arguments. */
    // TODO(agf): This uses a lot of pages and copying, and duplicates some
    // work from load().
    char * fn_copy2 = palloc_get_page(0);
    if (fn_copy2 == NULL)
        return TID_ERROR;
    strlcpy(fn_copy2, file_name, PGSIZE);
    char * save_ptr_page = palloc_get_page(0);
    if (save_ptr_page == NULL)
        return TID_ERROR;
    char ** save_ptr = &save_ptr_page;
    char * file_name_only = strtok_r(fn_copy2, " ", save_ptr);
    if (file_name_only == NULL)
        return TID_ERROR;
    // TODO(agf): Eventually need to palloc_free_page(fn_copy2)

    /* Create a new thread to execute FILE_NAME. */
    // load_sema is used to block until the new thread has loaded the
    // executable.
    // loaded_correctly becomes true only if the thread successfully loaded
    // the executable.
    struct semaphore load_sema;
    sema_init(&load_sema, 0);
    bool loaded_correctly = false;
    uint32_t aux[3] = {(uint32_t)fn_copy, (uint32_t)&load_sema,
                       (uint32_t)&loaded_correctly};
    tid = thread_create(file_name_only, PRI_DEFAULT, start_process, aux);
    sema_down(&load_sema);
    if (tid == TID_ERROR) {
        palloc_free_page(fn_copy);
    }
    if (loaded_correctly != 1) {
        return TID_ERROR;
    }
    return tid;
}

/*! A thread function that loads a user process and starts it running. */
static void start_process(void * aux) {
    char *file_name = (char *)((uint32_t *) aux)[0];
    struct semaphore *load_sema = (struct semaphore *)((uint32_t *) aux)[1];
    bool * loaded_correctly = (bool *)((uint32_t *) aux)[2];
    struct intr_frame if_;
    bool success;

    /* Initialize interrupt frame and load executable. */
    memset(&if_, 0, sizeof(if_));
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;
    success = load(file_name, &if_.eip, &if_.esp);

    if (success) {
        *loaded_correctly = true;
    }

    sema_up(load_sema);

    /* If load failed, quit. */
    palloc_free_page(file_name);
    if (!success) {
        struct file * executable = thread_current()->executable;
        if (executable != NULL) {
            file_allow_write(executable);
            thread_current()->executable = NULL;
        }
        thread_exit();
    }

    /* Start the user process by simulating a return from an
       interrupt, implemented by intr_exit (in
       threads/intr-stubs.S).  Because intr_exit takes all of its
       arguments on the stack in the form of a `struct intr_frame',
       we just point the stack pointer (%esp) to our stack frame
       and jump to it. */
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
    NOT_REACHED();
}

/*! Waits for thread TID to die and returns its exit status.  If it was
    terminated by the kernel (i.e. killed due to an exception), returns -1.
    If TID is invalid or if it was not a child of the calling process, or if
    process_wait() has already been successfully called for the given TID,
    returns -1 immediately, without waiting. */
int process_wait(tid_t child_tid UNUSED) {
    struct thread * current = thread_current();
    struct thread * t;
    struct list * child_list = &current->child_list;
    struct list_elem *e;
    for (e = list_begin(child_list); e != list_end(child_list);
            e = list_next(e)) {
        t = list_entry(e, struct thread, child_list_elem);
        if (t->tid == child_tid) {
            lock_acquire(&t->life_lock);
            list_remove(e);
            // TODO(agf): should be able to free the page for t now, e.g. with
            // a call to palloc_free_page(t)
            return t->exit_status;
        }
    }

    return -1;
}

/*! Free the current process's resources. */
void process_exit(void) {
    struct thread *cur = thread_current();
    uint32_t *pd;

    if (cur->executable != NULL) {
        file_close(cur->executable);
        cur->executable = NULL;
    }

    /* Destroy the current process's page directory and switch back
       to the kernel-only page directory. */
    pd = cur->pagedir;
    if (pd != NULL) {
        /* Correct ordering here is crucial.  We must set
           cur->pagedir to NULL before switching page directories,
           so that a timer interrupt can't switch back to the
           process page directory.  We must activate the base page
           directory before destroying the process's page
           directory, or our active page directory will be one
           that's been freed (and cleared). */
        cur->pagedir = NULL;
        pagedir_activate(NULL);
        pagedir_destroy(pd);
    }
}

/*! Sets up the CPU for running user code in the current thread.
    This function is called on every context switch. */
void process_activate(void) {
    struct thread *t = thread_current();

    /* Activate thread's page tables. */
    pagedir_activate(t->pagedir);

    /* Set thread's kernel stack for use in processing interrupts. */
    tss_update();
}

/*! We load ELF binaries.  The following definitions are taken
    from the ELF specification, [ELF1], more-or-less verbatim.  */

/*! ELF types.  See [ELF1] 1-2. @{ */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;
/*! @} */

/*! For use with ELF types in printf(). @{ */
#define PE32Wx PRIx32   /*!< Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /*!< Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /*!< Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /*!< Print Elf32_Half in hexadecimal. */
/*! @} */

/*! Executable header.  See [ELF1] 1-4 to 1-8.
    This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
};

/*! Program header.  See [ELF1] 2-2 to 2-4.  There are e_phnum of these,
    starting at file offset e_phoff (see [ELF1] 1-6). */
struct Elf32_Phdr {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

/*! Values for p_type.  See [ELF1] 2-3. @{ */
#define PT_NULL    0            /*!< Ignore. */
#define PT_LOAD    1            /*!< Loadable segment. */
#define PT_DYNAMIC 2            /*!< Dynamic linking info. */
#define PT_INTERP  3            /*!< Name of dynamic loader. */
#define PT_NOTE    4            /*!< Auxiliary info. */
#define PT_SHLIB   5            /*!< Reserved. */
#define PT_PHDR    6            /*!< Program header table. */
#define PT_STACK   0x6474e551   /*!< Stack segment. */
/*! @} */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. @{ */
#define PF_X 1          /*!< Executable. */
#define PF_W 2          /*!< Writable. */
#define PF_R 4          /*!< Readable. */
/*! @} */

static bool setup_stack(void **esp);
static bool validate_segment(const struct Elf32_Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);
static bool load_segment_lazy(struct file *file, off_t ofs, uint8_t *upage,
                              uint32_t read_bytes, uint32_t zero_bytes,
                              bool writable);

/*! Loads an ELF executable from FILE_NAME_AND_ARGS into the current thread.
    Stores the executable's entry point into *EIP and its initial stack pointer
    into *ESP.  Returns true if successful, false otherwise. */
bool load(const char *file_name_and_args, void (**eip) (void), void **esp) {
    struct thread *t = thread_current();
    struct Elf32_Ehdr ehdr;
    struct file *file = NULL;
    off_t file_ofs;
    bool success = false;
    int i;

    /* Allocate and activate page directory. */
    t->pagedir = pagedir_create();
    if (t->pagedir == NULL)
        goto done;
    process_activate();

    // Make a copy of file_name_and_args so that this copy can be non-const
    char * fnaa_copy = palloc_get_page(0);
    if (fnaa_copy == NULL) {
        printf("load: palloc failure\n");
        goto done;
    }
    strlcpy(fnaa_copy, file_name_and_args, PGSIZE);
    char * save_ptr_page = palloc_get_page(PAL_USER | PAL_ZERO);
    if (save_ptr_page == NULL) {
        printf("load: palloc failure\n");
        goto done;
    }
    char ** save_ptr = &save_ptr_page;
    // File name is the first token in file_name_and_args
    char * file_name = strtok_r(fnaa_copy, " ", save_ptr);
    // TODO(agf): Should eventually free save_ptr_page

    /* Open executable file. */
    filesys_lock_acquire();
    file = filesys_open(file_name);
    if (file != NULL) {
        file_deny_write(file);
        thread_current()->executable = file;
    }
    filesys_lock_release();
    if (file == NULL) {
        printf("load: %s: open failed\n", file_name);
        goto done;
    }

    /* Read and verify executable header. */
    filesys_lock_acquire();
    if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr ||
        memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2 ||
        ehdr.e_machine != 3 || ehdr.e_version != 1 ||
        ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024) {
        filesys_lock_release();
        printf("load: %s: error loading executable\n", file_name);
        goto done;
    }
    filesys_lock_release();

    /* Read program headers. */
    file_ofs = ehdr.e_phoff;
    for (i = 0; i < ehdr.e_phnum; i++) {
        struct Elf32_Phdr phdr;

        if (file_ofs < 0 || file_ofs > file_length(file))
            goto done;
        filesys_lock_acquire();
        file_seek(file, file_ofs);

        if (file_read(file, &phdr, sizeof phdr) != sizeof phdr) {
            filesys_lock_release();
            goto done;
        }
        filesys_lock_release();

        file_ofs += sizeof phdr;

        switch (phdr.p_type) {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
            /* Ignore this segment. */
            break;

        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
            goto done;

        // TODO(agf): I think around here is where we change to lazily loading
        // the program
        case PT_LOAD:
            if (validate_segment(&phdr, file)) {
                bool writable = (phdr.p_flags & PF_W) != 0;
                uint32_t file_page = phdr.p_offset & ~PGMASK;
                uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
                uint32_t page_offset = phdr.p_vaddr & PGMASK;
                uint32_t read_bytes, zero_bytes;
                if (phdr.p_filesz > 0) {
                    /* Normal segment.
                       Read initial part from disk and zero the rest. */
                    read_bytes = page_offset + phdr.p_filesz;
                    zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) -
                                 read_bytes);
                }
                else {
                    /* Entirely zero.
                       Don't read anything from disk. */
                    read_bytes = 0;
                    zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
                }
                if (!load_segment_lazy(file, file_page, (void *) mem_page,
                                       read_bytes, zero_bytes, writable))
                    goto done;
            }
            else {
                goto done;
            }
            break;
        }
    }

    /* Set up stack. */
    if (!setup_stack(esp))
        goto done;

    /* Finish setting up the stack */
    // Push the filename and arguments to the stack in the order that they
    // appear
    // TODO(agf): Instead of using PGSIZE here, should really use remaining
    // PGSIZE
    // TODO(agf): This entire block uses pointer arithmetic on *esp, which is
    // of type void*. We should avoid pointer arithmetic on void*.
    char * token = file_name;
    int total_length = 0;
    int num_tokens = 0;
    while (token) {
        num_tokens += 1;
        int length = strlen(token);
        int length_including_nullterm = length + 1;
        total_length += length_including_nullterm;
        *esp -= length_including_nullterm;
        int copied_length = strlcpy(*esp, token, PGSIZE);
        ASSERT(copied_length == length);
        ASSERT(*((char *)(*esp + length)) == '\0');
        // TODO(agf): Make sure that this properly handles multiple adjacent
        // spaces
        token = strtok_r(NULL, " ", save_ptr);
    }
    char * argstart = *esp;
    // Word-align the stack
    int word_align_length = (4 - total_length % 4) % 4;
    *esp -= word_align_length;
    // Push the last item of argv[], which should be 0
    *esp -= 4;
    // Push the rest of argv. The rest of argv are pointers to the filename and
    // args strings that we already pushed to the stack. So, iterate through
    // the stack from top to bottom in order to find these pointers, and push
    // them in the order that they are found.
    ASSERT(*argstart != '\0');
    *esp -= 4;
    *((char **)(*esp)) = argstart;
    int ntokens_processed = 1;
    char * p = argstart;
    while (ntokens_processed < num_tokens) {
        if (*p == '\0') {
            *esp -= 4;
            *((char **)(*esp)) = p + 1;
            ASSERT(*(p + 1) != '\0');
            ntokens_processed += 1;
        }
        p++;
    }
    // TODO(agf): Assert something like p == orig_esp
    ASSERT(ntokens_processed == num_tokens);
    // Push the argv pointer itself
    *esp -= 4;
    *((char ***)(*esp)) = (char **)(*esp + 4);
    // Push argc
    *esp -= 4;
    *((int *)(*esp)) = num_tokens;
    // Push fake return address
    *esp -= 4;

    /* Start address. */
    *eip = (void (*)(void)) ehdr.e_entry;

    success = true;

done:
    /* We arrive here whether the load is successful or not. */
    // `file` gets closed in process_exit()
    return success;
}

/* load() helpers. */

static bool install_page(void *upage, void *kpage, bool writable);
/*! Checks whether PHDR describes a valid, loadable segment in
    FILE and returns true if so, false otherwise. */
static bool validate_segment(const struct Elf32_Phdr *phdr, struct file *file) {
    /* p_offset and p_vaddr must have the same page offset. */
    if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
        return false;

    /* p_offset must point within FILE. */
    filesys_lock_acquire();
    if (phdr->p_offset > (Elf32_Off) file_length(file)) {
        filesys_lock_release();
        return false;
    }
    filesys_lock_release();

    /* p_memsz must be at least as big as p_filesz. */
    if (phdr->p_memsz < phdr->p_filesz)
        return false;

    /* The segment must not be empty. */
    if (phdr->p_memsz == 0)
        return false;

    /* The virtual memory region must both start and end within the
       user address space range. */
    if (!is_user_vaddr((void *) phdr->p_vaddr))
        return false;
    if (!is_user_vaddr((void *) (phdr->p_vaddr + phdr->p_memsz)))
        return false;

    /* The region cannot "wrap around" across the kernel virtual
       address space. */
    if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
        return false;

    /* Disallow mapping page 0.
       Not only is it a bad idea to map page 0, but if we allowed it then user
       code that passed a null pointer to system calls could quite likely panic
       the kernel by way of null pointer assertions in memcpy(), etc. */
    if (phdr->p_vaddr < PGSIZE)
        return false;

    /* It's okay. */
    return true;
}

/*! Loads a segment starting at offset OFS in FILE at address UPAGE.  In total,
    READ_BYTES + ZERO_BYTES bytes of virtual memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

    The pages initialized by this function must be writable by the user process
    if WRITABLE is true, read-only otherwise.

    Return true if successful, false if a memory allocation error or disk read
    error occurs. */
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable) {
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);

    filesys_lock_acquire();
    file_seek(file, ofs);
    filesys_lock_release();
    while (read_bytes > 0 || zero_bytes > 0) {
        /* Calculate how to fill this page.
           We will read PAGE_READ_BYTES bytes from FILE
           and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        /* Get a page of memory. */
        uint8_t *kpage = palloc_get_page(PAL_USER);
        if (kpage == NULL)
            return false;

        /* Load this page. */
        filesys_lock_acquire();
        if (file_read(file, kpage, page_read_bytes) != (int) page_read_bytes) {
            filesys_lock_release();
            palloc_free_page(kpage);
            return false;
        }
        filesys_lock_release();
        memset(kpage + page_read_bytes, 0, page_zero_bytes);

        /* Add the page to the process's address space. */
        if (!install_page(upage, kpage, writable)) {
            palloc_free_page(kpage);
            return false;
        }

        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
    }
    return true;
}

static bool load_segment_lazy(struct file *file, off_t ofs, uint8_t *upage,
                              uint32_t read_bytes, uint32_t zero_bytes,
                              bool writable) {
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);
    // TODO(agf): Do a more comprehensive review of concurrent access to SPT
    while (read_bytes > 0 || zero_bytes > 0) {
        // Calculate how to fill this page.
        // We will read PAGE_READ_BYTES bytes from FILE
        // and zero the final PAGE_ZERO_BYTES bytes.
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        // TODO(agf): Free this later
        struct spt_entry * spte = spt_entry_allocate(upage);
        ASSERT(spte != NULL);
        spte->file = file;
        spte->file_ofs = ofs;
        spte->file_read_bytes = page_read_bytes;
        spte->writable = writable;

        // Advance
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        ofs += page_read_bytes;
    }
    return true;
}

// If |spte| describes a page from a file, then obtain a page of memory, copy
// the data from that page of the file into the new page of memory, and
// map the memory into the user's address space. If all this was successful,
// return true. Otherwise, return false.
bool load_page_from_spte(struct spt_entry *spte) {
    if (spte == NULL || spte->file == NULL) {
        return false;
    }
    // Here is an example situation in which filesys_lock could already be
    // held:
    // filesys_lock_acquire(), then file_read(), which causes a page fault,
    // which causes load_page_from_spte() to be called.
    bool already_held = filesys_lock_held();
    if (!already_held) {
        filesys_lock_acquire();
    }
    file_seek(spte->file, spte->file_ofs);
    if (!already_held) {
        filesys_lock_release();
    }
    // Get a page of memory
    uint8_t *kpage = palloc_get_page(PAL_USER);
    if (kpage == NULL) {
        return false;
    }
    // Load this page
    if (spte->file_read_bytes != 0) {
        if (!already_held) {
            filesys_lock_acquire();
        }
        if (file_read(spte->file, kpage, spte->file_read_bytes) !=
                (int) spte->file_read_bytes) {
            if (!already_held) {
                filesys_lock_release();
            }
            palloc_free_page(kpage);
            return false;
        }
        if (!already_held) {
            filesys_lock_release();
        }
    }
    size_t spte_zero_bytes = PGSIZE - spte->file_read_bytes;
    memset(kpage + spte->file_read_bytes, 0, spte_zero_bytes);
    // Add the page to the process's address space
    if (!install_page(spte->key.addr, kpage, spte->writable)) {
        palloc_free_page(kpage);
        return false;
    }
    return true;
}

/*! Create a minimal stack by mapping a zeroed page at the top of
    user virtual memory. */
static bool setup_stack(void **esp) {
    uint8_t *kpage;
    bool success = false;

    kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    if (kpage != NULL) {
        success = install_page(((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
        if (success)
            *esp = PHYS_BASE;
        else
            palloc_free_page(kpage);
    }
    return success;
}

// Gets a frame from the user pool, fills it with zeros, and installs it in
// the user virtual address mapping at the address indicated by |upage| (with
// appropriate alignment; |upage| is rounded down).
bool allocate_and_install_blank_page(void *upage) {
    void *kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    if (kpage == NULL) {
        return false;
    }
    bool install_success = install_page(pg_round_down(upage), kpage, true);
    if (!install_success) {
        palloc_free_page(kpage);
    }
    return install_success;
}

/*! Adds a mapping from user virtual address UPAGE to kernel
    virtual address KPAGE to the page table.
    If WRITABLE is true, the user process may modify the page;
    otherwise, it is read-only.
    UPAGE must not already be mapped.
    KPAGE should probably be a page obtained from the user pool
    with palloc_get_page().
    Returns true on success, false if UPAGE is already mapped or
    if memory allocation fails. */
static bool install_page(void *upage, void *kpage, bool writable) {
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
       address, then map our page there. */
    return (pagedir_get_page(t->pagedir, upage) == NULL &&
            pagedir_set_page(t->pagedir, upage, kpage, writable));
}

