/* Test the -fconstant-string-class flag error.  */
/* { dg-do compile } */
/* { dg-options "-fconstant-string-class=" } */

{ dg-error "no class name specified|missing argument" "" { target *-*-* } 0 }

void foo () {}
