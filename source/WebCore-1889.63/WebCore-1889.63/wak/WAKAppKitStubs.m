//
//  WAKAppKitStubs.m
//
//  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
//

#import "config.h"
#import "WAKAppKitStubs.h"

id NSApp = nil;

void *WKAutorelease(id object)
{
    return [object autorelease];
}

BOOL WKMouseInRect(CGPoint aPoint, CGRect aRect) 
{
    return aPoint.x >= aRect.origin.x &&
	   aPoint.x < (aRect.origin.x + aRect.size.width) &&
	   aPoint.y >= aRect.origin.y && aPoint.y < (aRect.origin.y + aRect.size.height);
}

@implementation NSCursor
+ (void)setHiddenUntilMouseMoves:(BOOL)flag
{
    UNUSED_PARAM(flag);
}
@end

void WKBeep(void) { }
