#include <stdio.h>
#include <locale.h>
#include <pthread.h>

#include "bottle_impl.h"
typedef struct move
{
  char from, to;
} Move;
bottle_type_declare (Move);
bottle_type_define (Move);

struct thread_args
{
  size_t nb_rings;
  char from, to;
  bottle_t (Move) *moves_queue;
};

static void
print_peg (size_t nb_rings, char *rings, char peg)
{
  fprintf (stderr, "%c :|-", peg);
  for (size_t i = nb_rings ; i > 0 ; i--)
    if (rings[i - 1] == peg)
      fprintf (stderr, "%zu-", i);
  fprintf (stderr, ">\n");
}

static void *
repeat_moves (void *arg)
{
  size_t nb_rings = (*(struct thread_args *) arg).nb_rings;
  char from = (*(struct thread_args *) arg).from;
  char to = (*(struct thread_args *) arg).to;
  unsigned long *ret = malloc (sizeof (*ret));
  *ret = (unsigned long) -1;
  char *rings = malloc (nb_rings * sizeof (*rings));  // Array of rings, from the smallest to the largest.
  for (size_t ring = 0; ring < nb_rings; ring++)
    rings[ring] = from;
  fprintf (stderr, "Starting with:\n");
  print_peg (nb_rings, rings, from);
  bottle_t (Move) *moves_queue = (*(struct thread_args *) arg).moves_queue;

  size_t nb_moves = 0;
  Move m;
  while (bottle_recv (moves_queue, &m))
    for (size_t ring = 0; ring < nb_rings; ring++)
      if (rings[ring] == m.from)
      {
        rings[ring] = m.to;
        nb_moves++;
        fprintf (stderr, "OK, I move the ring %zu from peg %c to peg %c, therefore:\n", ring + 1, m.from, m.to);
        print_peg (nb_rings, rings, m.from);
        print_peg (nb_rings, rings, m.to);
        break;
      }
      else if (rings[ring] == m.to)
        return ret;  // Forbidden move.

  fprintf (stderr, "Ending with:\n");
  print_peg (nb_rings, rings, to);
  fprintf (stderr, "after %zu moves.\n", nb_moves);
  *ret = nb_moves;
  for (size_t i = 0; i < nb_rings; i++)
    if (rings[i] != to)
      *ret = (unsigned long) -1;
  free (rings);
  return ret;
}

static unsigned long
move_rings (size_t upper_rings, char from, char to, char intermediate, bottle_t (Move) *b)
{
  if (upper_rings == 0)
    return 0;

  unsigned long nb_moves = move_rings (upper_rings - 1, from, intermediate, to, b);

  nb_moves++;
  printf ("Please move the ring %zu from peg %c to peg %c.\n", upper_rings, from, to);
  Move m = {.from = from,.to = to };
  bottle_send (b, m);

  return nb_moves += move_rings (upper_rings - 1, intermediate, to, from, b);
}

int
main (int argc, char **argv)
{
  setlocale (LC_ALL, "");
  size_t nb_rings = argc > 1 ? strtoul (argv[1], 0, 0) : 8;
  bottle_t (Move) *moves_queue = bottle_create (Move, UNLIMITED);      // The queue has infinite size.

  pthread_t repeater;
  struct thread_args args = {.nb_rings = nb_rings,.moves_queue = moves_queue,.from = 'A',.to = 'C' };
  pthread_create (&repeater, 0, repeat_moves, &args);

  unsigned long nb_moves = move_rings (nb_rings, 'A', 'C', 'B', moves_queue);
  printf ("SOLVED. Moving %lu rings from peg %c to peg %c requires %lu moves.\n", nb_rings, 'A', 'C', nb_moves);

  bottle_close (moves_queue);
  void *ret = 0;
  pthread_join (repeater, &ret);
  bottle_destroy (moves_queue);

  if (ret && nb_moves == *(unsigned long *) ret && (nb_moves + 1 == 1UL << nb_rings))
  {
    printf ("OK.\n");
    free (ret);
    return EXIT_SUCCESS;
  }
  else
  {
    printf ("NOK.\n");
    free (ret);
    return EXIT_FAILURE;
  }
}
