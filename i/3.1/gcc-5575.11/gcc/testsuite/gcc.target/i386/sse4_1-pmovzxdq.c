/* APPLE LOCAL file 5612787 mainline sse4 */
/* { dg-do run { target i?86-*-* x86_64-*-* } } */
/* { dg-require-effective-target sse4 } */
/* { dg-options "-O2 -msse4.1" } */

#include "sse4_1-check.h"

#include <smmintrin.h>

#define NUM 128

static void
sse4_1_test (void)
{
  union
    {
      __m128i x[NUM / 2];
      unsigned long long ll[NUM];
      unsigned int i[NUM * 2];
    } dst, src;
  int i;

  for (i = 0; i < NUM; i++)
    {
      src.i[(i % 2) + (i / 2) * 4] = i * i;
      if ((i % 2))
        src.i[(i % 2) + (i / 2) * 4] |= 0x80000000;
    }

  for (i = 0; i < NUM; i += 2)
    dst.x [i / 2] = _mm_cvtepu32_epi64 (src.x [i / 2]);

  for (i = 0; i < NUM; i++)
    if (src.i[(i % 2) + (i / 2) * 4] != dst.ll[i])
      abort ();
}
