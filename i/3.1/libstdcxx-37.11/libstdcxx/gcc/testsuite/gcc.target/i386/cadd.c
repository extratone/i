/* { dg-do compile { target i?86-*-* x86_64-*-* } } */
/* { dg-options "-O2 -march=k8" } */
/* { dg-final { scan-assembler "sbb" } } */

extern void abort (void);

/* Conditional increment is best done using sbb $-1, val.  */
int t[]={0,0,0,0,1,1,1,1,1,1};
q()
{
  int sum=0;
  int i;
  for (i=0;i<10;i++)
    if (t[i])
       sum++;
  if (sum != 6)
    abort ();
}
main()
{
  int i;
  for (i=0;i<10000000;i++)
    q();
}
