/* APPLE LOCAL file 5612787 mainline sse4 */
/* { dg-do run { target i?86-*-* x86_64-*-* } } */
/* { dg-require-effective-target sse4 } */
/* { dg-options "-O2 -msse4.1" } */

#include "sse4_1-check.h"

#include <smmintrin.h>

static void
sse4_1_test (void)
{
  union
    {
      __m128 x;
      float f[4];
    } vals[4], val;
  int i, j;

  val.f[0]= 1.;
  val.f[1]= 2.;
  val.f[2]= 3.;
  val.f[3]= 4.;

  vals[0].x = _MM_PICK_OUT_PS (val.x, 0);
  vals[1].x = _MM_PICK_OUT_PS (val.x, 1);
  vals[2].x = _MM_PICK_OUT_PS (val.x, 2);
  vals[3].x = _MM_PICK_OUT_PS (val.x, 3);

  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      if ((j != 0 && vals[i].f[j] != 0)
	  || (j == 0 && vals[i].f[j] != val.f[i]))
	abort ();

  if (_MM_MK_INSERTPS_NDX(0, 0, 0x1) != 0x01
      || _MM_MK_INSERTPS_NDX(0, 1, 0x2) != 0x12
      || _MM_MK_INSERTPS_NDX(0, 2, 0x3) != 0x23
      || _MM_MK_INSERTPS_NDX(0, 3, 0x4) != 0x34
      || _MM_MK_INSERTPS_NDX(1, 0, 0x5) != 0x45
      || _MM_MK_INSERTPS_NDX(1, 1, 0x6) != 0x56
      || _MM_MK_INSERTPS_NDX(2, 2, 0x7) != 0xA7
      || _MM_MK_INSERTPS_NDX(3, 3, 0x8) != 0xF8)
    abort ();
} 
