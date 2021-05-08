/* { dg-options "-O2 -fdump-tree-optimized -fdump-tree-tree_profile" } */
unsigned int a[1000];
unsigned int b = 257;
unsigned int c = 1023;
unsigned int d = 19;
main ()
{
  int i;
  unsigned int n;
  for (i = 0; i < 1000; i++)
    {
	    a[i]=18;
    }
  for (i = 0; i < 1000; i++)
    {
      if (i % 2)
	n = b;
      else if (i % 3)
	n = c;
      else
	n = d;
      a[i] %= n;
    }
  return 0;
}
/* { dg-final-use { scan-tree-dump "Mod subtract transformation on insn" "tree_profile"} } */
/* This is part of code checking that n is greater than the divisor so we are sure that it
   didn't get optimized out.  */
/* { dg-final-use { scan-tree-dump "if \\(n \\>" "optimized"} } */
/* { dg-final-use { scan-tree-dump-not "Invalid sum" "optimized"} } */
/* { dg-final-use { cleanup-tree-dump "optimized" } } */
/* { dg-final-use { cleanup-tree-dump "tree_profile" } } */
