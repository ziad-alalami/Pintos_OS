#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"

void syscall_init (void);
bool validate_pointer(const void*);
void halt(void);
void exit_(int);
int write(int, const void *, unsigned);

#endif /* userprog/syscall.h */
