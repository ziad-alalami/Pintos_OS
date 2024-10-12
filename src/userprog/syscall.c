#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <string.h>
#define MAX_ARGS 3 // Maximum number of arguments for syscalls

//Used to cast void* type pointers to another type
#define CAST_TO_TYPE(pointer,type) ((type *) (uintptr_t)(pointer))

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
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

void** return_arguments(struct intr_frame *f, int num_args) {
 
    if (num_args > MAX_ARGS) 
        num_args = MAX_ARGS;
    
    static void* args[MAX_ARGS];
    
   
    for (int i = 0; i < num_args; i++) 
      
        args[i] = (void *)(*(uintptr_t *)(f->esp + 4 * (i + 1)));
    
    
    return args;  
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int syscall_number;
  get_syscall_number(&syscall_number);
  int syscall_output;

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
	//TODO
	return -1;
}

bool create(const char *file, unsigned initial_size)
{
	//TODO
	return false;
}

bool remove(const char *file)
{
	//TODO
	return false;
}

int open(const char *file)
{
	//TODO
	return -1;
}

int filesize(int fd)
{
	//TODO
	return -1;
}

int read(int fd, void *buffer, unsigned size)
{
	//TODO
	return -1;
}

int write(int fd, const void * buffer, unsigned size)
{
	//TODO
	return -1;
}

void seek(int fd, unsigned position)
{
	//TODO
	return;
}

unsigned tell(int fd)
{
	//TODO
	return 0;
}

void close(int fd)
{
	//TODO
	return;
}

