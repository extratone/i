/* Test ICE on defining function with a name previously declared as a
   nonfunction.  Bug 28299 from Bernhard Fischer
   <aldot@gcc.gnu.org>.  */
/* { dg-do compile } */
/* { dg-options "-Wmissing-prototypes" } */

extern __typeof(foo) foo __asm__(""); /* { dg-error "undeclared" } */
/* { dg-error "previous declaration" "previous declaration" { target *-*-* } 7 } */
void *foo (void) {} /* { dg-error "redeclared as different kind of symbol" } */
/* { dg-warning "no previous prototype" "no previous prototype" { target *-*-* } 9 } */
