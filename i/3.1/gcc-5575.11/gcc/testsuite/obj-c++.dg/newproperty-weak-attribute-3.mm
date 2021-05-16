/* APPLE LOCAL file radar 4805321 */
/* Test that we call objc_assign_weak and objc_read_weak */
/* { dg-do compile { target powerpc*-*-darwin* i?86*-*-darwin* } } */
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-new-property -fobjc-gc" } */
/* { dg-require-effective-target objc_gc } */

@interface INTF
{
  __weak id IVAR;
}
@property (assign) __weak id uses_inclass_weak;
@property  (assign) __weak id uses_default_weak;
@end

@implementation INTF
@synthesize uses_inclass_weak = IVAR, uses_default_weak = IVAR;
@end
/* { dg-final { scan-assembler "objc_assign_weak" } } */
/* { dg-final { scan-assembler "objc_read_weak" } } */
