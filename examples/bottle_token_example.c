#include <stdio.h>
#include "bottle_impl.h"
typedef int Token;
DECLARE_BOTTLE (Token);
DEFINE_BOTTLE (Token);

#define PRINT_TOKEN printf (" (%lu/%lu).\n", QUEUE_SIZE((tokens_in_use).queue), BOTTLE_CAPACITY)
#define GET_TOKEN   printf ("Token requested: %s", BOTTLE_TRY_FILL (&tokens_in_use) ? "OK" : "NOK")
#define LET_TOKEN   printf ("Token released:  %s", BOTTLE_TRY_DRAIN (&tokens_in_use) ? "OK" : "NOK")
#define GET_AND_PRINT do { GET_TOKEN; PRINT_TOKEN; } while (0)
#define LET_AND_PRINT do { LET_TOKEN; PRINT_TOKEN; } while (0)

int
main (void)
{
  const size_t BOTTLE_CAPACITY = 3;
  BOTTLE_DECL (tokens_in_use, Token, BOTTLE_CAPACITY);

  GET_AND_PRINT;
  GET_AND_PRINT;
  GET_AND_PRINT;
  GET_AND_PRINT;
  GET_AND_PRINT;

  LET_AND_PRINT;

  GET_AND_PRINT;

  LET_AND_PRINT;
}
