#include "threads/synch.h"
#include <stdbool.h>
#define NEXT_INDEX(next,size) ((next) % size)
/*
 *Pipe struct including lock mechanisms and ring buffer
 */

struct pipe{

	void* buffer; // Will be of size of one page
        int num_readers;
	int num_writers;
	struct semaphore pipe_sema;
	int next_read; // For pipe_read
	int next_write; // For pipe_write
	struct thread* write_thread;
	struct thread* read_thread;	
};

void pipe_init(struct pipe*);
int pipe_read(struct pipe*, void*, unsigned);
int pipe_write(struct pipe*, const void*, unsigned);
