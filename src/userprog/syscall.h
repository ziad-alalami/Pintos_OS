#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "threads/thread.h"

void syscall_init (void);
bool validate_pointer(const void*);
void halt(void);
void exit_(int);
int write(int, const void *, unsigned);
pid_t exec(const char*);
int wait(pid_t);

#endif /* userprog/syscall.h */
