/* APPLE LOCAL file AV data dependence */
/* { dg-do run } */
/* { dg-options "-O2 -ftree-vectorize -fdump-tree-vect-stats -maltivec" { target powerpc*-*-* } } */
/* { dg-options "-O2 -ftree-vectorize -fdump-tree-vect-stats -msse2" { target i?86-*-* x86_64-*-* } } */

#include <stdarg.h>
#include <signal.h>

#define N 64
#define MAX 42

extern void abort(void); 

int main ()
{  
  int A[N];
  int B[N];
  int C[N];
  int D[N];

  int i, j;

  for (i = 0; i < N; i++)
    {
      A[i] = i;
      B[i] = i;
      C[i] = i;
      D[i] = i;
    }

  /* Vectorizable */
  for (i = 0; i < 16; i++)
    {
      A[i] = A[i+20];
    }

  /* check results:  */
  for (i = 0; i < 16; i++)
    {
      if (A[i] != A[i+20])
	abort ();
    }

  /* Vectorizable */
  for (i = 0; i < 16; i++)
    {
      B[i] = B[i] + 5;
    }

  /* check results:  */
  for (i = 0; i < 16; i++)
    {
      if (B[i] != C[i] + 5)
	abort ();
    }

  /* Not vectorizable */
  for (i = 0; i < 4; i++)
    {
      C[i] = C[i+3];
    }

  /* check results:  */
  for (i = 0; i < 4; i++)
    {
      if (C[i] != D[i+3])
	abort ();
    }


  return 0;
}



/* { dg-final { scan-tree-dump-times "vectorized 2 loops" 1 "vect" } } */
