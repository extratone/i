/* Contributed by Nicola Pero - Fri Dec 14 08:36:00 GMT 2001 */
#include <objc/objc.h>
#include <objc/Object.h>

#ifdef __NEXT_RUNTIME__
#define objc_get_class(C)			objc_getClass(C)
#define class_create_instance(C)		class_createInstance(C, 0)
#endif

/* Test loading unclaimed categories - categories of a class defined
   separately from the class itself.  */

@interface TestClass
{
  id isa;
}
- (int)D;
@end

@interface TestClass (A)
- (int)A;
@end

@interface TestClass (B)
- (int)B;
@end

@interface TestClass (C)
- (int)C;
@end


