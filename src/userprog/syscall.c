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
#include "threads/pipe.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
static void syscall_handler (struct intr_frame *);

static struct lock filesys_lock;

/*Initializes lock and interrupt vector for syscall handling*/
void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
/*Handles syscall functions from user and takes inputs from the user stack*/
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
      f->eax = pipe(fds);
      break;
      }
    case SYS_CHDIR:
      {
	      if(!validate_pointer(addr1)) exit_(-1);
	      const char *dir = *(char **)(addr1);
	     f->eax = chdir(dir);
	    break; 
      }
       case SYS_MKDIR:
      {
              if(!validate_pointer(addr1)) exit_(-1);
              const char *dir = *(char **)(addr1);
             f->eax = mkdir(dir);
            break;
      }
          case SYS_READDIR:
      {
              void* addr2 = addr1 + sizeof(int);
      	  if (!validate_pointer(addr1) || !validate_pointer(addr2)) exit_(-1);
	      int fd = *(int *) (addr1);
              const char *name = *(char **)(addr2);
             f->eax = readdir(fd, name);
            break;
      }
       case SYS_ISDIR:
      {
              if(!validate_pointer(addr1)) exit_(-1);
              int dir = *(int*)(addr1);
             f->eax = isdir(dir);
            break;
      }
      case SYS_INUMBER:
      {
              if(!validate_pointer(addr1)) exit_(-1);
              int fd = *(int*)(addr1);
             f->eax = inumber(fd);
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

/*Get the next unoccupied FD between 2 and 63 otherwise return -1*/
int get_next_fd()
{
	struct thread* cur = thread_current();
	for(int i = 2; i < 64; ++i)
		if(cur->fdt[i] == NULL)
			return i;
	return -1; //Table is full
}

/*Turns off Pintos OS*/
void halt() {
  shutdown_power_off();
}
/*Exits current process/thread with given status and wakes up parent process*/
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

/*Returns size when size bytes is written from buffer, otherwise wait
 * If the pointers are invalid, it exits with status -1
 * If FD is invalid or corresponds to NULL, returns -1
 * If trying to write to STDIN, exits with status -1
 * If trying to write to STDOUT, waits for input
 * If trying to write to pipe buffer, calls pipe_write API
 * Else writes to a normal file*/

int write(int fd, const void *buffer, unsigned size) {
  if (!validate_pointer(buffer)) exit_(-1);
  if(fd < 0 || fd > 63) return -1;

  struct thread* cur = thread_current();

  if (fd == 0) exit_(-1);

  if (fd == 1) {
    lock_acquire(&filesys_lock);
    putbuf(buffer, size);
    lock_release(&filesys_lock);
    return size;
  }

  struct file_descriptor* file_desc = cur->fdt[fd];
  if (file_desc == NULL) return -1;

  int result = 0;
  if (file_desc->type == FILE) {
    if(inode_is_dir(file_get_inode(file_desc->file)))
	    return -1;
    lock_acquire(&filesys_lock);
    result = file_write(file_desc->file, buffer, size);
    lock_release(&filesys_lock);
  }
  else if (file_desc->type == PIPE_WRITER)
    result = pipe_write(file_desc->pipe, buffer, size);
  else
    result = -1;

  return result;
}

/*Creates a new process and executes it after taking input from command line
 * New process is added to child list of the parent process and returns its
 * process ID (pid)*/
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

/*Waits for a child process to terminate before waking up again*/
int wait(pid_t pid) {
  return process_wait(pid);
}

/*Returns number of bytes read into the buffer.
 * If the pointers are invalid, it exits with status -1
 * If FD is invalid or corresponds to NULL, returns -1
 * If trying to read from STDOUT, exits with status -1
 * If trying to read from STDIN, waits for input
 * If trying to read from pipe buffer, calls pipe_read API
 * Else reads from a normal file*/
int read(int fd, const void *buffer, unsigned size)
{
	if(!validate_pointer(buffer))
	  exit_(-1);

	if (fd < 0 || fd > 63) return -1;

  struct thread* cur = thread_current();

  if (fd == 1) exit_(-1);

  if (fd == 0 && cur->fdt[fd] == NULL) {
    lock_acquire(&filesys_lock);
    input_getc();
    lock_release(&filesys_lock);
    return size;
  }

  struct file_descriptor* file_desc = cur->fdt[fd];
  if (file_desc == NULL) return -1;

  int result = 0;
  if (file_desc->type == FILE) {
    if(inode_is_dir(file_get_inode(file_desc->file)))
	    return -1;
    lock_acquire(&filesys_lock);
    result = file_read(file_desc->file, buffer, size);
    lock_release(&filesys_lock);
  }
  else if (file_desc->type == PIPE_READER)
    result = pipe_read(file_desc->pipe, buffer, size);
  else
    result = -1;

  return result;
}
/*Returns current offset position from a given FD if the file exists, if the 
 * input is invalid or the corresponding FD is empty, it returns -1*/
unsigned int tell(int fd)
{
	if(fd < 0 || fd > 63)
		return -1;
	struct file_descriptor* file_desc = thread_current()->fdt[fd];
	if(file_desc == NULL || file_desc->type != FILE) return -1;

	lock_acquire(&filesys_lock);
	unsigned pos = file_tell(file_desc->file);
	lock_release(&filesys_lock);
	return pos;
}
/*Returns the size of the given file in the file descriptor, returns -1 if given
 FD is invalid or does not correspond to a file descriptor*/
int filesize(int fd)
{
  if(fd < 0 || fd > 63)
    return -1;
	struct file_descriptor* file_desc = thread_current()->fdt[fd];

  if(file_desc == NULL || file_desc->type != FILE) return -1;
  lock_acquire(&filesys_lock);
  int size = file_length(file_desc->file);
  lock_release(&filesys_lock);
	return size;
}
/*Moves current file pointer to given position, if position is larger than
 * file size, it will move to the end*/
void seek(int fd, unsigned position)
{
	if(fd < 0 || fd > 63) return;

	struct file_descriptor* file_desc = thread_current()->fdt[fd];

	if(file_desc == NULL || file_desc->type != FILE) return;
  lock_acquire(&filesys_lock);
	file_seek(file_desc->file, position);
  lock_release(&filesys_lock);
}
/*Creates a new file in the file system directory and exits in case of invalid
 * pointer*/
bool create(const char* file, unsigned initial_size)
{
	if(!validate_pointer(file))
		exit_(-1); //Malicious pointer

	lock_acquire(&filesys_lock);
	bool created = filesys_create(file, initial_size, false);
	lock_release(&filesys_lock);

	return created;
}
/*Removes a given file from the file system directory and exits
 * with status -1 in case of invalid pointer*/
bool remove(const char* file)
{
	if(!validate_pointer(file))
		exit_(-1);
	lock_acquire(&filesys_lock);
	bool removed = filesys_remove(file);
	lock_release(&filesys_lock);
	return removed;
}

/*Opens a file and returns its FD (between 2 and 63 as 0 and 1 are reserved
 * for STDIN and STDOUT respectively), return -1 when FDT is full and exits
 * in case of invalid pointer*/
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

	cur->fdt[next_fd] = malloc(sizeof(struct file_descriptor));
  cur->fdt[next_fd]->type = FILE;
  cur->fdt[next_fd]->file = file_;
  cur->fdt[next_fd]->pipe = NULL;

  return next_fd;	
}

