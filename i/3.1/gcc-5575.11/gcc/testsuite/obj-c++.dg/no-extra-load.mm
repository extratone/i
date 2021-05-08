// Radar 3926484

// { dg-do compile }

/* APPLE LOCAL radar 4894756 */
#include "../objc/execute/Object2.h"
#include <iostream>

@interface Greeter : Object
- (void) greet: (const char *)msg;
@end

@implementation Greeter
- (void) greet: (const char *)msg { std::cout << msg; }
@end

int
main ()
{
  std::cout << "Hello from C++\n";
  Greeter *obj = [Greeter new];
  [obj greet: "Hello from Objective-C\n"];
}

/* { dg-final { scan-assembler-not "L_objc_msgSend\\\$non_lazy_ptr" } } */
