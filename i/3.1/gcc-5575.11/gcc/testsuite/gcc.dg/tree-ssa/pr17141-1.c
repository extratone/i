/* { dg-do compile } */
/* { dg-options "-O2 -fdump-tree-forwprop2" } */


struct A { int i; };
int
foo(struct A *locp, int str)
{
  int T355, *T356;
  T356 = &locp->i;
  *T356 = str;
  return locp->i;
}

/* { dg-final { scan-tree-dump-times "&" 0 "forwprop2" } } */
/* { dg-final { cleanup-tree-dump "forwprop2" } } */


