/* PR target/21149 */
/* { dg-do run } */
/* { dg-options "-O2 -msse" } */
#include <xmmintrin.h>
#include "../../gcc.dg/i386-cpuid.h"

extern void abort (void);

void
__attribute__((noinline))
check (__m128 x, float a, float b, float c, float d)
{
  union { __m128 m; float f[4]; } u;
  u.m = x;
  if (u.f[0] != a || u.f[1] != b || u.f[2] != c || u.f[3] != d)
    abort ();
}

static inline
void
foo (__m128 *x)
{
  __m128 y = _mm_setzero_ps ();
  __m128 v = _mm_movehl_ps (y, *x);
  __m128 w = _mm_movehl_ps (*x, y);
  check (*x, 9, 1, 2, -3);
  check (v, 2, -3, 0, 0);
  check (w, 0, 0, 2, -3);
}

void
__attribute__((noinline))
run_tests (void)
{
  __m128 y = _mm_set_ps (-3, 2, 1, 9);
  foo (&y);
}

int
main ()
{
  unsigned long cpu_facilities;

  cpu_facilities = i386_cpuid ();

  if ((cpu_facilities & (bit_MMX | bit_SSE | bit_CMOV))
      != (bit_MMX | bit_SSE | bit_CMOV))
    /* If host has no vector support, pass.  */
    return 0;

  run_tests ();
  return 0;
}
