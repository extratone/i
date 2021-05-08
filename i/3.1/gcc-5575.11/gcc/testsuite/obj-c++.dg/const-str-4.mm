/* APPLE LOCAL file mainline */
/* Ensure that the preprocessor handles ObjC string constants gracefully. */
/* Author: Ziemowit Laski <zlaski@apple.com> */
/* APPLE LOCAL radar 4621575 */
/* { dg-options "-fnext-runtime -fno-constant-cfstrings -fconstant-string-class=MyString -lobjc" } */ 
/* { dg-do run { target *-*-darwin* } } */

extern "C" void abort(void);

@interface MyString
{
  void *isa;
  char *str;
  int len;
}
@end

#define kMyStringMacro1 "My String"
#define kMyStringMacro2 @"My String"

void *_MyStringClassReference;

@implementation MyString
@end

int main(void) {
  MyString* aString1 = @kMyStringMacro1;
  MyString* aString2 = kMyStringMacro2;
  if(aString1 != aString2) {
    abort();
  }
  return 0;
}
