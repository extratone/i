/* Test for redefining macros with significant differences.  */

/* { dg-do preprocess }
   { dg-options "-ansi -Wall -fno-show-column" } */

#define mac(a, b) (a) + (b)
#define mac(a, b) (a) * (b)
#define mac(a, b) (a) * (x)
#define mac(a, g) (a) * (x)

#define ro(x) foo x bar
#define ro(x, b) foo x bar

#define va(a...) a
#define va(...) __VA_ARGS__

#define foo(x) x
#define foo(x)x		/* { dg-bogus "redefined" "redefined foo" } */

/* { dg-warning "redefined" "redef mac"     { target *-*-* } 7  }
   { dg-warning "redefined" "redef mac"     { target *-*-* } 8  }
   { dg-warning "redefined" "redef mac"     { target *-*-* } 9  }
   { dg-warning "redefined" "redef ro"      { target *-*-* } 12 }
   { dg-warning "redefined" "redef va"      { target *-*-* } 15 }

   { dg-warning "previous"  "prev def mac"  { target *-*-* } 6  }
   { dg-warning "previous"  "prev def mac"  { target *-*-* } 7  }
   { dg-warning "previous"  "prev def mac"  { target *-*-* } 8  }
   { dg-warning "previous"  "prev def ro"   { target *-*-* } 11 }
   { dg-warning "previous"  "prev def va"   { target *-*-* } 14 }
*/
