/* { dg-do compile } */ 
/* { dg-options "-O2 -fdump-tree-pre-stats" } */
/* We can't eliminate the *p load here in any sane way, as eshup8 may 
   change it.  */
void
enormlz (x)
     unsigned short x[];
{
  register unsigned short *p;
  p = &x[2];
  while ((*p & 0xff00) == 0)
    {
      eshup8 (x);
    }
}
/* { dg-final { scan-tree-dump-times "Eliminated: 0" 1 "pre"} } */
/* { dg-final { cleanup-tree-dump "pre" } } */
