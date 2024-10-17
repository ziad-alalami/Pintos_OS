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
      halt();
      break;
    case SYS_EXIT:
      int status = *(int*)(f->esp + sizeof(int));
      exit_(status);
      break;
    case SYS_EXEC:
      char* cmd_line = *(char**)(f->esp + sizeof(int));
      f->eax = exec(cmd_line);
      break;
    case SYS_WAIT:
      pid_t pid = *(pid_t*)(f->esp + sizeof(int));
      f->eax = wait(pid);
      break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      int fd = *(int*)(f->esp + sizeof(int));
      const void* buffer = *(void**)(f->esp + 2 * sizeof(int));
      unsigned size = *(unsigned*)(f->esp + 2 * sizeof(int) + sizeof(void*));
      f->eax = write(fd, buffer, size);
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE :
      break;
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
  if (!validate_pointer(buffer) || fd < 0 || fd > 63) return -1;

  if (fd == 1) {
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
