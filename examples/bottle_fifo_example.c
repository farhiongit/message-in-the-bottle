#undef NDEBUG
#include <stdio.h>
#include <assert.h>
#include <threads.h>
#include "bottle_impl.h"
bottle_type_declare (int);
bottle_type_define (int);

static void
write (bottle_t (int) *fifo)
{
  errno = 0;
  for (int i = 1; i <= 3 && !errno; i++)
    if (bottle_try_send (fifo, i) && !errno)
      printf ("%i -->\n", i);
  assert (!errno);
}

static int
read (void *arg)
{
  bottle_t (int) * fifo = arg;
  int i;
  errno = 0;
  while (!errno)
    if (bottle_try_recv (fifo, &i) && !errno)
      printf ("--> %i\n", i);
  return errno;
}

int
main (void)
{
  bottle_t (int) * fifo;
  thrd_t thread_id;
  printf ("---------------------------------------------------------------\n");
  const size_t BOTTLE_CAPACITY = 10;
  fifo = bottle_create (int, BOTTLE_CAPACITY);
  printf ("Declared Capacity %zu\n", BOTTLE_CAPACITY);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  write (fifo);
  bottle_close (fifo);          // Close to indicate writing is done.
  printf ("Declared Capacity %zu\n", BOTTLE_CAPACITY);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  thrd_create (&thread_id, read, fifo);
  thrd_join (thread_id, 0);
  printf ("Declared Capacity %zu\n", BOTTLE_CAPACITY);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  bottle_destroy (fifo);
  printf ("---------------------------------------------------------------\n");
  fifo = bottle_create (int, UNLIMITED);
  printf ("Declared Capacity %zu\n", UNLIMITED);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  write (fifo);
  bottle_close (fifo);          // Close to indicate writing is done.
  printf ("Declared Capacity %zu\n", UNLIMITED);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));    // The capacity adapts to the number of transmitted messages.
  thrd_create (&thread_id, read, fifo);
  thrd_join (thread_id, 0);
  printf ("Declared Capacity %zu\n", UNLIMITED);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));    // The capacity adapts to the number of transmitted messages.
  bottle_destroy (fifo);
}
