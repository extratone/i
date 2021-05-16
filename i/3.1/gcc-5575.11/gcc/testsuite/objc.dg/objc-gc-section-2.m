/* APPLE LOCAL file radar 4810609 */
/* Check that -fobjc-gc-only option produces a 110 flag in objc section. */
/* { dg-options "-fobjc-gc-only" } */
/* { dg-do compile { target powerpc*-*-darwin* i?86*-*-darwin* } } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */
/* { dg-require-effective-target objc_gc } */

@interface INTF
@end
@implementation  INTF
@end
/* { dg-final { scan-assembler ".section __OBJC, __image_info" } } */
/* { dg-final { scan-assembler "L_OBJC_IMAGE_INFO:\n\t.long\t0\n\t.long\t6" } } */
