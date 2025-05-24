#include <time.h>
#include "bottle_impl.h"
bottle_type_declare (int);
bottle_type_define (int);
#define NB_MESSAGES (2 * 1000 * 1000)

static size_t LEVEL[2 * NB_MESSAGES];
static size_t nb_p, nb_c;
static size_t test_number;

static void *eat (void *arg)
{
  bottle_t (int) * bottle = arg;
  // Consumer
  while (bottle_recv (bottle))
    LEVEL[nb_c++ + nb_p] = QUEUE_SIZE (bottle->queue);
  return 0;
}

static void
test1 (void)
{
  size_t test[] = {UNBUFFERED, 1, 1000, UNLIMITED};
  for (size_t t = 0 ; t < sizeof (test) / sizeof (*test) ; t++)
  {
    printf ("*** TEST %lu ***\n", ++test_number);
    clock_t start = clock ();
    nb_p = nb_c = 0;
    bottle_t (int) * bottle = bottle_create (int, test[t]);
    printf ("Declared capacity: %zu\n", BOTTLE_CAPACITY(bottle));
    pthread_t eater;
    pthread_create (&eater, 0, eat, bottle);

    // Producer
    for (size_t i = 0; i < NB_MESSAGES && bottle_send (bottle); i++)
      LEVEL[nb_c + nb_p++] = QUEUE_SIZE (bottle->queue);

    bottle_close (bottle);
    pthread_join (eater, 0);
    bottle_destroy (bottle);

    printf ("%zu messages produced, %zu messages consumed in %f seconds.\n", nb_p, nb_c, (double) (clock () - start) / (double) CLOCKS_PER_SEC);
    size_t avg = 0;
    for (size_t i = 0 ; i < sizeof (LEVEL) / sizeof (*LEVEL) ; i++)
      avg += LEVEL[i];
    printf ("Average buffer size : %f\n\n", ((double) avg) * sizeof (*LEVEL) / sizeof (LEVEL));
  }
}

#define TWICE(n) (2 * (n))
static void *twice (void *arg)
{
  int v;
  bottle_t (int) * bottle = arg;
  while (bottle_recv (bottle, &v) && bottle_send (bottle, TWICE (v))) /**/;
  return 0;
}

static void
test2 (void)
{
  // On an idea from https://wingolog.org/archives/2017/06/29/a-new-concurrent-ml
  size_t test[] = {UNBUFFERED, 1};
  printf ("The unbuffered (0-sized) bottle will succeed while the buffered (1-sized) bottle will fail.\n");
  for (size_t t = 0 ; t < sizeof (test) / sizeof (*test) ; t++)
  {
    printf ("*** TEST %lu ***\n", ++test_number);
    bottle_t (int) * bottle = bottle_create (int, test[t]);
    printf ("Declared capacity: %zu\n", BOTTLE_CAPACITY(bottle));
    pthread_t doubler;
    pthread_create (&doubler, 0, twice, bottle);

    const size_t NB = 10000;
    int v;
    size_t ok = 0;
    size_t nok = 0;
    // The main function send a value, then immediately read a response from the same channel.
    /* If the bottle is buffered (size 1), it might read the sent value instead of the doubled value supplied by the 'twice' thread (it's not deterministic).
       Likewise the double routine could read its responses as its inputs.
       If the bottle is UNBUFFERED, it is a meeting-place:
       - processes meet to exchange values;
       - whichever party arrives first has to wait for the other party to show up;
       - the message that is handed off in send/receive operation is never "owned" by the bottle;
       - it is either owned by a sender who is waiting at the meeting point for a receiver, or it's accepted by a receiver
       - after the transaction is complete, both parties continue on.
       */
    for (size_t i = 0; i < NB && bottle_send (bottle, 20) && bottle_recv (bottle, &v); i++)
      v == TWICE (20) ? ++ok : ++nok;

    bottle_close (bottle);
    pthread_join (doubler, 0);
    bottle_destroy (bottle);
    printf ("%zu OK, %zu NOK (%sas expected.)\n\n", ok, nok, ok + (BOTTLE_CAPACITY(bottle) ? nok : 0) == NB ? "" : "NOT ");
  }
}

int
main (void)
{
  test2 ();
  test1 ();
}
