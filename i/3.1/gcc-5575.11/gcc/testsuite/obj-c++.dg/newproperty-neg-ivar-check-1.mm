/* APPLE LOCAL file radar 4816280 */
/* Diagnose as needed when 'ivar' synthesis is needed and it is not allowed. 
   'fragile' ivar (32bit abi) only. */
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-new-property -fobjc-abi-version=1" { target powerpc*-*-darwin* i?86*-*-darwin* } } */
/* { dg-options "-fobjc-new-property -fobjc-abi-version=1" { target arm*-*-darwin* } } */
/* { dg-do compile } */

@interface Moe
@property int ivar;
@end

@implementation Moe
@synthesize ivar; /* { dg-error "synthesized property 'ivar' must either be named the same as a compatible ivar or must explicitly name an ivar" } */
- (void)setIvar:(int)arg{}
@end

@interface Fred
@property int ivar;
@end

@implementation Fred
// no warning
@synthesize ivar;
- (void)setIvar:(int)arg{}
- (int)ivar{return 1;}
@end

@interface Bob
@property int ivar;
@end

@implementation Bob
// no warning
@dynamic ivar;
- (int)ivar{return 1;}
@end

@interface Jade
@property int ivar;
@end

@implementation Jade
// no warning
- (void)setIvar:(int)arg{}
- (int)ivar{return 1;}
@end

int main (void) {return 0;}

