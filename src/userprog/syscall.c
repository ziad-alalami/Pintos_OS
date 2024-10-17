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
      break;
    case SYS_WAIT:
      break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      int fd = *(int*)(f->esp + sizeof(int));
      f->eax = filesize(fd);
      break;
    case SYS_READ:
      int fd = *(int*)(f->esp + sizeof(int));
      const void* buffer = *(void**)(f->esp + 2 * sizeof(int));
      unsigned size = *(unsigned*)(f->esp + 2 * sizeof(int) + sizeof(void*));
      f->eax = read(fd, buffer, size);
      break;
    case SYS_WRITE:
      int fd = *(int*)(f->esp + sizeof(int));
      const void* buffer = *(void**)(f->esp + 2 * sizeof(int));
      unsigned size = *(unsigned*)(f->esp + 2 * sizeof(int) + sizeof(void*));
      f->eax = write(fd, buffer, size);
      break;
    case SYS_SEEK:
      int fd = *(int*) (f->esp + sizeof(int));
      unsigned position = *(unsigned*)(f->esp + 2 * sizeof(int));
      f->eax = seek(fd, position);
      break;
    case SYS_TELL:
      int fd = *(int*)(f->esp + sizeof(int));
      f->eax = tell(fd);
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
  cur->exit_status = status;
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

int read(int fd, const void *buffer, unsigned size)
{
	if(!validate_pointer(buffer) || fd < 0 || fd > 63) return -1;

 if (fd == 0) {
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

unsigned tell(int fd)
{
	if(fd < 0 || fd > 63)
		return -1;
	struct file* file_ = thread_current()->fdt[fd];
	if(file_ == NULL) return -1;

	lock_acquire(&filesys_lock);
	unsigned pos = file_tell(file_);
	lock_release(&filesys_lock);
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
		exit(-1); //Malicious pointer
	
	
}
