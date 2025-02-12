#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "threads/thread.h"

void syscall_init (void);
bool validate_pointer(const void*);
int get_next_fd(void);
void halt(void);
void exit_(int);
int write(int, const void *, unsigned);
pid_t exec(const char*);
int wait(pid_t);
unsigned int tell(int);
int read(int, const void *, unsigned);
bool create(const char*, unsigned);
bool remove(const char*);
int filesize(int);
void seek(int, unsigned);
void close(int);
int open(const char*);
int pipe(int*);
#endif /* userprog/syscall.h */
