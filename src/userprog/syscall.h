#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"

void syscall_init (void);
void halt(void);
void exit_(struct intr_frame*);
void exec(void);
void wait(void);

#endif /* userprog/syscall.h */
