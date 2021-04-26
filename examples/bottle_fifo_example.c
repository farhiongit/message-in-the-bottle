#include <stdio.h>
#include "bottle_impl.h"
DECLARE_BOTTLE (int);
DEFINE_BOTTLE (int);

static void write (BOTTLE (int) *b)
{
  for (int i = 1 ; i <= 3 ; i++)
    BOTTLE_TRY_FILL (b, i);
}

static void read (BOTTLE (int) *b)
{
  int i;
  while (BOTTLE_TRY_DRAIN (b, i))
    printf ("%i\n", i);
}

int main (void)
{
  BOTTLE (int) * fifo;

  fifo = BOTTLE_CREATE (int, 10);
  printf ("Capacity %zu\n", BOTTLE_CAPACITY (fifo));
  write (fifo);
  read (fifo);
  printf ("Capacity %zu\n", BOTTLE_CAPACITY (fifo));
  BOTTLE_CLOSE (fifo);
  BOTTLE_DESTROY (fifo);

  fifo = BOTTLE_CREATE (int, UNLIMITED);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  write (fifo);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  read (fifo);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  BOTTLE_CLOSE (fifo);
  BOTTLE_DESTROY (fifo);
}
