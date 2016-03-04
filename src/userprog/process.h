#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "vm/page.h"

tid_t process_execute(const char *file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);
bool load_page_from_spte(struct spt_entry *);
bool allocate_and_install_blank_page(void *);

#endif /* userprog/process.h */

