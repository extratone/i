/* Contributed by Nicola Pero - Fri Mar  9 19:39:15 CET 2001 */
#include <objc/objc.h>
#include <objc/objc-api.h>

#include "next_mapping.h"

/* Test getting and calling the IMP of a method */

@interface TestClass
{
  Class isa;
}
- (int) next: (int)a;
@end

@implementation TestClass
- (int) next: (int)a
{
  return a + 1;
}
@end


int main (void)
{
  Class class;
  SEL selector;
  int (* imp) (id, SEL, int);
  
  class = objc_get_class ("TestClass");
  selector = @selector (next:);
  imp = (int (*)(id, SEL, int))method_get_imp 
    (class_get_class_method (class, selector));
  
  if (imp (class, selector, 5) != 6)
    {
      abort ();
    }

  return 0;
}
