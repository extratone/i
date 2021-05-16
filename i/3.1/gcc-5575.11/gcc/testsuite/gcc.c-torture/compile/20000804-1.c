/* This does not work on m68hc11 or h8300 due to the use of an asm
   statement to force a 'long long' (64-bits) to go in a register.  */
/* { dg-do assemble { xfail m6811-*-* m6812-*-* h8300-*-* } } */
/* { dg-skip-if "" { { i?86-*-* x86_64-*-* } && ilp32 } { "-fpic" "-fPIC" } { "" } } */
/* { dg-skip-if "PIC default" { i?86-*-darwin* } { "*" } { "" } } */
/* { dg-skip-if "No 64-bit registers" { m32c-*-* } { "*" } { "" } } */

/* Copyright (C) 2000, 2003 Free Software Foundation */
__complex__ long long f ()
{
  int i[99];
  __complex__ long long v;

  v += f ();
  asm("": "+r" (v) : "r" (0), "r" (1));
  v = 2;
  return v;
  g (&v);
}
