/* { dg-do compile } */ 
/* { dg-options "-O2 -fno-tree-dominator-opts -fdump-tree-fre-stats" } */
int
foo (int *array)
{
    if (array[1] != 0)
          return array[1];
      return 0;
}
/* We should eliminate one address calculation, and one load.  */
/* { dg-final { scan-tree-dump-times "Eliminated: 2" 1 "fre"} } */
/* { dg-final { cleanup-tree-dump "fre" } } */
