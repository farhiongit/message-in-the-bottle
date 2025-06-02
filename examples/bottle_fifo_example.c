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
  while (BOTTLE_TRY_DRAIN (b, &i))
    printf ("%i\n", i);
}

int main (void)
{
  BOTTLE (int) * fifo;

  const size_t BOTTLE_CAPACITY = 10;
  fifo = BOTTLE_CREATE (int, BOTTLE_CAPACITY);
  printf ("Declared Capacity %zu\n", BOTTLE_CAPACITY);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  write (fifo);
  printf ("Declared Capacity %zu\n", BOTTLE_CAPACITY);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  read (fifo);
  printf ("Declared Capacity %zu\n", BOTTLE_CAPACITY);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  BOTTLE_CLOSE (fifo);
  BOTTLE_DESTROY (fifo);
  printf ("\n");
  fifo = BOTTLE_CREATE (int, UNLIMITED);
  printf ("Declared Capacity %zu\n", BOTTLE_CAPACITY);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  write (fifo);
  printf ("Declared Capacity %zu\n", BOTTLE_CAPACITY);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  read (fifo);
  printf ("Declared Capacity %zu\n", BOTTLE_CAPACITY);
  printf ("Effective capacity %zu\n", QUEUE_CAPACITY (fifo->queue));
  BOTTLE_CLOSE (fifo);
  BOTTLE_DESTROY (fifo);
}
