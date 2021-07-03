//
//  WKViewPrivate.h
//
//  Copyright (C) 2005, 2006, 2007, 2009 Apple Inc.  All rights reserved.
//

#ifndef WKViewPrivate_h
#define WKViewPrivate_h

#import "WKView.h"

#ifdef __cplusplus
extern "C" {
#endif    

void _WKViewSetWindow (WKViewRef view, WAKWindow *window);
void _WKViewSetSuperview (WKViewRef view, WKViewRef superview);
void _WKViewWillRemoveSubview(WKViewRef view, WKViewRef subview);
WKViewRef _WKViewBaseView (WKViewRef view);
void _WKViewAutoresize(WKViewRef view, const CGRect *oldSuperFrame, const CGRect *newSuperFrame);
void _WKViewSetViewContext (WKViewRef view, WKViewContext *context);

#ifdef __cplusplus
}
#endif

#endif // WKViewPrivate_h
