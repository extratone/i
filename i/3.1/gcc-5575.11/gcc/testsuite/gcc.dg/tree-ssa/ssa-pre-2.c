/* { dg-do compile } */ 
/* { dg-options "-O2 -fdump-tree-pre-stats" } */
int motion_test1(int data, int data_0, int data_3, int v)
{
	int i;
	int t, u;

	if (data)
		i = data_0 + data_3;
	else {
		v = 2;
		i = 5;
	}
	t = data_0 + data_3;
	u = i;
	return v * t * u;
}
/* We should eliminate one computation of data_0 + data_3 along the 
   main path, and one computation of v * i along the main path, causing 
   two eliminations. */
/* { dg-final { scan-tree-dump-times "Eliminated: 2" 1 "pre"} } */
/* { dg-final { cleanup-tree-dump "pre" } } */
