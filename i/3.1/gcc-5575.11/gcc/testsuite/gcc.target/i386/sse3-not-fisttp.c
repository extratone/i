/* APPLE LOCAL file mainline */
/* Test that we don't generate a fisttp instruction when -mno-sse3.  */
/* { dg-do compile { target i?86-*-* x86_64-*-* } } */
/* { dg-options "-O -march=nocona -mno-sse3" } */
/* { dg-final { scan-assembler-not "fisttp" } } */
struct foo
{
 long a;
 long b;
};

extern double c;

extern unsigned long long baz (void);

int
walrus (const struct foo *input)
{
    unsigned long long d;

    d = baz ()
      + (unsigned long long) (((double) input->a * 1000000000
			      + (double) input->b) * c);
    return (d ? 1 : 0);
}
