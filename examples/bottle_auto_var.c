#include <time.h>
#include "bottle_impl.h"

bottle_type_declare (time_t);
bottle_type_define (time_t);

int main (void)
{
  bottle_auto (b, time_t, 1);
  bottle_send (&b, time (0));
  time_t val;
  bottle_recv (&b, &val);
  // b is destroyed automatically.
}
