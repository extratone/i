/* APPLE LOCAL file mainline 4.2 5569774 */
/* { dg-require-effective-target vect_int } */

#include <stdlib.h>
#include <stdarg.h>
#include "tree-vect.h"

/* Compile time known misalignment. Cannot use loop peeling to align
   the store.  */

#define N 16

struct foo {
  char x;
  int y[N];
} __attribute__((packed));

int
main1 (struct foo * __restrict__ p)
{
  int i;
  int x[N] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

  for (i = 0; i < N; i++)
    {
      p->y[i] = x[i];
    }

  /* check results:  */
  for (i = 0; i < N; i++)
    {
      if (p->y[i] != x[i])
	abort ();
    }
  return 0;
}


int main (void)
{
  int i;
  struct foo *p = malloc (2*sizeof (struct foo));
  check_vect ();
  
  main1 (p);
  return 0;
}

/* { dg-final { scan-tree-dump-times "Alignment of access forced using versioning" 1 "vect" } } */
/* { dg-final { scan-tree-dump-times "vectorized 1 loops" 1 "vect" } } */
/* { dg-final { cleanup-tree-dump "vect" } } */
