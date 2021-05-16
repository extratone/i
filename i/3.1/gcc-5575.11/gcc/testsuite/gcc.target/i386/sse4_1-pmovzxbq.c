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
      unsigned char c[NUM * 8];
    } dst, src;
  int i;

  for (i = 0; i < NUM; i++)
    {
      src.c[(i % 2) + (i / 2) * 16] = i * i;
      if ((i % 2))
	src.c[(i % 2) + (i / 2) * 16] |= 0x80;
    }

  for (i = 0; i < NUM; i += 2)
    dst.x [i / 2] = _mm_cvtepu8_epi64 (src.x [i / 2]);

  for (i = 0; i < NUM; i++)
    if (src.c[(i % 2) + (i / 2) * 16] != dst.ll[i])
      abort ();
}
