//
//  WKViewPrivate.h
//
//  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
//

#import "WKView.h"

#ifdef __cplusplus
extern "C" {
#endif    

void _WKViewSetWindow (WKViewRef view, WKWindowRef window);
void _WKViewSetSuperview (WKViewRef view, WKViewRef superview);
void _WKViewWillRemoveSubview(WKViewRef view, WKViewRef subview);
WKViewRef _WKViewBaseView (WKViewRef view);
WKViewRef _WKViewHitTest(WKViewRef view, CGPoint point);
bool _WKViewHandleEvent (WKViewRef view, GSEventRef event);
void _WKViewAutoresize(WKViewRef view, const CGRect *oldSuperFrame, const CGRect *newSuperFrame);
WKScrollViewRef _WKViewParentScrollView(WKViewRef view);
void _WKViewAdjustScrollers(WKViewRef view);
void _WKViewDraw(CGContextRef context, WKViewRef view, CGRect dirtyRect, bool onlyDrawVisibleRect);
void _WKViewSetViewContext (WKViewRef view, WKViewContext *context);

#ifdef __cplusplus
}
#endif