/*Closes the given file descriptor (file or pipe) and exits wiht -1 in case of
 * wrong input or attempting to close STDIN or STDOUT*/
void close(int fd)
{
	if(fd < 0 || fd > 63)
		exit_(-1);
	
	struct thread* cur = thread_current();

  if (fd == 0 || fd == 1) exit_(-1);
  if(cur->fdt[fd] == NULL)
    return;
  
  struct file_descriptor* file_desc = cur->fdt[fd];

  lock_acquire(&filesys_lock);
  if (file_desc->type == FILE)
    file_close(file_desc->file);
  else if (file_desc->type == PIPE_READER)
    pipe_close_reader(file_desc->pipe);
  else if (file_desc->type == PIPE_WRITER)
    pipe_close_writer(file_desc->pipe);
  lock_release(&filesys_lock);
  cur->fdt[fd] = NULL;
}

/*Pipe syscall that returns 0 if pipe created successfully, otherwise
 * returns -1 . The first element in the array fds[0] is the pipe read file descriptor
 * and the second fds[1] is the pipe write file descriptor*/
int pipe(int* fds)
{
	if(!validate_pointer(fds))
		exit_(-1);

	struct thread* cur = thread_current();
	
  int reader_fd = get_next_fd();
  if (reader_fd == -1) return -1;
  cur->fdt[reader_fd] = 1; // Temp set to 1 while we search for the next fd

  int writer_fd = get_next_fd();
  if (writer_fd == -1) {
    cur->fdt[reader_fd] = NULL;
    return -1;
  }

  fds[0] = reader_fd;
  fds[1] = writer_fd;

  struct pipe* pipe = malloc(sizeof(struct pipe));
  pipe_init(pipe);

	struct file_descriptor* reader = malloc(sizeof(struct file_descriptor));
  reader->type = PIPE_READER;
  reader->pipe = pipe;
  reader->file = NULL;
  cur->fdt[reader_fd] = reader;

  struct file_descriptor* writer = malloc(sizeof(struct file_descriptor));
  writer->type = PIPE_WRITER;
  writer->pipe = pipe;
  writer->file = NULL;
  cur->fdt[writer_fd] = writer;

	return 0;
}

