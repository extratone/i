#include <objc/objc.h>
#include <objc/objc-api.h>
#include <objc/Object.h>

@interface MyObject
{
  Class isa;
  unsigned int i;
  MyObject *object;
}
@end

@implementation MyObject
@end

#include "bf-common.h"

