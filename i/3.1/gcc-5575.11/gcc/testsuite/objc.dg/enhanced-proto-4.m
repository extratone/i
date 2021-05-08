/* APPLE LOCAL file radar 4653422 */
/* Test implementation of @optional property */
/* { dg-options "-mmacosx-version-min=10.5 -framework Foundation" { target powerpc*-*-darwin* i?86*-*-darwin* } } */
/* { dg-options "-framework Foundation" { target arm*-*-darwin* } } */
/* { dg-do run { target *-*-darwin* } } */
#import <Foundation/Foundation.h>
/* { dg-do run } */

@protocol Test
  @property int required;

@optional
  @property int optional;
  @property int optional1;
  @property int optional_preexisting_setter_getter;
  @property (setter = setOptional_preexisting_setter_getter: ,
	     getter = optional_preexisting_setter_getter) int optional_with_setter_getter_attr;
@required
  @property int required1;
@optional
  @property int optional_to_be_defined;
  @property (readonly, getter = optional_preexisting_setter_getter) int optional_getter_attr;
@end

@interface Test : NSObject <Test> {
  int ivar;
}
@property int required;
@property int optional_to_be_defined;
- (int) optional_preexisting_setter_getter;
- (void) setOptional_preexisting_setter_getter:(int)value;
@end

@implementation Test
@synthesize required = ivar;
@synthesize required1 = ivar;
@synthesize optional_to_be_defined = ivar;
- (int) optional_preexisting_setter_getter { return ivar; }
- (void) setOptional_preexisting_setter_getter:(int)value
	   {
		ivar = value;
	   }
- (void) setOptional_getter_attr:(int)value { ivar = value; }
@end

int main ()
{
	Test *x = [[Test alloc] init];
	/* 1. Test of a requred property */
	x.required = 100;
  	if (x.required1 != 100)
	  abort ();

	/* 2. Test of a synthesize optional property */
  	x.optional_to_be_defined = 123;
	if (x.required1 != 123)
	  abort ();

	/* 3. Test of optional property with pre-sxisting defined setter/getter */
	x.optional_preexisting_setter_getter = 200;
	if (x.optional_preexisting_setter_getter != 200)
	  abort ();

	/* 4. Test of optional property with setter/getter attribute */
	if (x.optional_with_setter_getter_attr != 200)
	  abort ();
	return 0;

	/* 5. Test of optional property with getter attribute and default setter method. */
	x.optional_getter_attr = 1000;
        if (x.optional_getter_attr != 1000)
	  abort ();

	return 0;
}

