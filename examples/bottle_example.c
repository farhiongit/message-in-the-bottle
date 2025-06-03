// Compile with:
// clang -pthread bottle_example.c -o bottle_example
// or
// gcc -pthread bottle_example.c -o bottle_example

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "bottle_impl.h"

// Structure of the messages brought by the bottle.
typedef struct
{
  double x, y;
  char *s;
} Point;

// Declare and define use of bottles for type Point.
DECLARE_BOTTLE (Point);
DEFINE_BOTTLE (Point);

static int
process_message (Point p)
{
  (void) p;
  sleep (1);                    // Just to mimic thread processing the data.
  return 1;
}

// Stopper thread
static void *
stop (void *arg)
{
  sleep (2);
  BOTTLE (Point) * bottle = arg;
  BOTTLE_PLUG (bottle);
  fprintf (stderr, "Feeder thread %1$#lx: bottle %2$p PLUGGED.\n", pthread_self (), (void *) bottle);
  return 0;
}

// Starter thread
static void *
restart (void *arg)
{
  sleep (5);
  BOTTLE (Point) * bottle = arg;
  BOTTLE_UNPLUG (bottle);
  fprintf (stderr, "Feeder thread %1$#lx: bottle %2$p UNPLUGGED.\n", pthread_self (), (void *) bottle);
  return 0;
}

// Closer thread
static void *
close_bottle (void *arg)
{
  sleep (7);
  BOTTLE (Point) * bottle = arg;
  fprintf (stderr, "Closer thread %1$#lx: bottle %2$p CLOSING...\n", pthread_self (), (void *) bottle);
  BOTTLE_CLOSE (bottle);
  fprintf (stderr, "Closer thread %1$#lx: bottle %2$p CLOSED and DISPOSING...\n", pthread_self (), (void *) bottle);
  //BOTTLE_WAIT_UNTIL_EMPTY (bottle);
  fprintf (stderr, "Closer thread %1$#lx: bottle %2$p CLOSED and DISPOSED.\n", pthread_self (), (void *) bottle);
  return 0;
}

// Feeder thread
static void *
feed (void *arg)
{
  const char fmt[] = "(%g, %g)";
  BOTTLE (Point) * bottle = arg;
  for (unsigned int i = 1; i <= 26; i++)
  {
    Point p;
    p.x = i;
    p.y = 7 * i;
    int isize = snprintf (0, 0, fmt, p.x, p.y);
    size_t size = (size_t) (isize >= 0 ? isize : 0);
    // The feeder is responsible for any required ressource allocation in the message.
    BOTTLE_ASSERT (p.s = malloc ((size + 1) * sizeof (*p.s)));
    snprintf (p.s, size + 1, fmt, p.x, p.y);
    char *scpy = strdup (p.s);

    fprintf (stderr, "Feeder thread %1$#lx: bottle %2$p <- { (%3$g, %4$g), \"%5$s\" } ?\n",
             pthread_self (), (void *) bottle, p.x, p.y, scpy);
    errno = 0;
    if (!BOTTLE_FILL (bottle, p))       // bottle <- message p
    {
      if (errno == ECONNABORTED)
        fprintf (stderr, "Feeder thread %1$#lx: bottle %2$p WAS CLOSED.\n", pthread_self (), (void *) bottle);
      free (p.s);
    }
    // From here, p has been sent in the bottle and is not owned by the thread anymore:
    // it could be drained from the bottle by an eater thread, and ressources of p released.
    // Ressources of p should not be used once feeded, if not by an eater: scpy is used instead.
    else
      fprintf (stderr, "Feeder thread %1$#lx: bottle %2$p <- { (%3$g, %4$g), \"%5$s\" }.\n",
               pthread_self (), (void *) bottle, p.x, p.y, scpy);
    free (scpy);
  }

  fprintf (stderr, "Feeder thread %1$#lx finished.\n", pthread_self ());
  return 0;
}

// Eater thread
static void *
eat (void *arg)
{
  BOTTLE (Point) * bottle = arg;

  while (1)
  {
    fprintf (stderr, "Eater thread %1$#lx: ? <- bottle %2$p...\n", pthread_self (), (void *) bottle);
    Point p;
    errno = 0;
    if (BOTTLE_DRAIN (bottle, &p))       // message p <- bottle
    {
      fprintf (stderr, "Eater thread %1$#lx: { (%3$g, %4$g), \"%5$s\" } <- bottle %2$p.\n",
               pthread_self (), (void *) bottle, p.x, p.y, p.s);

      fprintf (stderr, "Eater thread %1$#lx: processing { (%2$g, %3$g), \"%4$s\" }...\n",
               pthread_self (), p.x, p.y, p.s);
      process_message (p);
      fprintf (stderr, "Eater thread %1$#lx: processed { (%2$g, %3$g), \"%4$s\" }.\n", pthread_self (), p.x, p.y, p.s);

      // The eater is responsible for ressource allocated by the feeder once the message has been processed.
      free (p.s);
    }
    else
    {
      if (errno == ECONNABORTED)
        fprintf (stderr, "Eater thread %1$#lx: bottle %2$p WAS CLOSED.\n", pthread_self (), (void *) bottle);
      break;
    }
  }

  fprintf (stderr, "Eater thread %1$#lx finished.\n", pthread_self ());
  return 0;
}

