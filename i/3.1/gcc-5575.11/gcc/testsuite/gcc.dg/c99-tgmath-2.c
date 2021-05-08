/* APPLE LOCAL file mainline 2007-06-12 2872232 */
/* Test for <tgmath.h> in C99. */
/* Origin: Matt Austern <austern@apple.com>
/* { dg-do compile } */
/* { dg-options "-std=iso9899:1999" } */

/* Test that invoking type-generic sin on a float invokes sinf. */
#include <tgmath.h>

float foo(float x)
{
  return sin(x);
}

/* {dg-final {scan-assembler "sinf" } } */
