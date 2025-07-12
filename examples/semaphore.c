#include <unistd.h>
#include <threads.h>
#include "bottle_impl.h"
#define semaphore_declare bottle_type_declare (char)
#define semaphore_define bottle_type_define (char)
#define sem_t bottle_t (char)
#define semaphore_create(size) bottle_create (char, (size))
#define semaphore_init(sem) do { while (bottle_try_send (sem)); } while (0)
#define semaphore_capacity(sem) bottle_capacity(sem)
#define semaphore_request(sem) bottle_recv (sem)
#define semaphore_release(sem) bottle_send (sem)
#define semaphore_destroy(sem) do { while (bottle_try_recv (sem)); bottle_destroy (sem); } while (0)

semaphore_declare;
semaphore_define;

static int
run_thread (void *arg)
{
  sem_t *sem = arg;
  semaphore_request (sem);
  sleep (1);
  semaphore_release (sem);
  return 0;
}

#define CAP 10
int
main (void)
{
  sem_t *sem = semaphore_create (CAP);
  semaphore_init (sem);

  thrd_t thread_id[4 * CAP];
  for (size_t i = 0; i < sizeof (thread_id) / sizeof (*thread_id); i++)
  {
    thrd_create (&thread_id[i], run_thread, sem);
    printf (".");
    fflush (stdout);
  }
  printf ("\r");
  fflush (stdout);
  for (size_t i = 0; i < sizeof (thread_id) / sizeof (*thread_id); i++)
  {
    thrd_join (thread_id[i], 0);
    printf ("*");
    fflush (stdout);
  }
  printf ("\n");

  semaphore_destroy (sem);
}
