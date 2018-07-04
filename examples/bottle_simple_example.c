#include <unistd.h>
#include <stdio.h>

#include "bottle_impl.h"
typedef const char *Message;
DECLARE_BOTTLE (Message);       // Declares the template for type 'Message' (note, no trailing ';')
DEFINE_BOTTLE (Message);        // Defines the template for type 'Message' (note, no trailing ';')

static void *
eat (void *arg)                 // The thread that receives the messages
{
  bottle_t (Message) * bottle = arg;
  Message m;
  while (bottle_recv (bottle, m))
    printf ("%s\n", m);
  return 0;
}

static void *
feed (void *arg)
{
  bottle_t (Message) * bottle = arg;
  Message police[] = { "I'll send an SOS to the world", "I hope that someone gets my", "Message in a bottle" };
  for (size_t i = 0; i < 2 * sizeof (police) / sizeof (*police); i++)
  {
    bottle_send (bottle, police[i / 2]);
    sleep (1);
  }
  return 0;
}

int
main (void)                     // The (main) thread that creates the bottle
{
  bottle_t (Message) * bottle = bottle_create (Message);

  pthread_t eater;
  pthread_create (&eater, 0, eat, bottle);

#ifndef FEEDER_THREAD           // Option 1: the sender is in the same thread as the one that created it.

  feed (bottle);

#else // Option 2: the sender is in a dedicated thread.

  pthread_t feeder;
  pthread_create (&feeder, 0, feed, bottle);

  pthread_join (feeder, 0);     // Waits for the sender to finish.

#endif

  bottle_close (bottle);        // Tells the receiver thread that the sending thread has finished sending messages
  pthread_join (eater, 0);      // Waits for the receiver thread to finish its work (it uses the bottle).
  bottle_destroy (bottle);      // Destroys the bottle once both threads are over.
}
