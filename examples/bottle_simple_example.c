#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include "bottle_impl.h"        // Include necessary stuff
typedef const char *Message;
bottle_type_declare (Message);  // Declare the template for type 'Message'
bottle_type_define (Message);   // Define the template for type 'Message'

static void *
eat (void *arg)                 // The thread that receives the messages
{
  bottle_t (Message) * bottle = arg;
  Message m;
  while (bottle_recv (bottle, &m))       // Receive a message
    printf ("...%s\n", m);
  return 0;
}

int
main (void)
{
  bottle_t (Message) * bottle = bottle_create (Message);        // create a bottle (on the sender side), unbuffered by default, for communication and synchronization

  // 10 consumers
  pthread_t eater[10];
  for (pthread_t * p = eater; p < eater + sizeof (eater) / sizeof (*eater); p++)
    pthread_create (p, 0, eat, bottle);

  // 1 producer
  Message police[] = { "I'll send an SOS to the world", "I hope that someone gets my", "Message in a bottle" };
  for (Message * m = police; m < police + sizeof (police) / sizeof (*police); m++)
  {
    printf ("%s...\n", *m);
    bottle_send (bottle, *m);   // Send a message
    sleep (1);
  }

  bottle_close (bottle);        // close the bottle (tells the receiver threads that all messages have been sent.)
  for (pthread_t * p = eater; p < eater + sizeof (eater) / sizeof (*eater); p++)
    pthread_join (*p, 0);       // Waits for the receiver thread to finish its work (it uses the bottle).
  bottle_destroy (bottle);      // Destroys the bottle once receiver threads are over.
}
