/* APPLE LOCAL file mainline */
/* { dg-do run { target native } } */
/* { dg-options "-fstack-protector" } */
#include <stdlib.h>

static void
__stack_chk_fail (void)
{
  exit (0); /* pass */
}

int main ()
{
  int i;
  char foo[255];

  // smash stack
  for (i = 0; i <= 400; i++)
    foo[i] = 42;

  return 1; /* fail */
}
