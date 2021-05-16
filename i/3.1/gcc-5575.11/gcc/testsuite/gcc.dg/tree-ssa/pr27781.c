/* { dg-do compile } */
/* { dg-require-weak "" } */
/* { dg-options "-O2 -fdump-tree-optimized" } */

void __attribute__((weak)) func(void)
{
    /* no code */
}

int main()
{
    func();
    return 0;
}

/* { dg-final { scan-tree-dump "func \\(\\);" "optimized" } } */
/* { dg-final { cleanup-tree-dump "optimized" } } */
