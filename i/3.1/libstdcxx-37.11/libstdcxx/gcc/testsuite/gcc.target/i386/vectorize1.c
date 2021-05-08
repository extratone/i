/* PR middle-end/28915 */
/* { dg-options "-msse -O2 -ftree-vectorize -fdump-tree-vect" } */

extern char lanip[3][40];
typedef struct
{
  char *t[4];
}tx_typ;

int set_names (void)
{
  static tx_typ tt1;
  int ln;
  for (ln = 0; ln < 4; ln++)
      tt1.t[ln] = lanip[1];
}

/* { dg-final { scan-tree-dump "vect_cst" "vect" } } */
