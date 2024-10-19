#include "threads/synch.h"
#include <stdbool.h>
#define PIPE_CAP (4096) // Max number of elements

struct pipe{
void* buffer;
  int size;
  int num_readers;
  int num_writers;
  struct semaphore read_wait_sema;
  struct semaphore write_wait_sema;
  struct semaphore modify_sema;
  int next_read;
  int next_write;
  bool read_waiting;
  bool write_waiting;
};

void pipe_init(struct pipe*);
int pipe_read(struct pipe*, void*, unsigned);
int pipe_write(struct pipe*, const void*, unsigned);
void pipe_close_reader(struct pipe*);
void pipe_close_writer(struct pipe*);