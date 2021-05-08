/* Test for ICE with redeclaration in inner scope which is accepted
   despite incompatible type.  */
/* Origin: Joseph Myers <joseph@codesourcery.com> */
/* { dg-do compile } */
/* { dg-options "" } */

void
f (void)
{
  g(); /* { dg-warning "warning: previous implicit declaration of 'g' was here" } */
  {
    void g(); /* { dg-warning "warning: conflicting types for 'g'" } */
  }
}
