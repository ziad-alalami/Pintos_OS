#include "userprog/syscall.h"
#include <stdio.h>
#include <stdbool.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <string.h>
#define MAX_ARGS 3 // Maximum number of arguments for syscalls

//Used to cast void* type pointers to another type
#define CAST_TO_TYPE(pointer,type) ((type *) (uintptr_t)(pointer))

static void syscall_handler (struct intr_frame *);

static struct lock filesys_lock;

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/*
 Use in line assembly to retrieve the last pushed element to the stack pointer
 which is the syscall number
  */
void get_syscall_number(int* value_address)
{
	asm volatile("movl (%%esp), %0\n":
			"=r" (*value_address)
			);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int syscall_number;
  get_syscall_number(&syscall_number);

  struct thread* cur = thread_current();
  

  switch(syscall_number) {
	case SYS_HALT:{
	  halt();
	  break;
		      }
	case SYS_EXIT:{

       	 int status = *(int *) (f->esp + 4);
          cur -> stack -= sizeof(int);
          memcpy(cur->stack, &status, sizeof(int));
	  exit(status);
	  break;
		      }

	case SYS_EXEC:{

	  const char * cmd_line = *(char **)(f->esp + 4);
	  cur -> stack -= sizeof(char *);
	  *(const char **) cur->stack = cmd_line;
	  f->eax = exec(cmd_line);
	  break;}

	case SYS_WAIT:{

	  pid_t pid = *(pid_t *) (f->esp + 4);
	  struct thread *cur = thread_current();
	  cur->stack -= sizeof(pid_t);
	  memcpy(cur->stack, &pid, sizeof(pid_t));
	  f->eax = wait(pid);
	  break;}

	case SYS_CREATE:{
	  const char *file = *(char **)(f->esp + 4);
	  unsigned initial_size = *(int *)(f->esp + 8);
	  cur -> stack -= sizeof(unsigned);
	  memcpy(cur->stack, &initial_size, sizeof(unsigned));

	  cur -> stack -= sizeof(char*);
	  *(const char **) cur->stack = file;

	  f->eax = create(file, initial_size);
	  break;}

	case SYS_REMOVE:{

	  const char *file = *(char **)(f->esp + 4);
	  cur-> stack -= sizeof(const char*);
          *(const char **) cur->stack = file;


	  f->eax = remove(file);
	  break;
			}

	case SYS_OPEN:{

	  const char *file = *(char **)(f->esp + 4);
	  cur -> stack -= sizeof(const char*);
          *(const char **) cur->stack = file;

	  f->eax = open(file);
	  break;
		      }

	case SYS_FILESIZE:{

	  int fd = *(int *)(f->esp + 4);
	  cur -> stack -= sizeof(int);
	 

	  f->eax = filesize(fd);
	  break;
			  }


	case SYS_READ:{
	  int fd = *(int *)(f->esp + 4);
	  void * buffer = *(void **) (f->esp + 8);
	  unsigned size = *(unsigned *)(f->esp + 12);

	  cur -> stack -= sizeof(unsigned);
	  memcpy(cur->stack, &size, sizeof(unsigned));

	  cur -> stack -= sizeof(void *);
	  *(void **) cur->stack = buffer;

	  cur -> stack -= sizeof(int);
	  memcpy(cur->stack, &fd, sizeof(int));

	  f->eax = read(fd, buffer, size);
	  break;
		      }

	case SYS_WRITE:{

	  int fd = *(int *)(f->esp + 4);
          void * buffer = *(void **) (f->esp + 8);
          unsigned size = *(unsigned *)(f->esp + 12);

          cur -> stack -= sizeof(unsigned);
          memcpy(cur->stack, &fd, sizeof(unsigned));

          cur -> stack -= sizeof(void *);
          *(void **) cur -> stack = buffer;

          cur -> stack -= sizeof(int);
          memcpy(cur->stack, &fd, sizeof(int));

	  f->eax = write(fd, buffer,size);
	  break;
		       }

	case SYS_SEEK:{

	  int fd = *(int *)(f->esp + 4);
          unsigned position = *(unsigned*) (f->esp + 8);

          cur -> stack -= sizeof(unsigned);
          memcpy(cur->stack, &fd, sizeof(unsigned));

          cur -> stack -= sizeof(int);
          memcpy(cur->stack, &position, sizeof(int));

	  seek(fd, position);
	  break;
		      }

	case SYS_TELL:{

	  int fd = *(int *)(f->esp + 4);
	  cur -> stack -= sizeof(int);
	  memcpy(cur -> stack, &fd, sizeof(int));

	  f->eax = tell(fd);
	  break;
		      }

	case SYS_CLOSE:{

	  int fd = *(int *)(f->esp + 4);
          cur -> stack -= sizeof(int);
          memcpy(cur -> stack, &fd, sizeof(int));

	  close(fd);
	  break;
		       }
  }

  printf ("system call!\n");
  thread_exit ();
}


/*
 *Return false if pointer is null, not mapped to virtual memory, or part of kernel memory
 * */
bool validate_pointer(void * pointer)
{
	if(pointer == NULL || !is_user_vaddr(pointer) || pagedir_get_page(thread_current()->pagedir, pointer) == NULL)
		return false;
}

