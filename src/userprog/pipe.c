#include "pipe.h"
#include "threads/thread.h"
#define PAGE_SIZE 4096

void pipe_init(struct pipe *pipe)
{
    pipe->buffer = palloc_get_page(0);
    pipe->num_readers = 1;
    pipe->num_writers = 1;
    pipe->next_read = 0;
    pipe->next_write = 0;
    pipe->is_full = false;
    pipe->is_empty = true;  // Initially, the buffer is empty
    pipe->write_thread = thread_current();
    pipe->read_thread = thread_current();
    sema_init(&pipe->pipe_sema, 1);
}

int pipe_write(struct pipe* pipe, const void* write_buffer, unsigned size)
{
    pipe->write_thread = thread_current();
    if (pipe->num_readers == 0)  // No readers
        return -1;

    char* buffer = (char*)pipe->buffer;
    const char* data = (const char*)write_buffer;

    sema_down(&pipe->pipe_sema);

    for (int i = 0; i < size; i++)
    {
      
        if (pipe->is_full)
        {
            // Release semaphore and block the writer until space is available
            sema_up(&pipe->pipe_sema);
            thread_block();
            sema_down(&pipe->pipe_sema);
        }

    
        buffer[pipe->next_write] = data[i];
        pipe->next_write = NEXT_INDEX(pipe->next_write + 1, PAGE_SIZE);

      
        pipe->is_full = (pipe->next_write == pipe->next_read);
        pipe->is_empty = false;  

        // Unblock reader if it's waiting
        if (pipe->read_thread != NULL && pipe->read_thread->status == THREAD_BLOCKED)
        {
            thread_unblock(pipe->read_thread);
        }
    }

    sema_up(&pipe->pipe_sema);
    return size;
}

int pipe_read(struct pipe* pipe, void *buffer, unsigned size)
{
    if (pipe->num_writers == 0 && pipe->is_empty)  
        return 0;

    char* data_buffer = (char*) buffer;
    char* pipe_buffer = (char*)pipe->buffer;

    pipe->read_thread = thread_current();
    sema_down(&pipe->pipe_sema);

    unsigned bytes_read = 0;

    for (int i = 0; i < size; i++)
    {
        // If the buffer is empty, wait for writer
        if (pipe->is_empty)
        {
            // If no writers left, return the data we've read so far
            if (pipe->num_writers == 0)
            {
                sema_up(&pipe->pipe_sema);
                return bytes_read;
            }

            // Block the reader until more data is available
            sema_up(&pipe->pipe_sema);
            thread_block();
            sema_down(&pipe->pipe_sema);
        }

        
        data_buffer[i] = pipe_buffer[pipe->next_read];
        pipe->next_read = NEXT_INDEX(pipe->next_read + 1, PAGE_SIZE);

        bytes_read++;

    
        pipe->is_empty = (pipe->next_read == pipe->next_write);
        pipe->is_full = false; 

        if (pipe->write_thread != NULL && pipe->write_thread->status == THREAD_BLOCKED)
        {
            thread_unblock(pipe->write_thread);  
        }

     
        if (pipe->is_empty && pipe->num_writers == 0)
        {
            sema_up(&pipe->pipe_sema);
            return bytes_read;
        }
    }

    sema_up(&pipe->pipe_sema);
    return bytes_read;
}

