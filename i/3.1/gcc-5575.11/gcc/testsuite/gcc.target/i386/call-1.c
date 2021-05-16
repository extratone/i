/* PR optimization/11304 */
/* Originator: <manuel.serrano@sophia.inria.fr> */
/* { dg-do run { target i?86-*-* x86_64-*-* } } */
/* { dg-options "-O -fomit-frame-pointer" } */

/* Verify that %eax is always restored after a call.  */

extern void abort(void);

volatile int r;

void set_eax(int val)
{
  __asm__ __volatile__ ("mov %0, %%eax" : : "m" (val));
}

void foo(int val)
{
  r = val;
}

int bar(int x)
{
  if (x)
  {
    set_eax(0);
    return x;
  }

  foo(x);
}

int main(void)
{
  if (bar(1) != 1)
    abort();

  return 0;
}
