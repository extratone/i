/* Simplified from testcase by David Staepelaere <staapa@ultimatech.com> */

/* { dg-do compile } */
/* { dg-options "" } */
/* { dg-options -mcpu=ultrasparc { target sparc-*-* } } */

int foo(long long y) {
  return -1 * y;
}
