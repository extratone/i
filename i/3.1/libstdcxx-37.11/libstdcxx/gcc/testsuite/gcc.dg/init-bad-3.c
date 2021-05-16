/* Test diagnostics for various bad initializers.  Test cases with
   standard syntax with -pedantic-errors.  */
/* Origin: Joseph Myers <joseph@codesourcery.com> */
/* { dg-do compile } */
/* { dg-options "-std=gnu99 -pedantic-errors" } */

void f(void);
void g(void) = f; /* { dg-error "error: function 'g' is initialized like a variable" } */

void h(a)
     int a = 1; /* { dg-error "error: parameter 'a' is initialized" } */
{
  struct s x = { 0 }; /* { dg-error "error: variable 'x' has initializer but incomplete type" } */
  /* { dg-error "excess elements|near init" "near" { target *-*-* } 13 } */
  /* { dg-error "storage size" "size" { target *-*-* } 13 } */
}

char s[1] = "x";
char s1[1] = { "x" };
char t[1] = "xy"; /* { dg-error "error: initializer-string for array of chars is too long" } */
char t1[1] = { "xy" }; /* { dg-error "error: initializer-string for array of chars is too long" } */
char u[1] = { "x", "x" }; /* { dg-error "error: excess elements in char array initializer" } */
/* { dg-error "near init" "near" { target *-*-* } 22 } */

int j = { 1 };

int k = { 1, 2 }; /* { dg-error "error: excess elements in scalar initializer" } */
/* { dg-error "near init" "near" { target *-*-* } 27 } */

int a1[1] = { [1] = 0 }; /* { dg-error "error: array index in initializer exceeds array bounds" } */
/* { dg-error "near init" "near" { target *-*-* } 30 } */
int a2[1] = { [-1] = 0 }; /* { dg-error "error: array index in initializer exceeds array bounds" } */
/* { dg-error "near init" "near" { target *-*-* } 32 } */
