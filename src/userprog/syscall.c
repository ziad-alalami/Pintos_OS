#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
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

  switch (syscall_num) {
    case SYS_HALT:
	    {
      halt();
      break;
	    }
    case SYS_EXIT:
     {
      int status = *(int*)(f->esp + sizeof(int));
      exit_(status);
      break;
     }
    case SYS_EXEC:
     {
      char* cmd_line = *(char**)(f->esp + sizeof(int));
      f->eax = exec(cmd_line);
      break;
     }
    case SYS_WAIT:
     {
      pid_t pid = *(pid_t*)(f->esp + sizeof(int));
      f->eax = wait(pid);
      break;
     }
    case SYS_CREATE:
      {
      const char *file = *(char **)(f->esp + sizeof(int));
      unsigned initial_size = *(unsigned *)(f->esp + sizeof(int) + sizeof(char*));
      f->eax = create(file, initial_size);
      break;
      }
    case SYS_REMOVE:
      {
       const char* file = *(char **)(f->esp + sizeof(int));
       f->eax = remove(file);
      break;
      }
    case SYS_OPEN:
      {
      const char* file = *(char **)(f->esp + sizeof(int));
      f->eax = open(file);
      break;
      }
    case SYS_FILESIZE:
      {
      int fd = *(int*)(f->esp + sizeof(int));
      f->eax = filesize(fd);
      break;
      }
    case SYS_READ:
      {
      int fd = *(int*)(f->esp + sizeof(int));
      const void* buffer = *(void**)(f->esp + 2 * sizeof(int));
      unsigned size = *(unsigned*)(f->esp + 2 * sizeof(int) + sizeof(void*));
      f->eax = read(fd, buffer, size);
      break;
      }
    case SYS_WRITE:
      {
      int fd = *(int*)(f->esp + sizeof(int));
      const void* buffer = *(void**)(f->esp + 2 * sizeof(int));
      unsigned size = *(unsigned*)(f->esp + 2 * sizeof(int) + sizeof(void*));
      f->eax = write(fd, buffer, size);
      break;
      }
    case SYS_SEEK:
      {
      int fd = *(int*) (f->esp + sizeof(int));
      unsigned position = *(unsigned*)(f->esp + 2 * sizeof(int));
      seek(fd, position);
      break;
      }
    case SYS_TELL:
      {
      int fd = *(int*)(f->esp + sizeof(int));
      f->eax = tell(fd);
      break;
      }
    case SYS_CLOSE :
      {
      int fd = *(int*)(f->esp + sizeof(int));
      close(fd);
      break;
      }
    case SYS_PIPE :
      {
	      int* fds = *(int **)(f->esp + sizeof(int));
	      f->eax= pipe(fds);
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
	for(int i = 2; i < 64; i++)
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
    sema_up(&cur->pd->sema);
  }
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

int write(int fd, const void *buffer, unsigned size) {
  if (!validate_pointer(buffer)) exit_(-1);
  if(fd < 0 || fd > 63) return -1;

  if(fd == 0) exit_(-1);

  if (fd == 1 && !thread_current()->stdout_closed) {
    lock_acquire(&filesys_lock);
    putbuf(buffer, size);
    lock_release(&filesys_lock);
    return size;
  }

  struct file* file_ = thread_current()->fdt[fd];
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

  sema_down(&child->sema);

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

 if(fd == 1) exit_(-1);

 if (fd == 0 && !thread_current() -> stdin_closed) {
    lock_acquire(&filesys_lock);
    input_getc();
    lock_release(&filesys_lock);
    return size;
  }

  struct file* file_ = thread_current()->fdt[fd];
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
        int pos = file_length(file_);
        lock_release(&filesys_lock);
	return pos;

}

void seek(int fd, unsigned position)
{
	if(fd < 0 || fd > 63) return;

	struct file* file_ = thread_current()->fdt[fd];
	if(file_ == NULL) return;
	file_seek(file_, position);
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
	int return_fd = cur ->next_fd;

	if(cur-> next_fd == -1) // FDT is full
	   return -1;

	lock_acquire(&filesys_lock);
	struct file* file_ = filesys_open(file);
	lock_release(&filesys_lock);

	if(file_ == NULL)
		return -1;

	cur->fdt[return_fd] = file_;

	cur->next_fd = get_next_fd();

       return cur->next_fd;	

}

void close(int fd)
{
	if(fd < 0 || fd > 63)
		exit_(-1);
	
	struct thread* cur = thread_current();
	if(cur->fdt[fd] == NULL)
	{
		if(fd == 0)
		   cur->stdin_closed = true;
	        
		else if(fd == 1)
		   cur->stdout_closed = true;

		return;
	}
	lock_acquire(&filesys_lock);
	file_close(cur->fdt[fd]);
	lock_release(&filesys_lock);

	cur->next_fd = fd;

}

int pipe(int* fds)
{
	if(!validate_pointer(fds))
		exit_(-1);

	struct thread* cur = thread_current();
	



	//TODO create a buffer for the pipe and make it circular
	return -1;
}
