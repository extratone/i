/* APPLE LOCAL file radar 4441551 */
/* Warn @defs() in Objective-C++ */
/* { dg-options "-lobjc -Wobjc2" } */
/* { dg-do run } */
/* APPLE LOCAL radar 4894756 */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */
/* APPLE LOCAL ARM objc2 */
/* { dg-skip-if "" { arm*-*-darwin* } { "*" } { "" } } */


#include <stdlib.h>
#include <objc/objc.h>
#include <objc/Object.h>

extern void abort(void);

@interface A : Object
{
  @public
    int a;
}
@end

struct A_defs 
{
  @defs(A);	/* { dg-warning  "@defs will not be supported in future" } */
};

@implementation A
- init 
{
  a = 42;
  return self;
}
@end


int main() 
{
  A *a = [A init];
  struct A_defs *a_defs = (struct A_defs *)a;
  
  if (a->a != a_defs->a)
    abort ();	
  
  return 0;
}
