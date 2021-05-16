/* APPLE LOCAL file 4216500 */
/* Check that typedefs of ObjC classes preserve 
   any @protocol qualifiers.  */
/* { dg-do compile } */
/* APPLE LOCAL radar 4894756 */
#include "../objc/execute/Object2.h"

@protocol CanDoStuff;

typedef Object<CanDoStuff> CanDoStuffType;
typedef Object<CanDoStuff> *CanDoStuffTypePtr;

@protocol CanDoStuff
- (int) dostuff;
@end

@protocol MoreStuff
- (int) morestuff;
@end

int main(void)
{
    CanDoStuffTypePtr  dice     = nil;
    CanDoStuffType    *nodice   = nil;
    int count;
    count = [dice dostuff];
    count = [nodice dostuff];
    return 0;
}

