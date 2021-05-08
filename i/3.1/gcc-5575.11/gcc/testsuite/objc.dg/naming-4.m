/* APPLE LOCAL begin radar 4291785 */
/* Testing for detecting duplicate ivars. */
/* { dg-do compile } */

typedef struct S { int i; } NSDictionary;

@interface A 
{
    NSDictionary * _userInfo;
}
@end

@interface B : A
{
    NSDictionary * _userInfo;	/* { dg-error "duplicate member" } */
    NSDictionary * _userInfo;	/* { dg-error "duplicate member" } */
}
@end

@interface C : A
@end

@interface D : C
{
    NSDictionary * _userInfo;   /* { dg-error "duplicate member" } */
    NSDictionary * _userInfo;   /* { dg-error "duplicate member" } */
}
@end
/* APPLE LOCAL end radar 4291785 */
