/* APPLE LOCAL file mainline 4.3 2006-10-31 4134307 */
/* Check that you can't redefine a C99 inline function.  */
/* { dg-do compile } */
/* { dg-options "-std=c99" } */

extern inline int func1 (void)
{ /* { dg-error "previous definition" } */
  return 1;
}

inline int func1 (void)
{ /* { dg-error "redefinition" } */
  return 1;
}

inline int func2 (void)
{ /* { dg-error "previous definition" } */
  return 2;
}

inline int func2 (void)
{ /* { dg-error "redefinition" } */
  return 2;
}
