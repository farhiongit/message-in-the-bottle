#include <stdio.h>
#include "bottle_impl.h"
typedef int Token;
DECLARE_BOTTLE (Token);
DEFINE_BOTTLE (Token);

#define PRINT_TOKEN printf (" (%lu/%lu).\n", QUEUE_SIZE((tokens_in_use).queue), BOTTLE_CAPACITY (&tokens_in_use))
#define GET_TOKEN   printf ("Token requested: %s", BOTTLE_TRY_FILL (&tokens_in_use) ? "OK" : "NOK")
#define LET_TOKEN   printf ("Token released:  %s", BOTTLE_TRY_DRAIN (&tokens_in_use) ? "OK" : "NOK")

int main (void)
{
  BOTTLE_DECL (tokens_in_use, Token, 3);

  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;

  LET_TOKEN ; PRINT_TOKEN ;

  GET_TOKEN ; PRINT_TOKEN ;
}
