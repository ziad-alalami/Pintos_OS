#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

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
			"=r" (*value)
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
  switch(syscall_number)
	case SYS_HALT:
	  halt();

	case SYS_EXIT:
	  exit(f);

	case SYS_EXEC:
	 f->eax = exec(f);

	case SYS_WAIT:
	 f->eax =  wait(f);

	case SYS_CREATE:
	 f->eax =  create(f);

	case SYS_REMOVE:
	 f->eax = remove(f);

	case SYS_OPEN:
	 f->eax =  open(f);

	case SYS_FILESIZE:
	 f->eax = filesize(f);

	case SYS_READ:
	 f->eax =  read(f);

	case SYS_WRITE:
	  f->eax = write(f);

	case SYS_SEEK:
	  seek(f);

	case SYS_TELL:
	  f->eax = tell(f);

	case SYS_CLOSE:
	  close(f);

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

void exit(struct intr_frame *f)
{
	
	int status = *(int *) (f->esp + 4);	

	struct thread *cur = thread_current();

	cur -> stack -= 4;
	memcpy(stack, &status, sizeof(int));
	cur -> exit_status = status;
	printf("%s: exit(%d)\n", cur->name, status);	
	thread_exit();
	
}

int wait_(struct intr_frame *f)
{
   pid_t pid = *(pid_t *) (f->esp + 4);

    struct thread *cur = thread_current();


    cur->stack -= sizeof(pid_t);
    memcpy(cur->stack, &pid, sizeof(pid_t));

  
    struct list_elem *e;
    for (e = list_begin(&cur->waited_children); e != list_end(&cur->waited_children); e = list_next(e)) {
        struct thread *waited_child = list_entry(e, struct thread, elem);
        
        if (waited_child->pid == pid) {
            return -1;
        }
    }

    struct thread *child_thread = NULL;
    for (e = list_begin(&cur->children); e != list_end(&cur->children); e = list_next(e)) {
        struct thread *child = list_entry(e, struct thread, child_elem);
        
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

pid_t exec(struct intr_frame *f)
{
	//TODO
	return -1;
}

bool create(struct intr_frame *f)
{
	//TODO
	return false;
}

int open(struct intr_frame *f)
{
	//TODO
	return -1;
}

int filesize(struct intr_frame *f)
{
	//TODO
	return -1;
}

int write(struct intr_frame *f)
{
	//TODO
	return -1;
}

void seek(struct intr_frame *f)
{
	//TODO
	return;
}

unsigned tell(struct intr_frame *f)
{
	//TODO
	return 0;
}

void close(struct intr_frame *f)
{
	//TODO
	return;
}

