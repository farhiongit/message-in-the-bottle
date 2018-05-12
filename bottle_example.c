// Compile with:
// clang -pthread bottle_example.c -o bottle_example
// or
// gcc -pthread bottle_example.c -o bottle_example

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "bottle_impl.h"

// Structure of the messages brought by the bottle.
typedef struct
{
  double x, y;
  char *s;
} Point;

// Declare and define use of bottles for type Point.
/* *INDENT-OFF* */
DECLARE_BOTTLE (Point)
DEFINE_BOTTLE (Point)
/* *INDENT-ON*  */

static int
process_message (Point p)
{
  sleep (1);                    // Just to mimic thread processing the data.
  return 1;
}

// Feeder thread
static void *
feed (void *arg)
{
  const char fmt[] = "(%g, %g)";
  BOTTLE (Point) * bottle = arg;
  for (unsigned int i = 1; i <= 10; i++)
  {
    Point p;
    p.x = 5 * i;
    p.y = 7 * i;
    int size = snprintf (0, 0, fmt, p.x, p.y);

    // The feeder is responsible for any required ressource allocation in the message.
    ASSERT (p.s = malloc ((size + 1) * sizeof (*p.s)));
    snprintf (p.s, size + 1, fmt, p.x, p.y);
    char *scpy = strdup (p.s);

    fprintf (stderr, "Feeder thread %1$#lx: bottle %2$p <- ? ...\n", pthread_self (), (void *) bottle);
    if (!BOTTLE_FILL (bottle, p))       // bottle <- message p
      free (p.s);
    // From here, p could be drained from the bottle by an eater thread, and ressources of p released.
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
    if (BOTTLE_DRAIN (bottle, p))       // message p <- bottle
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
      break;
  }

  fprintf (stderr, "Eater thread %1$#lx finished.\n", pthread_self ());
  return 0;
}

int
main (void)
{
  BOTTLE (Point) * bottle = BOTTLE_CREATE (Point);
  ASSERT (bottle);
  switch (BOTTLE_CAPACITY (bottle))
  {
    case INFINITE_CAPACITY:
      fprintf (stderr, "Bottle %1$p created (buffered with infinite capacity).\n", (void *) bottle);
      break;
    case UNBUFFERED:
      fprintf (stderr, "Bottle %1$p created (unbuffered).\n", (void *) bottle);
      break;
    default:
      fprintf (stderr, "Bottle %1$p created (capacity %2$zu).\n", (void *) bottle, BOTTLE_CAPACITY (bottle));
      break;
  }

  pthread_t eater[3];           // 3 eater threads
  for (unsigned int i = 0; i < sizeof (eater) / sizeof (*eater); i++)
    if (!pthread_create (&eater[i], 0, eat, bottle))    // Pass the bottle as an argument of the eater.
      fprintf (stderr, "Eater thread %#lx started.\n", eater[i]);

  pthread_t feeder;
  if (!pthread_create (&feeder, 0, feed, bottle))       // Pass the bottle as an argument of the feeder.
    fprintf (stderr, "Feeder thread %#lx started.\n", feeder);

  // Wait for all messages to be fed through the bottle.
  pthread_join (feeder, 0);

  // From here, the feeder is done, the bottle won't be filled any more.

  fprintf (stderr, "Bottle %p drying...\n", (void *) bottle);
  // Therefore, we can close the bottle:
  // - prevent any new message from being sent in the bottle
  // - inform all receivers that there won't be any more food sent through the bottle
  // - wait for the bottle to be emptied.
  // - ask for all the receivers to stop waiting for food and to finish their job.
  // This call must be done in the thread (main thread here) that controls the execution of the feeder,
  // after the feeder has finished its work.
  BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY (bottle);
  fprintf (stderr, "Bottle %p dry.\n", (void *) bottle);

  // From here, the bottle has been emptied.

  // Wait for all the receivers to finish their job.
  for (unsigned int i = 0; i < sizeof (eater) / sizeof (*eater); i++)
    pthread_join (eater[i], 0);

  // From here, all eaters are done, and there are not anymore users of the bottle:
  // the bottle can be destroyed safely

  BOTTLE_DESTROY (bottle);
  fprintf (stderr, "Bottle %p destroyed.\n", (void *) bottle);
  fprintf (stderr, "Finished.\n");
}
