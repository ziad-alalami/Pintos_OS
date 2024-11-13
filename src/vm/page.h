#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "userprog/pagedir.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "lib/kernel/list.h"
#include "filesys/file.h"
#include <stdbool.h>


enum page_type{
	PAGE_FILE, PAGE_ELF, PAGE_ANON
};

struct vm_entry{
	struct list_elem list_elem;
        enum page_type type; 
	bool is_write;
	size_t swap_slot;
	void* vaddr; // The address of the page, and the VPN is found from it
	struct file *file;
	int offset;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool is_swapped;
	int swap_index; // Which bitmap index in swap table contains this page, if none, it is -1
};

struct vm_entry * vm_entry_init(void *, enum page_type, bool,struct file *, unsigned, uint32_t, uint32_t);

struct vm_entry *vm_entry_find(void*);

void free_vm_list(struct list *);

bool load_file(void *, struct vm_entry *);

#endif /* VM_PAGE_H */

