/* APPLE LOCAL file 4196953 */
/* Test to check type def stabs are not nested insinde any other stabs, in -gused mode.  */
/* Contributed by Devang Patel  <dpatel@apple.com>  */

/* { dg-do compile } */
/* { dg-skip-if "No stabs" { mmix-*-* *-*-aix* *-*-netware* alpha*-*-* hppa*64*-*-* ia64-*-* } { "*" } { "" } } */
/* { dg-options "-gstabs+ -feliminate-unused-debug-symbols" } */

static double d = 0.42;

void foobar()
{
  double d = 1.0;
}

/* { dg-final { scan-assembler ".stabs.*double:t\\(\[0-9\],\[0-9\]\\)\=r\\(\[0-0\],\[0-9\]\\);\[0-9\];\[0-\9];" } } */


