/* APPLE LOCAL file CW asm blocks */
/* { dg-do assemble { target i?86*-*-darwin* } } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */
/* { dg-options { "-fasm-blocks" } } */
/* Radar 4214021 */

void foo() {
  asm bar: jmp bar
  __asm nop __asm nop
  __asm nop
  __asm emms
    asm @1: @2: @3 foo1: nop asm @4: @5: foo2: nop
    asm .word 0
}
