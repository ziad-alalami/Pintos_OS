#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "userprog/pagedir.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "lib/kernel/list.h"
#include "filesys/file.h"
#include "vm/page.h"
#include <stdbool.h>
struct page
{
        void* phys_address;
        struct vm_entry *vme;
        struct thread *thread;
        struct list_elem list_elem;
};

void lru_list_init(void);
void lru_insert(struct page*);
void lru_remove(struct page*);
struct page* allocate_page_struct(void*, struct vm_entry*);
void remove_page_struct(struct page*);
void free_lru_list(void);
struct page* get_victim(void);

#endif /*VM_FRAME_H */
