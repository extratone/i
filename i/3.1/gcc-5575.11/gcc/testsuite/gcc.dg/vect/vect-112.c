/* { dg-require-effective-target vect_int } */

#include <stdarg.h>
#include "tree-vect.h"

#define N 16

int
main1 (void)
{
  int i;
  int diff = 0;
  char cb[N] = {2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17};
  char cc[N] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

  /* Cross-iteration cycle.  */
  diff = 0;
  for (i = 0; i < N; i++) {
    diff += (cb[i] - cc[i]);
  }

  /* Check results.  */
  if (diff != 16)
    abort();

  return 0;
}

int main (void)
{
  check_vect ();
  return main1 ();
}

/* { dg-final { scan-tree-dump-times "vectorized 0 loops" 1 "vect" } } */
/* { dg-final { cleanup-tree-dump "vect" } } */


