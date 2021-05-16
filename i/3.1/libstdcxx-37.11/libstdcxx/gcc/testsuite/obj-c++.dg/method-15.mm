/* Check if finding multiple signatures for a method is handled gracefully when method lookup succeeds (see also method-7.m).  */
/* Contributed by Ziemowit Laski <zlaski@apple.com>  */
/* { dg-options "-Wstrict-selector-match" } */
/* { dg-do compile } */

#include <objc/Object.h>

@protocol MyObject
- (id)initWithData:(Object *)data;
@end

@protocol SomeOther
- (id)initWithData:(int)data;
@end

@protocol MyCoding
- (id)initWithData:(id<MyObject, MyCoding>)data;
@end

@interface NTGridDataObject: Object <MyCoding>
{
    Object<MyCoding> *_data;
}
+ (NTGridDataObject*)dataObject:(id<MyObject, MyCoding>)data;
@end

@implementation NTGridDataObject
- (id)initWithData:(id<MyObject, MyCoding>)data {
  return data;
}
+ (NTGridDataObject*)dataObject:(id<MyObject, MyCoding>)data
{
    NTGridDataObject *result = [[NTGridDataObject alloc] initWithData:data];
     /* { dg-warning "multiple methods named .\\-initWithData:. found" "" { target *-*-* } 33 } */
     /* { dg-warning "using .\\-\\(id\\)initWithData:\\(Object \\*\\)data." "" { target *-*-* } 9 } */
     /* { dg-warning "also found .\\-\\(id\\)initWithData:\\(id <MyObject, MyCoding>\\)data." "" { target *-*-* } 17 } */
     /* { dg-warning "also found .\\-\\(id\\)initWithData:\\(int\\)data." "" { target *-*-* } 13 } */

     /* The following warning is a consequence of picking the "wrong" method signature.  */
     /* { dg-warning "passing argument 1 of .initWithData:. from distinct Objective\\-C type" "" { target *-*-* } 33 } */
    return result;
}
@end
