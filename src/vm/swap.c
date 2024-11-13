#include "devices/block.h"
#include "vm/page.h"
#include "threads/interrupt.h"
#include "vm/swap.h"
#include <stdbool.h>

static struct block* swap_block; //Points to the swap block that writes to the swap region in memory
static block_sector_t block_sector_size; // Number of sectors in the swap block
static int allowed_swap_pages; // Number of possible pages
static bool* page_bitmap; // Index i is true if the 8th page in swap memory is occupied

void swap_init(void)
{
	swap_block = block_get_role(BLOCK_SWAP);
	ASSERT(swap_block != NULL); // Shouldnt happen
	block_sector_size = block_size(swap_block);
	allowed_swap_pages = (int) (block_sector_size / 8);
	page_bitmap = malloc(sizeof(bool) * allowed_swap_pages);
	
}

static int get_free_slot_index()
{
	for(int i = 0; i < allowed_swap_pages; i++)
		if(page_bitmap[i] == false)
			return i;
	return -1;
}

bool swap_out(struct vm_entry* vme) //Returns false when swap table is full
{
	
	enum intr_level old_level = intr_disable();

	if(vme->type == PAGE_FILE)
	{
		off_t write_bytes = file_write_at(vme->file,vme->vaddr,vme->read_bytes, vme->offset);
		return write_bytes == vme->read_bytes;

	}
	int next_page_index = get_free_slot_index();
	if(next_page_index == -1)
		return false;

	void* vaddr = vme->vaddr;
	ASSERT(vaddr != NULL); //Shouldnt happen
	int block_number = 0;
	for(int i = next_page_index * 8; i < (next_page_index + 1) * 8; i++){
		block_write(swap_block, i, vaddr + (BLOCK_SECTOR_SIZE * block_number));
		block_number += 1;
	}
	
	page_bitmap[next_page_index] = true;
	vme->is_swapped = true;
	vme->swap_index = next_page_index;
	intr_set_level(old_level);
	return true;
}

void swap_in(struct vm_entry* vme)
{
	enum intr_level old_level = intr_disable();

	ASSERT(vme->swap_index != -1);
	int page_index = vme->swap_index;
	void* vaddr = vme->vaddr;

	ASSERT(vaddr != NULL) //Shouldnt happen
	int block_number = 0;
	for(int i = page_index * 8; i < (page_index + 1) * 8; i++)
	{
		block_read(swap_block, i, vaddr + (BLOCK_SECTOR_SIZE * block_number));
		block_number += 1;
	}
	page_bitmap[page_index] = false;
	vme->is_swapped = false;
	vme->swap_index = -1;
	intr_set_level(old_level);

}



