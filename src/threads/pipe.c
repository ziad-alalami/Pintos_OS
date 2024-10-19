#include "pipe.h"
#include "threads/malloc.h"
#include <string.h>
#include <stdio.h>
#include "threads/thread.h"

void pipe_init(struct pipe* pipe) {
  pipe->buffer = malloc(PIPE_CAP);
  pipe->size = 0;
  pipe->num_readers = 1;
  pipe->num_writers = 1;
  sema_init(&pipe->read_wait_sema, 0);
  sema_init(&pipe->write_wait_sema, 0);
  sema_init(&pipe->modify_sema, 1);
  pipe->next_read = 0;
  pipe->next_write = 0;
  pipe->read_waiting = false;
  pipe->write_waiting = false;
}

int pipe_read(struct pipe* pipe, void* buffer, unsigned size) {
  if (pipe->num_writers == 0 && pipe->size == 0) return 0;
  
  sema_down(&pipe->modify_sema);
  if (pipe->size == 0) {
    pipe->read_waiting = true;
    sema_up(&pipe->modify_sema);
    sema_down(&pipe->read_wait_sema);
    sema_down(&pipe->modify_sema);
  }

  if (pipe->size == 0) return 0;

  unsigned bytes_read = 0;
  while (bytes_read < size && pipe->size > 0) {
    memcpy(buffer + bytes_read, pipe->buffer + pipe->next_read, 1);
    pipe->next_read = (pipe->next_read + 1) % PIPE_CAP;
    ++bytes_read;
    --pipe->size;
  }

  if (pipe->write_waiting) {
    pipe->write_waiting = false;
    sema_up(&pipe->write_wait_sema);
  }

  sema_up(&pipe->modify_sema);
  return bytes_read;
}

int pipe_write(struct pipe* pipe, const void* buffer, unsigned size) {
  if (pipe->num_readers == 0) return -1;

  unsigned bytes_written = 0;
  while (bytes_written < size && pipe->num_readers != 0) {
    sema_down(&pipe->modify_sema);
    while (bytes_written < size && pipe->size < PIPE_CAP) {
      memcpy(pipe->buffer + pipe->next_write, buffer + bytes_written, 1);
      pipe->next_write = (pipe->next_write + 1) % PIPE_CAP;
      ++bytes_written;
      ++pipe->size;
    }

    if (bytes_written < size)
      pipe->write_waiting = true;

    if (pipe->read_waiting) {
      pipe->read_waiting = false;
      sema_up(&pipe->read_wait_sema);
    }

    sema_up(&pipe->modify_sema);

    if (bytes_written < size)
      sema_down(&pipe->write_wait_sema);
  }
  return bytes_written;
}

void pipe_close_reader(struct pipe* pipe) {
  sema_down(&pipe->modify_sema);
  --pipe->num_readers;
  if (pipe->num_readers == 0 && pipe->num_writers == 0) {
    free(pipe->buffer);
    free(pipe);
  } else {
    sema_up(&pipe->modify_sema);
    if (pipe->num_readers == 0) {
      sema_up(&pipe->write_wait_sema);
    }
  }
}

void pipe_close_writer(struct pipe* pipe) {
  sema_down(&pipe->modify_sema);
  --pipe->num_writers;
  if (pipe->num_readers == 0 && pipe->num_writers == 0) {
    free(pipe->buffer);
    free(pipe);
  } else {
    sema_up(&pipe->modify_sema);
    if (pipe->num_writers == 0) {
      sema_up(&pipe->read_wait_sema);
    }
  }
}