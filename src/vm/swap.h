#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/page.h"

void swap_init(void);
bool swap_out(struct vm_entry*);
void swap_in(struct vm_entry*);
#endif /*VM_SWAP_H*/
