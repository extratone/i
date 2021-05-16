/* Test for C99 forms of array declarator.  */
/* Origin: Joseph Myers <jsm28@cam.ac.uk> */
/* { dg-do compile } */
/* { dg-options "-std=iso9899:1999 -pedantic-errors" } */

/* Because GCC doesn't yet implement it, we don't yet test for [*] here.  */

/* Test each of: [quals], [quals expr], [static expr], [static quals expr],
   [quals static expr].  Not yet: [quals *].  */

void f00 (int a[const]);
void f01 (int [const]); /* { dg-bogus "warning" "warning in place of error" } */
/* { dg-error "abstract" "\[quals\] in abstract declarator" { target *-*-* } 12 } */
void
f02 (int a[const])
{
  int **b = &a; /* { dg-bogus "warning" "warning in place of error" } */
  /* { dg-error "discards" "discards quals" { target *-*-* } 17 } */
  int *const *c = &a;
}
void
f03 (a)
     int a[const];
{
  int **b = &a; /* { dg-bogus "warning" "warning in place of error" } */
  /* { dg-error "discards" "discards quals" { target *-*-* } 25 } */
  int *const *c = &a;
}

void f10 (int a[const 2]);
void f11 (int [const 2]); /* { dg-bogus "warning" "warning in place of error" } */
/* { dg-error "abstract" "\[quals expr\] in abstract declarator" { target *-*-* } 31 } */
void
f12 (int a[const 2])
{
  int **b = &a; /* { dg-bogus "warning" "warning in place of error" } */
  /* { dg-error "discards" "discards quals" { target *-*-* } 36 } */
  int *const *c = &a;
}
void
f13 (a)
     int a[const 2];
{
  int **b = &a; /* { dg-bogus "warning" "warning in place of error" } */
  /* { dg-error "discards" "discards quals" { target *-*-* } 44 } */
  int *const *c = &a;
}

void f20 (int a[static 2]);
void f21 (int [static 2]); /* { dg-bogus "warning" "warning in place of error" } */
/* { dg-error "abstract" "\[static expr\] in abstract declarator" { target *-*-* } 50 } */
void
f22 (int a[static 2])
{
  int **b = &a;
  int *const *c = &a;
}
void
f23 (a)
     int a[static 2];
{
  int **b = &a;
  int *const *c = &a;
}

void f30 (int a[static const 2]);
void f31 (int [static const 2]); /* { dg-bogus "warning" "warning in place of error" } */
/* { dg-error "abstract" "\[static quals expr\] in abstract declarator" { target *-*-* } 67 } */
void
f32 (int a[static const 2])
{
  int **b = &a; /* { dg-bogus "warning" "warning in place of error" } */
  /* { dg-error "discards" "discards quals" { target *-*-* } 72 } */
  int *const *c = &a;
}
void
f33 (a)
     int a[static const 2];
{
  int **b = &a; /* { dg-bogus "warning" "warning in place of error" } */
  /* { dg-error "discards" "discards quals" { target *-*-* } 80 } */
  int *const *c = &a;
}

void f40 (int a[const static 2]);
void f41 (int [const static 2]); /* { dg-bogus "warning" "warning in place of error" } */
/* { dg-error "abstract" "\[quals static expr\] in abstract declarator" { target *-*-* } 86 } */
void
f42 (int a[const static 2])
{
  int **b = &a; /* { dg-bogus "warning" "warning in place of error" } */
  /* { dg-error "discards" "discards quals" { target *-*-* } 91 } */
  int *const *c = &a;
}
void
f43 (a)
     int a[const static 2];
{
  int **b = &a; /* { dg-bogus "warning" "warning in place of error" } */
  /* { dg-error "discards" "discards quals" { target *-*-* } 99 } */
  int *const *c = &a;
}

/* Test rejection of static and type qualifiers in non-parameter contexts.  */
int x[const 2]; /* { dg-bogus "warning" "warning in place of error" } */
/* { dg-error "non-parameter" "quals in non-parm array" { target *-*-* } 105 } */
int y[static 2]; /* { dg-bogus "warning" "warning in place of error" } */
/* { dg-error "non-parameter" "static in non-parm array" { target *-*-* } 107 } */
void g (int a[static 2][3]);
void h (int a[2][static 3]); /* { dg-bogus "warning" "warning in place of error" } */
/* { dg-error "non-parameter" "static in non-final parm array" { target *-*-* } 110 } */
