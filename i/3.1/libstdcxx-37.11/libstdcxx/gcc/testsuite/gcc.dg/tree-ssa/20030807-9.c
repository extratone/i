/* { dg-do compile } */
/* { dg-options "-O1 -fdump-tree-dom3" } */

static void
bar ()
{
  const char *label2 = (*"*.L_sfnames_b" == '*') + "*.L_sfnames_b";
  oof (label2);
}

void
ooof ()
{
  if (""[0] == 0)
    foo();
}

/* There should be no IF conditionals.  */
/* { dg-final { scan-tree-dump-times "if " 0 "dom3"} } */
/* { dg-final { cleanup-tree-dump "dom3" } } */
