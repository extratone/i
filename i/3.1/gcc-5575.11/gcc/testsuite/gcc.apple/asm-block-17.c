/* APPLE LOCAL file CW asm blocks */
/* { dg-do assemble { target i?86*-*-darwin* } } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */
/* { dg-options { -fasm-blocks -msse3 -O2 } } */
/* Radar  */

asm inline void foo() {
  a: nop
  b: nop
  jmp a
}

void bar() {
  foo();
  foo();
}
