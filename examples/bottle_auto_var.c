#include <time.h>
#include "bottle_impl.h"

DECLARE_BOTTLE (time_t);
DEFINE_BOTTLE (time_t);

int main (void)
{
  BOTTLE_DECL (b, time_t);
  bottle_send (&b, time (0));
  time_t val;
  bottle_recv (&b, val);
}
