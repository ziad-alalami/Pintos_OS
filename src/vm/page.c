#include "userprog/pagedir.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "lib/kernel/list.h"
#include "filesys/file.h"
#include "page.h"
#include "threads/malloc.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

struct vm_entry * vm_entry_init(void *vaddr, enum page_type type, bool writeable,struct file *file, unsigned offset, uint32_t read_bytes, uint32_t zero_bytes)
{
        struct vm_entry* vme = malloc(sizeof(struct vm_entry));

        if(vme == NULL)
                return NULL;

        struct thread* cur = thread_current();
        vme -> vaddr = vaddr;
        vme -> type = type;
        vme -> swap_slot = -1; // Still not allocated in swap slot
        vme -> is_write = writeable;
        vme -> file = file;
        vme -> offset = offset;
        vme -> read_bytes = read_bytes;
        vme -> zero_bytes = zero_bytes;
	vme -> is_swapped = false;
	vme -> swap_index = -1;
        list_push_back(&cur->vm_list, &vme->list_elem);
        return vme;
}

struct vm_entry *vm_entry_find(void *vaddr) {
    struct list_elem *e;
    void* page_addr = pg_round_down(vaddr);
    struct list *vm_list = &thread_current()->vm_list;
    for (e = list_begin(vm_list); e != list_end(vm_list); e = list_next(e)) {
        struct vm_entry *vme = list_entry(e, struct vm_entry, list_elem);
        if (vme->vaddr == page_addr) {
            return vme;
        }
    }
    return NULL;
}
void free_vm_list(struct list *vm_list) {
    if(vm_list == NULL)
	    return;

    struct list_elem *e;
    while (!list_empty(vm_list)){
        e = list_pop_front(vm_list);
        struct vm_entry *vme = list_entry(e, struct vm_entry, list_elem);
	pagedir_clear_page(thread_current()->pagedir, vme->vaddr);
	free(vme);
    }
  
}

bool load_file(void *kaddr, struct vm_entry * vme)
{
	if(vme->read_bytes == 0)
		return true;

	file_seek(vme->file, vme->offset);
    off_t read_bytes = file_read(vme->file, kaddr, vme->read_bytes);
    if(read_bytes != vme->read_bytes)
        return false;

    memset(kaddr + vme->read_bytes, 0, vme->zero_bytes);
    return true;
}
