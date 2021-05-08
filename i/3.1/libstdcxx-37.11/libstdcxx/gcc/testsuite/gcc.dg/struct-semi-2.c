/* Test diagnostics for missing and extra semicolons in structures.
   Test with -pedantic.  */
/* Origin: Joseph Myers <joseph@codesourcery.com> */
/* { dg-do compile } */
/* { dg-options "-pedantic" } */

struct s0 { ; }; /* { dg-warning "warning: extra semicolon in struct or union specified" } */
/* { dg-warning "warning: struct has no members" "empty" { target *-*-* } 7 } */
struct s1 {
  int a;
  ; /* { dg-warning "warning: extra semicolon in struct or union specified" } */
  int b;
};
struct s2 {
  ; /* { dg-warning "warning: extra semicolon in struct or union specified" } */
  int c
}; /* { dg-warning "warning: no semicolon at end of struct or union" } */
struct s3 {
  int d
}; /* { dg-warning "warning: no semicolon at end of struct or union" } */
