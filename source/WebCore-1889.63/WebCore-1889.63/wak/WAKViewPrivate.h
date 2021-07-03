//
//  WAKViewPrivate.h
//
//  Copyright (C) 2005, 2006, 2007, 2009 Apple Inc.  All rights reserved.
//

#ifndef WAKViewPrivate_h
#define WAKViewPrivate_h

#import "WAKView.h"

@interface WAKView (WAKPrivate)
- (WKViewRef)_viewRef;
+ (WAKView *)_wrapperForViewRef:(WKViewRef)_viewRef;
- (id)_initWithViewRef:(WKViewRef)view;
- (BOOL)_handleResponderCall:(WKViewResponderCallbackType)type;
- (NSMutableSet *)_subviewReferences;
- (BOOL)_selfHandleEvent:(WebEvent *)event;
@end

static inline WAKView *WAKViewForWKViewRef(WKViewRef view)
{
    if (!view)
        return nil;
    WAKView *wrapper = (WAKView *)view->wrapper;
    if (wrapper)
        return wrapper;
    return [WAKView _wrapperForViewRef:view];
}

#endif // WAKViewPrivate_h
