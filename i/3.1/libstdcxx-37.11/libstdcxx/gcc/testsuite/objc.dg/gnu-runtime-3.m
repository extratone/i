/* Sanity check for GNU-runtime regardless of runtime used on target system.  */

/* { dg-do run } */
/* { dg-options "-fgnu-runtime" } */

#include <objc/Object.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, void **args)
{
  [Object new];
  return 0;
}
