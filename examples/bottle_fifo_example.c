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
  BOTTLE (int) * fifo = BOTTLE_CREATE (int, 10);

  write (fifo);
  read (fifo);

  BOTTLE_DESTROY (fifo);
}
