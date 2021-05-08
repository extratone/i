/* { dg-require-effective-target vect_int } */

#include <stdarg.h>
#include "tree-vect.h"

#define N 8
#define OFF 4

/* Check handling of accesses for which the "initial condition" -
   the expression that represents the first location accessed - is
   more involved than just an ssa_name.  */

int ib[N+OFF] __attribute__ ((__aligned__(16))) = {0, 1, 3, 5, 7, 11, 13, 17, 0, 2, 6, 10};

int main1 (int *pib)
{
  int i;
  int ia[N+OFF];
  int ic[N+OFF] = {0, 1, 3, 5, 7, 11, 13, 17, 0, 2, 6, 10};

  for (i = OFF; i < N; i++)
    {
      ia[i] = pib[i - OFF];
    }


  /* check results:  */
  for (i = OFF; i < N; i++)
    {
     if (ia[i] != pib[i - OFF])
        abort ();
    }

  for (i = 0; i < N; i++)
    {
      ia[i] = pib[i - OFF];
    }


  /* check results:  */
  for (i = 0; i < N; i++)
    {
     if (ia[i] != pib[i - OFF])
        abort ();
    }

  for (i = OFF; i < N; i++)
    {
      ia[i] = ic[i - OFF];
    }


  /* check results:  */
  for (i = OFF; i < N; i++)
    {
     if (ia[i] != ic[i - OFF])
        abort ();  
    }

  return 0;  
}

int main (void)
{
  check_vect ();

  main1 (&ib[OFF]);
  return 0;
}


/* { dg-final { scan-tree-dump-times "vectorized 3 loops" 1 "vect" } } */
/* { dg-final { scan-tree-dump-times "Vectorizing an unaligned access" 2 "vect" { xfail vect_no_align } } } */
/* { dg-final { cleanup-tree-dump "vect" } } */
