#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_num = *((int*)f->esp);
  printf("syscall_num: %d\n", syscall_num);

  switch (syscall_num) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      exit_(f);
      break;
    case SYS_EXEC:
      break;
    case SYS_WAIT:
      break;
  }
}

void halt() {
  shutdown_power_off();
}

void exit_(struct intr_frame *f) {
  int status = *(int*)(f->esp + 4);
  struct thread* cur = thread_current();
  cur->exit_status = status;
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

void exec() {

}

void wait() {

}