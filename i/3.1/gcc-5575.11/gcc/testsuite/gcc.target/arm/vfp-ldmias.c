/* { dg-do compile } */
/* { dg-require-effective-target arm_vfp_ok } */
/* { dg-options "-O2 -mfpu=vfp -mfloat-abi=softfp" } */

extern void baz (float);

void
foo (float *p, float a, int n)
{
  do
    bar (*p++ + a);
  while (n--);
}

/* { dg-final { scan-assembler "fldmias" } } */
