#include "vm/frame.h"
#include "threads/interrupt.h"
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include <stdbool.h>
//Includes all pages allocated by the system
static struct list lru_list;

void lru_list_init()
{
	list_init(&lru_list);
}

void lru_insert(struct page* page)
{
	if(page == NULL)
		return;
	enum intr_level old_level = intr_disable();
	list_push_front(&lru_list,&page->list_elem);
	intr_set_level(old_level);

}

void lru_remove(struct page* page)
{
	if(page == NULL)
		return;
	enum intr_level old_level = intr_disable();
	list_remove(&page->list_elem);
	intr_set_level(old_level);

}

//Creates page struct and adds it to the LRU List
struct page* allocate_page_struct(void *physical_address, struct vm_entry* vme)
{
	struct page* page = malloc(sizeof(struct page));
	if(page == NULL)
		return NULL;

	page->phys_address = physical_address;
	ASSERT(vme != NULL);
	page->vme = vme;
	page->thread = thread_current();

	lru_insert(page);

	return page;
}

void remove_page_struct(struct page* page)
{
	if(page == NULL)
		return;
	lru_remove(page);
	free(page);
}

void free_lru_list()
{
   if(&lru_list == NULL)
            return;

    struct list_elem *e;
    while (!list_empty(&lru_list)){

        e = list_pop_front(&lru_list);
        struct page *page = list_entry(e, struct page, list_elem);
	remove_page_struct(page);
       
    }

}

struct page* get_victim()
{
	struct list_elem* e;
	while (true)
	{
		for(e = list_begin(&lru_list); e != list_end(&lru_list); e = list_next(e))
		{
			struct page* page = list_entry(e, struct page, list_elem);
	       		 bool clock_bit = pagedir_is_accessed(page->thread->pagedir, page->vme->vaddr);
			
			if(clock_bit == 0)
				return page;
			
			else
				pagedir_set_accessed(page->thread->pagedir,page->vme->vaddr,false);
		}
	}
}