void halt()
{
	shutdown_power_off();
}

void exit(int status)
{
		
	struct thread *cur = thread_current();
	cur -> exit_status = status;
	printf("%s: exit(%d)\n", cur->name, status);	
	thread_exit();
	
}

int wait(pid_t pid)
{
    struct thread *cur = thread_current();
  
    struct list_elem *e;
    for (e = list_begin(&cur->waited_children); e != list_end(&cur->waited_children); e = list_next(e)) {
        struct thread *waited_child = list_entry(e, struct thread, elem);
        
        if (waited_child->pid == pid) {
            return -1;
        }
    }

    struct thread *child_thread = NULL;
    for (e = list_begin(&cur->children); e != list_end(&cur->children); e = list_next(e)) {
        struct thread *child = list_entry(e, struct thread, elem);
        
        if (child->pid == pid) {
            child_thread = child;
            break;
        }
    }

    if (child_thread == NULL) 
        return -1;
    
    list_push_back(&cur->waited_children, &child_thread->elem);	
    return process_wait(child_thread->tid);	
	
}

pid_t exec(const char * cmd_line)
{
	if(!validate_pointer(cmd_line))
		exit(-1);

	tid_t tid = process_execute(cmd_line);
	if(tid == TID_ERROR)
		return -1;

	struct thread * cur = thread_current();
	struct thread *child_thread = NULL;
	struct list_elem* e;
	for(e = list_begin(&cur -> children); e != list_end(&cur -> children); e = list_next(e))
	{
		struct thread *child = list_entry(e, struct thread, elem);
		if(child -> tid == tid)
		{
			child_thread = child;
			break;
		}
	}

	if(child_thread != NULL)
		sema_down(&child_thread->sema);

	return tid;

}

bool create(const char *file, unsigned initial_size)
{

	if(!validate_pointer(file))
		exit(-1);

	return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
	//TODO
	return false;
}

int open(const char *file)
{
	struct thread *cur = thread_current();
	int fd = cur -> next_fd;

	if(!validate_pointer(file))
		exit(-1);

	if(fd == -1)
		return -1;

	lock_acquire(&filesys_lock);
	struct file* opened_file = filesys_open(file);
	lock_release(&filesys_lock);

	if(opened_file == NULL)
		return -1;

	cur->fdt[fd] = opened_file;

	for(int i = 2; i < 64; i++)
	{
		if(cur -> fdt[i] == NULL)
		{
			cur -> next_fd = i;
			break;
		}	
	}
	if(cur -> next_fd == fd) //Next_fd did not change so there is no space
	   cur -> next_fd = -1;

	return fd;

}

int filesize(int fd)
{
	if(fd < 0 || fd > 63)
		return -1;

	struct thread * cur = thread_current();
	struct file* file_ = cur -> fdt[fd];

	if(file_ == NULL)
		return -1;
	
	int result;
	lock_acquire(&filesys_lock);
	result = file_length(file_);
	lock_release(&filesys_lock);
	return result;
}

int read(int fd, void *buffer, unsigned size)
{

	if(!validate_pointer(buffer) || fd < 0 || fd > 63)
		return -1;
	
	struct file* file_ = thread_current() -> fdt[fd];

	if(file_ == NULL && fd != 0)
		return -1;

	int result;
	lock_acquire(&filesys_lock);
	//DEAL WITH SPECIAL CASE FD == 0
	if(fd == 0)
	{
		//TODO
		result = -1;
	}
	else
		result = file_read(file_, buffer, size);
	lock_release(&filesys_lock);

	return result;
}

int write(int fd, const void * buffer, unsigned size)
{
	if(!validate_pointer(buffer) || fd < 1 || fd > 63)
		return -1;

	//DEAL WITH SPECIAL CASE FD == 0
	struct thread * cur = thread_current();
	if(cur->fdt[fd] == NULL)
		return -1;

	int result;
	lock_acquire(&filesys_lock);
	if(fd == 1)
	{
		result = -1;
		//TODO
	}
	else
		result = file_write(cur->fdt[fd], buffer, size);

	lock_release(&filesys_lock);
	return result;
}

void seek(int fd, unsigned position)
{
	ASSERT(fd > 1 && fd < 64); //You can not seek in std_in and std_out?	
	struct file * file_ = thread_current()->fdt[fd];

	lock_acquire(&filesys_lock);
	file_seek(file_, position);
	lock_release(&filesys_lock);
}

unsigned tell(int fd)
{
	if(fd < 2 || fd > 63)
		return -1;

	struct file * file_ = thread_current()->fdt[fd];

	int result;
	lock_acquire(&filesys_lock);
	result = file_tell(file_);
	lock_release(&filesys_lock);

	return result;
}

void close(int fd)
{
	ASSERT(fd >= 0 && fd < 64);
	struct thread * cur = thread_current();
	struct file* file_ = cur->fdt[fd];

	lock_acquire(&filesys_lock);
	file_close(file_);
	lock_release(&filesys_lock);

	if(fd < cur->next_fd)
		cur->next_fd =fd;
}

