#include "bottle_impl.h"
DECLARE_BOTTLE (int);
DEFINE_BOTTLE (int);
#define NB_MESSAGES 25000000
#define BUFFER_SIZE 1000
#define NB_BIN 100
//#define BUFFER_SIZE UNBUFFERED

static size_t LEVEL[2 * NB_MESSAGES];
static size_t nb_p, nb_c;

static void *eat (void *arg)
{
  bottle_t (int) * bottle = arg;
  while (bottle_recv (bottle))
    LEVEL[nb_c++ + nb_p] = QUEUE_SIZE (bottle->queue);
  return 0;
}

int
main (void)
{
  bottle_t (int) * bottle = bottle_create (int, BUFFER_SIZE);
  pthread_t eater;
  pthread_create (&eater, 0, eat, bottle);

  for (size_t i = 0; i < NB_MESSAGES && bottle_send (bottle); i++)
    LEVEL[nb_c + nb_p++] = QUEUE_SIZE (bottle->queue);

  bottle_close (bottle);
  pthread_join (eater, 0);
  bottle_destroy (bottle);

  printf ("%zu messages produced, %zu messages consumed.\n", nb_p, nb_c);
}