bool chdir(const char* path)
{
	if(path == NULL)
		return false;

	char* name = path_to_name(path);
	struct dir* dir = resolve_path(path);
	struct inode* inode = NULL;
	if(dir == NULL)
	{
		free(name);
		return false;
	}
	else if(strcmp(name,"..") == 0)
	{
		inode = inode_get_parent(dir_get_inode(dir));
		if(inode == NULL) // no parent... called at root?
		{
			free(name);
			return false;
		}
	}

	else if((strcmp(name, ".") == 0) || (strlen(name) == 0 && dir_is_root(dir)))
	{
		thread_current()->curr_dir = dir;
		free(name);
		return true;
	}
	else 
		dir_lookup(dir, name, &inode);

	dir_close(dir);

	if(inode == NULL)
	{
		free(name);
		return false;
	}

	dir = dir_open(inode);

	if(dir == NULL) //that path does not exist or it is a normal file?
	{
		free(name);
		return false;
	}
	dir_close(thread_current()->curr_dir);
	thread_current()->curr_dir = dir;
	free(name);
	return true;
}

bool mkdir(const char* path)
{
	bool success = filesys_create(path, 0, true);
	return success;
}

bool isdir(int fd)
{
        if(fd < 0 || fd > 63)
                exit_(-1);
        struct file* cur_file = thread_current()->fdt[fd];
        if(cur_file == NULL) return false;
        struct inode *inode = file_get_inode(cur_file);
        if(inode == NULL || !inode_is_dir(inode)) return false;
        return true;
}



bool readdir(int fd, char* path)
{
	if(fd < 0 || fd > 63)
	       exit_(-1);
	struct file* cur_file = thread_current()->fdt[fd];
	if(cur_file == NULL) return false;
	
	struct inode* inode = file_get_inode(cur_file);
	if(inode == NULL || !inode_is_dir(inode)) return false;

	//It must be a dir then...
	struct dir* dir = dir_open(inode);
	return dir_readdir(dir, path);
}

int inumber(int fd)
{
	if(fd < 0 || fd > 63)
		exit_(-1);
	struct file* cur_file = thread_current()->fdt[fd];
	if(cur_file == NULL) return -1; //Should we return -1 or just exit?
	struct inode* inode = file_get_inode(cur_file);
	if(inode == NULL) return -1;

	block_sector_t inode_number = inode_get_inumber(inode);
	return inode_number;

}
