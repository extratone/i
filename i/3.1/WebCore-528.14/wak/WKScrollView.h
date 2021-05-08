/*
 *  WKScrollView.h
 *  WebCore
 *
 *  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
 *
 */
#import "WKView.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*WKScrollViewShouldScrollCallback)(WKScrollViewRef scrollView, CGPoint scrollPoint, void *userInfo);

typedef struct _WKScrollViewContext {
    WKViewContext viewContext;
    WKScrollViewShouldScrollCallback shouldScrollCallback;
    void *shouldScrollUserInfo;
} WKScrollViewContext;

struct WKScrollView {
    struct WKView view;
    
    WKScrollViewContext scrollContext;
    WKClipViewRef contentView;
    
    CGPoint mouseDownPoint;
    CGPoint lastDraggedPoint;
    unsigned int mouseDraggedStartedPan:1;
};

extern WKClassInfo WKScrollViewClassInfo;

WKScrollViewRef WKScrollViewCreateWithFrame (CGRect rect);
void WKScrollViewInitialize (WKScrollViewRef view);

WKClipViewRef WKScrollViewGetContentView (WKScrollViewRef view);
void WKScrollViewSetContentView (WKScrollViewRef view, WKClipViewRef contentView);

WKViewRef WKScrollViewGetDocumentView (WKScrollViewRef view);
void WKScrollViewSetDocumentView (WKScrollViewRef view, WKViewRef documentView);

void WKScrollViewTile (WKScrollViewRef view);

void WKScrollViewAdjustScrollers (WKScrollViewRef view);

bool WKScrollViewScrollToPoint(WKScrollViewRef view, CGPoint point);

#ifdef __cplusplus
}
#endif
