/* { dg-do run { target i?86-*-* } } */
/* { dg-require-effective-target ilp32 } */
/* { dg-options "-std=gnu99 -msse2" } */
#include <emmintrin.h>
__m128i __attribute__ ((__noinline__))
vector_using_function ()
{
  volatile __m128i vx;	/* We want to force a vector-aligned store into the stack.  */
  vx = _mm_xor_si128 (vx, vx);
  return vx;
}
int __attribute__ ((__noinline__, __force_align_arg_pointer__))
self_aligning_function (int x, int y)
{
  __m128i ignored = vector_using_function ();
  return (x + y);
}
int g_1 = 20;
int g_2 = 22;
int
main ()
{
  int result;
  asm ("pushl %esi");		/* Disalign runtime stack.  */
  result = self_aligning_function (g_1, g_2);
  asm ("popl %esi");
  if (result != 42)
    abort ();
}
