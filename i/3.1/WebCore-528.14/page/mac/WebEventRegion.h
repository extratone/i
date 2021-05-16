/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 * No license or rights are granted by Apple expressly or by implication,
 * estoppel, or otherwise, to Apple copyrights, patents, trademarks, trade
 * secrets or other rights.
 */

#include <wtf/Platform.h>

#if ENABLE(TOUCH_EVENTS)

@interface WebEventRegion : NSObject <NSCopying>
{
    CGPoint p1, p2, p3, p4;
}
- (id)initWithPoints:(CGPoint) inP1:(CGPoint) inP2:(CGPoint) inP3:(CGPoint) inP4;
- (BOOL)hitTest:(CGPoint)point;
@end

#endif // ENABLE(TOUCH_EVENTS)
