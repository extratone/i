/* { dg-do compile } */
/* { dg-options "-Waddress -Werror -Wno-error=address -Wattributes" } */
/* { dg-warning "warnings being treated as errors" "" {target "*-*-*"} 0 } */

/* Make sure -Wno-error= overrides -Werror.  */

void __attribute__((dj)) bar() { }	/* { dg-warning "warning: .* attribute directive ignored" } */

int i;

void
foo ()
{
  if (&i)	/* { dg-warning "warning: .* will always evaluate as 'true'" } */
    grill ();
}
