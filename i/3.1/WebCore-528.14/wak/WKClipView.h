/*
 *  WKClipView.h
 *  WebCore
 *
 *  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
 *
 */
#import "WKView.h"

#ifdef __cplusplus
extern "C" {
#endif

struct WKClipView {
    struct WKView view;
    WKViewRef documentView;
    unsigned int copiesOnScroll:1;
};

extern WKClassInfo WKClipViewClassInfo;

WKClipViewRef WKClipViewCreateWithFrame (CGRect rect, WKViewContext *context);
void WKClipViewInitialize (WKClipViewRef view);

WKViewRef WKClipViewGetDocumentView (WKClipViewRef view);
void WKClipViewSetDocumentView (WKClipViewRef view, WKViewRef documentView);

bool WKClipViewCopiesOnScroll (WKClipViewRef view);
void WKClipViewSetCopiesOnScroll (WKClipViewRef view, bool flag);

CGRect WKClipViewGetDocumentVisibleRect (WKClipViewRef view);

#ifdef __cplusplus
}
#endif
