/* { dg-do compile } */
/* { dg-options "-O2 -funsafe-math-optimizations -fdump-tree-recip -fdump-tree-lim" } */

double F[2] = { 0., 0. }, e = 0.;

int main()
{
	int i;
	double E, W, P, d;

        /* make sure the program crashes on FP exception */
        unsigned short int Mask;

	W = 1.;
	d = 2.*e;
	E = 1. - d;

	for( i=0; i < 2; i++ )
		if( d > 0.01 )
		{
			P = ( W < E ) ? (W - E)/d : (E - W)/d;
			F[i] += P;
		}

	return 0;
}

/* LIM only performs the transformation in the no-trapping-math case.  In
   the future we will do it for trapping-math as well in recip, check that
   this is not wrongly optimized.  */
/* { dg-final { scan-tree-dump-not "reciptmp" "lim" } } */
/* { dg-final { scan-tree-dump-not "reciptmp" "recip" } } */
/* { dg-final { cleanup-tree-dump "recip" } } */
/* { dg-final { cleanup-tree-dump "lim" } } */

