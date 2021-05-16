/* APPLE LOCAL file 4209854 */
/* Check if array and function parameters get decayed to pointers as
   they should.  */
/* { dg-do run } */
/* { dg-options "-O2" } */

/* APPLE LOCAL radar 4894756 */
#include "../objc/execute/Object2.h"
#include <string.h>
#include <stdlib.h>

static char global_buf[20];

char *strcpy_like_callee(const char *s) {
  strcpy(global_buf, s);
  return global_buf;
}  

typedef char io_string_t[512];
typedef char *(func_type)(const char *);

@interface DeviceObject: Object
- (void) func:(func_type)func stucPathInIORegistry:(io_string_t)ioRegPath;
@end
@implementation DeviceObject
- (void) func:(func_type)func stucPathInIORegistry:(io_string_t)ioRegPath
{
    func(ioRegPath);
}
@end

int main (void) {
  io_string_t my_string;
  DeviceObject *obj = [DeviceObject new];

  strcpy (my_string, "Hello!");
  strcpy (global_buf, "Good-bye!");

  [obj func:strcpy_like_callee stucPathInIORegistry:my_string];

  if (strcmp (global_buf, "Hello!"))
    abort ();

  return 0;
}
