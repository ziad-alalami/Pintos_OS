#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <stdlib.h>
#include "userprog/process.h"
#include "threads/malloc.h"

static void syscall_handler (struct intr_frame *);

static struct lock filesys_lock;

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_num = *((int*)f->esp);

  // Address of first argument - may or may not be used
  void* addr1 = f->esp + sizeof(int);
  switch (syscall_num) {
    case SYS_HALT:
      {
      halt();
      break;
      }
    case SYS_EXIT:
      {
      if (!validate_pointer(addr1)) exit_(-1);
      int status = *(int*)(addr1);
      exit_(status);
      break;
      }
    case SYS_EXEC:
      {
      if (!validate_pointer(addr1)) exit_(-1);
      char* cmd_line = *(char**)(addr1);
      f->eax = exec(cmd_line);
      break;
      }
    case SYS_WAIT:
      {
      if (!validate_pointer(addr1)) exit_(-1);
      pid_t pid = *(pid_t*)(addr1);
      f->eax = wait(pid);
      break;
      }
    case SYS_CREATE:
      {
      void* addr2 = addr1 + sizeof(char*);
      if (!validate_pointer(addr1) || !validate_pointer(addr2)) exit_(-1);
      const char *file = *(char **)(addr1);
      unsigned initial_size = *(unsigned *)(addr2);
      f->eax = create(file, initial_size);
      break;
      }
    case SYS_REMOVE:
      {
      if (!validate_pointer(addr1)) exit_(-1);
      const char* file = *(char **)(addr1);
      f->eax = remove(file);
      break;
      }
    case SYS_OPEN:
      {
      if (!validate_pointer(addr1)) exit_(-1);
      const char* file = *(char **)(addr1);
      f->eax = open(file);
      break;
      }
    case SYS_FILESIZE:
      {
      if (!validate_pointer(addr1)) exit_(-1);
      int fd = *(int*)(addr1);
      f->eax = filesize(fd);
      break;
      }
    case SYS_READ:
      {
      void* addr2 = addr1 + sizeof(int);
      void* addr3 = addr2 + sizeof(void*);
      if (!validate_pointer(addr1) || !validate_pointer(addr2) || !validate_pointer(addr3)) exit_(-1);
      int fd = *(int*)(addr1);
      const void* buffer = *(void**)(addr2);
      unsigned size = *(unsigned*)(addr3);
      f->eax = read(fd, buffer, size);
      break;
      }
    case SYS_WRITE:
      {
      void* addr2 = addr1 + sizeof(int);
      void* addr3 = addr2 + sizeof(void*);
      if (!validate_pointer(addr1) || !validate_pointer(addr2) || !validate_pointer(addr3)) exit_(-1);
      int fd = *(int*)(addr1);
      const void* buffer = *(void**)(addr2);
      unsigned size = *(unsigned*)(addr3);
      f->eax = write(fd, buffer, size);
      break;
      }
    case SYS_SEEK:
      {
      void* addr2 = addr1 + sizeof(int);
      if (!validate_pointer(addr1) || !validate_pointer(addr2)) exit_(-1);
      int fd = *(int*) (addr1);
      unsigned position = *(unsigned*)(addr2);
      seek(fd, position);
      break;
      }
    case SYS_TELL:
      {
      if (!validate_pointer(addr1)) exit_(-1);
      int fd = *(int*)(addr1);
      f->eax = tell(fd);
      break;
      }
    case SYS_CLOSE:
      {
      if (!validate_pointer(addr1)) exit_(-1);
      int fd = *(int*)(addr1);
      close(fd);
      break;
      }
    case SYS_PIPE:
      {
      if (!validate_pointer(addr1)) exit_(-1);
      int* fds = *(int **)(addr1);
      f->eax= pipe(fds);
      break;
      }
  }
}

/*
 * Return false if pointer is null, not mapped to virtual memory, or part of kernel memory
 */
bool validate_pointer(const void* pointer)
{
  // TODO should it be an || between is_user_vaddr and pagedir_get_page?
	return pointer != NULL && is_user_vaddr(pointer) && pagedir_get_page(thread_current()->pagedir, pointer) != NULL;
}

int get_next_fd()
{
	struct thread* cur = thread_current();
	if(cur->stdin_closed && cur->fdt[0] == NULL)
		return 0;
	if(cur->stdout_closed && cur->fdt[1] == NULL)
		return 1;
	for(int i = 2; i < 64; ++i)
		if(cur->fdt[i] == NULL)
			return i;
	return -1; //Table is full
}

void halt() {
  shutdown_power_off();
}

