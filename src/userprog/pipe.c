#include "pipe.h"
#include "threads/thread.h"
#define PAGE_SIZE 4096
void pipe_init(struct pipe *pipe)
{
	pipe->buffer = palloc_get_page(0);
	pipe->num_readers = 0;
	pipe->num_writers = 0;
	pipe->next_read = -1;
	pipe->next_write = -1;
	sema_init(&pipe->pipe_sema, 1);
}

int pipe_write(struct pipe* pipe,const void* write_buffer ,unsigned size)
{
	pipe->write_thread = thread_current();
	if(pipe->num_readers == 0)
		return -1;
	if(pipe->next_write == -1)
		pipe->next_write = 0;

	char* buffer = (char*)pipe->buffer;
	const char* data = (const char*)write_buffer;
	pipe->num_writers++;
	sema_down(&pipe->pipe_sema);
	for(int i = 0; i < size; i++)
	{
		pipe->next_write = NEXT_INDEX(pipe->next_write, PAGE_SIZE);

		if(pipe->next_write == pipe->next_read)
		{
			sema_up(&pipe->pipe_sema); // Release lock before block
			thread_block();
			sema_down(&pipe->pipe_sema);
		}

		buffer[pipe->next_write] = data[i];

		if(pipe->read_thread != NULL && pipe->read_thread->status == THREAD_BLOCKED)
			thread_unblock(pipe->read_thread); // There is data for read thread to read now

	}
	sema_up(&pipe->pipe_sema);
	pipe->write_thread = NULL; // There is no writer anymore
	pipe->num_writers--;
	return size;

}

int pipe_read(struct pipe* pipe,void *buffer ,unsigned size)
{
	if(pipe->num_writers == 0 && pipe->next_read == -1)
		return 0;


	if(pipe->next_read == -1)
		pipe->next_read = 0;

	char* data_buffer = (char*) buffer;
	char* pipe_buffer = (char*)pipe->buffer;
	pipe->num_readers++;
	pipe->read_thread = thread_current();
	sema_down(&pipe->pipe_sema);
	for(int i = 0; i < size; i++)
	{
		pipe->next_read = NEXT_INDEX(pipe->next_read, PAGE_SIZE);
		data_buffer[i] = pipe_buffer[pipe->next_read];
		if(pipe->next_read == pipe->next_write)
			return i + 1;

		if(pipe->write_thread != NULL && pipe->write_thread->status == THREAD_BLOCKED)

			thread_unblock(pipe->write_thread);
	}
	sema_up(&pipe->pipe_sema);
	pipe->num_readers--;
	pipe->read_thread = NULL;
	return size;



}
