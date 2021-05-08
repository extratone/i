/* PR middle-end/11151 */
/* Originator: Andrew Church <gcczilla@achurch.org> */
/* { dg-do run } */

/* This used to fail on SPARC because the (undefined) return
   value of 'bar' was overwriting that of 'foo'.  */

extern void abort(void);

int foo(int n)
{
  return n+1;
}

int bar(int n)
{
  __builtin_return(__builtin_apply((void (*)(void))foo, __builtin_apply_args(), 64));
}

int main(void)
{
  if (bar(1) != 2)
    abort();

  return 0;
}
