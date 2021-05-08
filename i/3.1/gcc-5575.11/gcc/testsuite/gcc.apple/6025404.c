/* APPLE LOCAL file 6025404 */
/* { dg-do run { target i?86*-*-darwin* } } */
/* { dg-options "-O3 -mssse3" } */
#include <stdio.h>
#include <tmmintrin.h>
void foo(__m128i *a, __m128i *b, __m128i c, __m128i d, __m128i e)
{
  *a = _mm_maddubs_epi16(c, d);
  *b = _mm_maddubs_epi16(c, e);
}
int main(void)
{
  union { char c[16]; __m128i v; } c = { -1 }, d = { 1 }, e = { 1 };
  union { short s[8]; __m128i v; } a, b;

  foo(&a.v, &b.v, c.v, d.v, e.v);

  if (0)
    {
      printf("a contains %d.\n", a.s[0]); 
      printf("b contains %d.\n", b.s[0]);
    }

  if (a.s[0] != 255)
    abort () ;
  if (b.s[0] != 255)
    abort () ;

  return 0;
}
