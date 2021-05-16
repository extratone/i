/* Test declaration specifiers.  Test various checks on storage class
   and function specifiers that depend on information about the
   declaration, not just the specifiers.  Test with no special
   options.  */
/* Origin: Joseph Myers <jsm@polyomino.org.uk> */
/* { dg-do compile } */
/* { dg-options "" } */

auto void f0 (void) {} /* { dg-warning "warning: function definition declared 'auto'" } */
register void f1 (void) {} /* { dg-error "error: function definition declared 'register'" } */
typedef void f2 (void) {} /* { dg-error "error: function definition declared 'typedef'" } */

void f3 (auto int); /* { dg-error "error: storage class specified for parameter 'type name'" } */
void f4 (extern int); /* { dg-error "error: storage class specified for parameter 'type name'" } */
void f5 (register int);
void f6 (static int); /* { dg-error "error: storage class specified for parameter 'type name'" } */
void f7 (typedef int); /* { dg-error "error: storage class specified for parameter 'type name'" } */

auto int x; /* { dg-error "error: file-scope declaration of 'x' specifies 'auto'" } */
register int y;

void h (void) { extern void x (void) {} } /* { dg-error "error: nested function 'x' declared 'extern'" } */

void
g (void)
{
  void a; /* { dg-error "error: variable or field 'a' declared void" } */
  const void b; /* { dg-error "error: variable or field 'b' declared void" } */
  static void c; /* { dg-error "error: variable or field 'c' declared void" } */
}

void p;
const void p1;
extern void q;
extern const void q1;
static void r; /* { dg-error "error: variable or field 'r' declared void" } */
static const void r1; /* { dg-error "error: variable or field 'r1' declared void" } */

register void f8 (void); /* { dg-error "error: invalid storage class for function 'f8'" } */

void i (void) { auto void y (void) {} }

inline int main (void) { return 0; } /* { dg-warning "warning: cannot inline function 'main'" } */