void exit_(int status) {
  struct thread* cur = thread_current();
  if (cur->pd != NULL) {
    cur->pd->exit_status = status;
    cur->pd->is_exited = true;
    sema_up(&cur->pd->wait_sema);
  }
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

int write(int fd, const void *buffer, unsigned size) {
  if (!validate_pointer(buffer)) exit_(-1);
  if(fd < 0 || fd > 63) return -1;

  struct thread* cur = thread_current();

  if (fd == 0 && !cur->stdin_closed) exit_(-1);

  if (fd == 1 && !cur->stdout_closed) {
    lock_acquire(&filesys_lock);
    putbuf(buffer, size);
    lock_release(&filesys_lock);
    return size;
  }

  struct file* file_ = cur->fdt[fd];
  if (file_ == NULL) return -1;

  lock_acquire(&filesys_lock);
  int result = file_write(file_, buffer, size);
  lock_release(&filesys_lock);

  return result;
}

pid_t exec(const char* cmd_line) {
  pid_t pid = process_execute(cmd_line);

  if (pid == TID_ERROR) return -1;

  struct thread* cur = thread_current();
  struct list_elem* child_elem = NULL;
  for (struct list_elem* e = list_begin(&cur->children); e != list_end(&cur->children); e = list_next(e)) {
    struct process_descriptor* pd = list_entry(e, struct process_descriptor, elem);
    if (pd->tid == pid) {
      child_elem = e;
      break;
    }
  }

  if (child_elem == NULL) return -1;

  struct process_descriptor* child = list_entry(child_elem, struct process_descriptor, elem);

  sema_down(&child->exec_sema);

  // Return child->tid as it will be updated to -1 if loading fails
  return child->tid;
}

int wait(pid_t pid) {
  return process_wait(pid);
}

int read(int fd, const void *buffer, unsigned size)
{
	if(!validate_pointer(buffer))
	  exit_(-1);

	if (fd < 0 || fd > 63) return -1;

  struct thread* cur = thread_current();

  if (fd == 1 && !cur->stdout_closed) exit_(-1);

  if (fd == 0 && !cur->stdin_closed) {
    lock_acquire(&filesys_lock);
    input_getc();
    lock_release(&filesys_lock);
    return size;
  }

  struct file* file_ = cur->fdt[fd];
  if (file_ == NULL) return -1;

  lock_acquire(&filesys_lock);
  int result = file_read(file_, buffer, size);
  lock_release(&filesys_lock);

  return result;
}

unsigned int tell(int fd)
{
	if(fd < 0 || fd > 63)
		return -1;
	struct file* file_ = thread_current()->fdt[fd];
	if(file_ == NULL) return -1;

	lock_acquire(&filesys_lock);
	unsigned pos = file_tell(file_);
	lock_release(&filesys_lock);
	return pos;
}

int filesize(int fd)
{
  if(fd < 0 || fd > 63)
    return -1;
  struct file* file_ = thread_current()->fdt[fd];
  if(file_ == NULL) return -1;

  lock_acquire(&filesys_lock);
  int size = file_length(file_);
  lock_release(&filesys_lock);
	return size;
}

void seek(int fd, unsigned position)
{
	if(fd < 0 || fd > 63) return;

	struct file* file_ = thread_current()->fdt[fd];
	if(file_ == NULL) return;
  lock_acquire(&filesys_lock);
	file_seek(file_, position);
  lock_release(&filesys_lock);
}

bool create(const char* file, unsigned initial_size)
{
	if(!validate_pointer(file))
		exit_(-1); //Malicious pointer

	lock_acquire(&filesys_lock);
	bool created = filesys_create(file, initial_size);
	lock_release(&filesys_lock);

	return created;
}

bool remove(const char* file)
{
	if(!validate_pointer(file))
		exit_(-1);
	lock_acquire(&filesys_lock);
	bool removed = filesys_remove(file);
	lock_release(&filesys_lock);
	return removed;
}

int open(const char* file)
{
	if(!validate_pointer(file))
		exit_(-1);
	
	struct thread* cur = thread_current();
	int next_fd = get_next_fd();

	if(next_fd == -1) // FDT is full
	  return -1;

	lock_acquire(&filesys_lock);
	struct file* file_ = filesys_open(file);
	lock_release(&filesys_lock);

	if(file_ == NULL)
		return -1;

	cur->fdt[next_fd] = file_;

  return next_fd;	
}

void close(int fd)
{
	if(fd < 0 || fd > 63)
		exit_(-1);
	
	struct thread* cur = thread_current();

  if (fd == 0 && !cur->stdin_closed)
    cur->stdin_closed = true;
  else if (fd == 1 && !cur->stdout_closed)
    cur->stdout_closed = true;
  else {
    if(cur->fdt[fd] == NULL)
		  return;
    lock_acquire(&filesys_lock);
    file_close(cur->fdt[fd]);
    lock_release(&filesys_lock);
    cur->fdt[fd] = NULL;
  }
}

int pipe(int* fds)
{
	if(!validate_pointer(fds))
		exit_(-1);

	struct thread* cur = thread_current();
	



	//TODO create a buffer for the pipe and make it circular
	return -1;
}
