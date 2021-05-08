/* { dg-do compile } */
/* { dg-options "-fobjc-exceptions" } */

/* APPLE LOCAL radar 4894756 */
#include "../objc/execute/Object2.h"

int main (int argc, const char * argv[]) {
  Object * pool = [Object new];
  int a;

  if ( 1 ) {
    
    @try {
      a = 1;
    }
    @catch (Object *e) {
      a = 2;
    }
    @finally {
      a = 3;
    }
  }
    
  [pool free];
  return 0;
}