int
main (void)
{
  size_t test[] = { UNBUFFERED, 3, UNLIMITED };
  for (size_t t = 0 ; t < sizeof (test) / sizeof (*test) ; t++)
  {
    fprintf (stderr, "*** TEST %lu ***\n", t + 1);
    BOTTLE (Point) * bottle = BOTTLE_CREATE (Point, test[t]);
    BOTTLE_ASSERT (bottle);
    switch (test[t])
    {
      case UNBUFFERED:
        fprintf (stderr, "Bottle %1$p created (unbuffered).\n", (void *) bottle);
        break;
      case UNLIMITED:
        fprintf (stderr, "Bottle %1$p created (unbound, effective capacity %2$zu).\n", (void *) bottle, QUEUE_CAPACITY (bottle->queue));
        break;
      default:
        fprintf (stderr, "Bottle %1$p created (capacity %2$zu).\n", (void *) bottle, test[t]);
        break;
    }

    pthread_t eater[3];           // 3 eater threads
    for (unsigned int i = 0; i < sizeof (eater) / sizeof (*eater); i++)
      if (!pthread_create (&eater[i], 0, eat, bottle))    // Pass the bottle as an argument of the eater.
        fprintf (stderr, "Eater thread %#lx started.\n", eater[i]);

    pthread_t feeder;
    if (!pthread_create (&feeder, 0, feed, bottle))       // Pass the bottle as an argument of the feeder.
      fprintf (stderr, "Feeder thread %#lx started.\n", feeder);

    pthread_t stopper;
    if (!pthread_create (&stopper, 0, stop, bottle))      // Pass the bottle as an argument.
      fprintf (stderr, "Stopper thread %#lx started.\n", stopper);

    pthread_t starter;
    if (!pthread_create (&starter, 0, restart, bottle))   // Pass the bottle as an argument.
      fprintf (stderr, "Starter thread %#lx started.\n", starter);

    pthread_t closer;
    if (!pthread_create (&closer, 0, close_bottle, bottle))       // Pass the bottle as an argument.
      fprintf (stderr, "Closer thread %#lx started.\n", closer);

    // Wait for all messages to be fed through the bottle.
    pthread_join (feeder, 0);

    // From here, the feeder is done, the bottle won't be filled any more.

    // Therefore, we can close the bottle:
    // 1. prevents any new message from being sent in the bottle.
    // 2. waits for the bottle to be emptied by the eaters.
    // 3. asks for any blocked receivers to stop waiting for food and to finish their job.
    fprintf (stderr, "Bottle %p closing...\n", (void *) bottle);
    BOTTLE_CLOSE (bottle);
    fprintf (stderr, "Bottle %p closed.\n", (void *) bottle);

    // From here, the bottle has been emptied.

    // Wait for all the receivers to finish their job.
    for (unsigned int i = 0; i < sizeof (eater) / sizeof (*eater); i++)
      pthread_join (eater[i], 0);

    // From here, all eaters are done, and there are not anymore users of the bottle:
    // the bottle can be destroyed safely

    fprintf (stderr, "Bottle %p dispose...\n", (void *) bottle);
    //BOTTLE_WAIT_UNTIL_EMPTY (bottle);
    fprintf (stderr, "Bottle %p disposed.\n", (void *) bottle);

    pthread_join (starter, 0);
    pthread_join (stopper, 0);
    pthread_join (closer, 0);
    switch (test[t])
    {
      case UNBUFFERED:
        fprintf (stderr, "Bottle %1$p destroyed (unbuffered).\n", (void *) bottle);
        break;
      case UNLIMITED:
        fprintf (stderr, "Bottle %1$p destroyed (unbound, effective capacity %2$zu).\n", (void *) bottle, QUEUE_CAPACITY (bottle->queue));
        break;
      default:
        fprintf (stderr, "Bottle %1$p destroyed (capacity %2$zu).\n", (void *) bottle, test[t]);
        break;
    }
    BOTTLE_DESTROY (bottle);
    fprintf (stderr, "Finished.\n\n");
  }
}
