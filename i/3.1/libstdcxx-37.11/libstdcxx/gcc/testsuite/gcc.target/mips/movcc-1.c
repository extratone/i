/* { dg-do compile } */
/* { dg-mips-options "-O2 -mips4" } */
/* { dg-final { scan-assembler "movz" } } */
/* { dg-final { scan-assembler "movn" } } */

void ext_int (int);

int
sub1 (int i, int j, int k)
{
  ext_int (k ? i : j);
}

int
sub2 (int i, int j, long l)
{
  ext_int (!l ? i : j);
}
