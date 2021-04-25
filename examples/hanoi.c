#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <locale.h>
#include <unistd.h>
#ifndef TRACE
#  define NDEBUG
#endif
#include <assert.h>
#include <pthread.h>

enum tower
{
  A = 'A',
  B = 'B',
  C = 'C',
};

#ifdef TRACE
#  include "bottle_impl.h"
typedef struct move
{
  unsigned long ring;
  enum tower from, to;
} Move;

bottle_type_declare (Move);
bottle_type_define (Move);

typedef struct
{
  enum tower name;
  unsigned long hight;
  unsigned long *rings;
} Stack;

static Stack stacks[3] = {
  {.name = A,.hight = 0,.rings = 0,},
  {.name = B,.hight = 0,.rings = 0,},
  {.name = C,.hight = 0,.rings = 0,},
};

static Stack *
stack_get_from_name (enum tower name)
{
  for (size_t i = 0; i < sizeof (stacks) / sizeof (*stacks); i++)
  {
    if (name == stacks[i].name)
      return &stacks[i];
  }
  errno = EINVAL;
  return 0;
}

static unsigned long
stack_add (enum tower name, unsigned long ring)
{
  Stack *stack = stack_get_from_name (name);
  if (!stack)
    return 0;
  stack->hight++;
  stack->rings = realloc (stack->rings, sizeof (*stack->rings) * stack->hight);
  stack->rings[stack->hight - 1] = ring;
  return stack->hight;
}

static unsigned long
stack_remove (enum tower name)
{
  Stack *stack = stack_get_from_name (name);
  if (!stack || !stack->hight)
    return 0;
  stack->hight--;
  stack->rings = realloc (stack->rings, sizeof (*stack) * stack->hight);
  if (!stack->hight)
    stack->rings = 0;
  return stack->hight;
}

static void
stack_print (enum tower name)
{
  Stack *stack = stack_get_from_name (name);
  if (!stack)
    return;
  fprintf (stderr, "%c:", stack->name);
  if (stack->hight)
    for (unsigned long i = 0; i < stack->hight; i++)
      fprintf (stderr, " %lu", stack->rings[i]);
  fprintf (stderr, "\n");
}

static void
move_ring (unsigned long ring, enum tower from, enum tower to)
{
  stack_remove (from);
  stack_add (to, ring);
  for (size_t i = 0 ; i < sizeof (stacks) / sizeof (*stacks) ; i++)
    stack_print (stacks[i].name);
}
#endif

static unsigned long
move_rings (unsigned long upper_rings, enum tower from, enum tower to, enum tower intermediate
#ifdef TRACE
            , bottle_t (Move) * b
#endif
  )
{
  if (upper_rings == 0)
    return 0;

  unsigned long nb_moves = move_rings (upper_rings - 1, from, intermediate, to
#ifdef TRACE
                                       , b
#endif
    );

  nb_moves++;
#ifdef TRACE
  Move m = {.ring = upper_rings,.from = from,.to = to };
  bottle_send (b, m);
#else
  fprintf (stderr, "Move ring %lu from %c to %c.\n", upper_rings, from, to);
#endif

  return nb_moves += move_rings (upper_rings - 1, intermediate, to, from
#ifdef TRACE
                                 , b
#endif
    );
}

struct thread_args
{
  unsigned long nb_rings;
#ifdef TRACE
    bottle_t (Move) * moves;
#endif
};

static void *
solve (void *arg)
{
  unsigned long nb_rings = (*(struct thread_args *) arg).nb_rings;
#ifdef TRACE
  bottle_t (Move) * b = (*(struct thread_args *) arg).moves;
#endif
  assert (A != B && B != C && C != A);
  unsigned long nb_moves = move_rings (nb_rings, A, C, B
#ifdef TRACE
                                       , b
#endif
    );
#ifdef TRACE
  bottle_close (b);
#endif
  unsigned long *ret = malloc (sizeof (*ret));
  *ret = nb_moves;
  //printf ("Moving %lu rings from %c to %c requires %lu moves.\n", nb_rings, A, C, nb_moves);
  return ret;
}

int
main (int argc, char **argv)
{
  setlocale (LC_ALL, "");

  pthread_t solver;
  unsigned long nb_rings = argc > 1 ? strtoul (argv[1], 0, 0) : 8;
  struct thread_args solver_args = {.nb_rings = nb_rings };
#ifdef TRACE
  for (unsigned long ring = nb_rings; ring > 0; ring--)
    stack_add (A, ring);
  solver_args.moves = bottle_create (Move);
#endif
  pthread_create (&solver, 0, solve, &solver_args);

#ifdef TRACE
  Move m;
  while (bottle_recv (solver_args.moves, m))
  {
    fprintf (stderr, "Move ring %lu from %c to %c.\n", m.ring, m.from, m.to);
    move_ring (m.ring, m.from, m.to);
  }
  while (stack_remove (C))
     /**/;
#endif

  void *ret;
  pthread_join (solver, &ret);
  printf ("Moving %lu rings from %c to %c requires %lu moves.\n", nb_rings, A, C, *(unsigned long*)ret);
  free (ret);
#ifdef TRACE
  bottle_destroy (solver_args.moves);
#endif
}
