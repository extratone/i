/* { dg-do run { target vect_cmdline_needed } } */
/* { dg-options "-O2 -ftree-vectorize -ftree-vectorizer-verbose=3 -fdump-tree-vect-stats" } */

#include <stdlib.h>

#define N 128

#if __LONG_MAX__ == 2147483647
typedef short half_word;
#else
typedef int half_word;
#endif

int main_1 (int n, int *p)
{
  int i;
  half_word ib[N];
  half_word ia[N];
  int k;

  for (i = 0; i < N; i++)
    {
      ia[i] = n;
    }

  /* check results:  */
  for (i = 0; i < N; i++)
    {
      if (ia[i] != n)
        abort ();
    }

  k = *p;
  for (i = 0; i < N; i++)
    {
      ib[i] = k;
    }

  /* check results:  */
  for (i = 0; i < N; i++)
    {
      if (ib[i] != k)
        abort ();
    }

  return 0;
}

int main (int n)
{
  return main_1 (n + 2, &n);
}

/* { dg-final { scan-tree-dump-times "vectorized 2 loops" 1 "vect" } } */
/* { dg-final { scan-tree-dump-times "Vectorizing an unaligned access" 0 "vect" } } */
/* { dg-final { cleanup-tree-dump "vect" } } */
