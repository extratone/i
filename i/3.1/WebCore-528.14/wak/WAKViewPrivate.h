//
//  WAKViewPrivate.h
//
//  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
//
#import "WAKView.h"

@interface WAKView (WAKPrivate)
- (WKViewRef)_viewRef;
+ (void)_addViewWrapper:(WAKView *)view;
+ (void)_removeViewWrapper:(WAKView *)view;
+ (WAKView *)_wrapperForViewRef:(WKViewRef)_viewRef;
- (void)_handleEvent:(GSEventRef)event;
- (BOOL)_handleResponderCall:(WKViewResponderCallbackType)type;
- (NSMutableSet *)_subviewReferences;
@end
