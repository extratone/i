/* APPLE LOCAL file CW asm blocks */
/* { dg-do assemble { target i?86*-*-darwin* } } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */
/* { dg-options { -fasm-blocks -msse3 } } */
/* Radar 4429851 */

void foo() {
  asm {
    push ebx
    mov ebx, offset label3
    nop
    label3:
    pop ebx
  }
}
