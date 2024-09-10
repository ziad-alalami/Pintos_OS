/* Low priority thread L acquires a lock, then blocks downing a
   semaphore.  Medium priority thread M then blocks waiting on
   the same semaphore.  Next, high priority thread H attempts to
   acquire the lock, donating its priority to L.

   Next, the main thread ups the semaphore, waking up L.  L
   releases the lock, which wakes up H.  H "up"s the semaphore,
   waking up M.  H terminates, then M, then L, and finally the
   main thread.

   Written by Godmar Back <gback@cs.vt.edu>. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"
#include "threads/malloc.h"

static void
w_thread_func (void *ls_); 

void
test_seqlock5 (void) 
{
  struct seqlock seqlock;
  int64_t sequence, start_time, i;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  seqlock_init (&seqlock);
  sequence = read_seqlock_begin(&seqlock);
  thread_create ("writer", PRI_DEFAULT, w_thread_func, &seqlock);

  start_time = timer_ticks ();
  while (timer_elapsed (start_time) < 2 * TIMER_FREQ)
    continue;
  
  for (i = 0; i < 1000; i++) {
    if (!read_seqretry(&seqlock, sequence)) {
      fail ("you should retry.");
    }
  }
}

static void
w_thread_func (void *ls_) 
{
  int64_t sequence;
  struct seqlock *seqlock = ls_;
  
  sequence = read_seqlock_begin(seqlock);
  write_seqlock(seqlock);
    
  if (sequence != (read_seqlock_begin(seqlock) - 1)) {
    fail ("1 wrong sequence number!");
  }
  
  write_sequnlock(seqlock);

  if (sequence != (read_seqlock_begin(seqlock) - 2)) {
    fail ("wrong sequence number!");
  }

  return;
}
