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

static void *
repeat_moves (void *arg)
{
  size_t nb_rings = (*(struct thread_args *) arg).nb_rings;
  char from = (*(struct thread_args *) arg).from;
  char to = (*(struct thread_args *) arg).to;
  char *rings = malloc (nb_rings * sizeof (*rings));
  for (size_t ring = 0; ring < nb_rings; ring++)
    rings[ring] = from;
  fprintf (stderr, "%c :< ", from);
  for (size_t i = 0; i < nb_rings; i++)
    fprintf (stderr, "%zu ", i + 1);
  fprintf (stderr, "|\n");
  bottle_t (Move) *moves_queue = (*(struct thread_args *) arg).moves_queue;

  size_t nb_moves = 0;
  Move m;
  while (bottle_recv (moves_queue, &m))
    for (size_t ring = 0; ring < nb_rings; ring++)
      if (rings[ring] == m.from)
      {
        rings[ring] = m.to;
        fprintf (stderr, "After a move of ring %zu from %c to %c:\n", ring + 1, m.from, m.to);
        nb_moves++;
        fprintf (stderr, "%c :< ", m.from);
        for (size_t i = 0; i < nb_rings; i++)
          if (rings[i] == m.from)
            fprintf (stderr, "%zu ", i + 1);
        fprintf (stderr, "|\n");
        fprintf (stderr, "%c :< ", m.to);
        for (size_t i = 0; i < nb_rings; i++)
          if (rings[i] == m.to)
            fprintf (stderr, "%zu ", i + 1);
        fprintf (stderr, "|\n");
        break;
      }

  unsigned long *ret = malloc (sizeof (*ret));
  *ret = nb_moves;
  for (size_t i = 0; i < nb_rings; i++)
    if (rings[i] != to)
      *ret = 0;
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
  printf ("Ask for moving ring %zu from %c to %c.\n", upper_rings, from, to);
  Move m = {.from = from,.to = to };
  bottle_send (b, m);

  return nb_moves += move_rings (upper_rings - 1, intermediate, to, from, b);
}

int
main (int argc, char **argv)
{
  setlocale (LC_ALL, "");

  size_t nb_rings = argc > 1 ? strtoul (argv[1], 0, 0) : 8;
  if (!nb_rings)
    return EXIT_FAILURE;

  bottle_t (Move) *moves_queue = bottle_create (Move, UNLIMITED);      // The queue has infinite size.

  pthread_t displayer;
  struct thread_args args = {.nb_rings = nb_rings,.moves_queue = moves_queue,.from = 'A',.to = 'C' };
  pthread_create (&displayer, 0, repeat_moves, &args);

  unsigned long nb_moves = move_rings (nb_rings, 'A', 'C', 'B', moves_queue);
  printf ("SOLVED. Moving %lu rings from %c to %c requires %lu moves.\n", nb_rings, 'A', 'C', nb_moves);

  bottle_close (moves_queue);
  void *ret;
  pthread_join (displayer, &ret);
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
