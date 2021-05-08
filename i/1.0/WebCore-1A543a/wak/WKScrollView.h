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

#define ScrollerAlphaHidden     0.4
#define ScrollerAlphaShown      0.8
    
#define WKScrollViewAllParts (WKScrollViewHorizontalScrollerPart | WKScrollViewVerticalScrollerPart)    
    
typedef enum WKScrollViewParts {
    WKScrollViewNoPart = 0,
    WKScrollViewHorizontalScrollerPart = 0x1,
    WKScrollViewVerticalScrollerPart = 0x2
} WKScrollViewParts;

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
    WKScrollerViewRef horizontalScroller;
    WKScrollerViewRef verticalScroller;
    
    WKScrollViewParts parts;
    WKScrollViewParts allowedParts;
    
    CGPoint mouseDownPoint;
    CGPoint lastDraggedPoint;
    unsigned int mouseDraggedStartedPan:1;
};

extern WKClassInfo WKScrollViewClassInfo;

WKScrollViewRef WKScrollViewCreateWithFrame (CGRect rect, WKScrollViewParts parts);
void WKScrollViewInitialize (WKScrollViewRef view, WKScrollViewParts parts);

WKScrollViewParts WKScrollViewGetParts (WKScrollViewRef view);
void WKScrollViewSetParts (WKScrollViewRef view, WKScrollViewParts parts);

WKScrollViewParts WKScrollViewGetAllowedParts (WKScrollViewRef view);
void WKScrollViewSetAllowedParts (WKScrollViewRef view, WKScrollViewParts parts);

WKClipViewRef WKScrollViewGetContentView (WKScrollViewRef view);
void WKScrollViewSetContentView (WKScrollViewRef view, WKClipViewRef contentView);

WKViewRef WKScrollViewGetDocumentView (WKScrollViewRef view);
void WKScrollViewSetDocumentView (WKScrollViewRef view, WKViewRef documentView);

void WKScrollViewTile (WKScrollViewRef view);

void WKScrollViewAdjustScrollers (WKScrollViewRef view);

bool WKScrollViewScrollToPoint(WKScrollViewRef view, CGPoint point);

void WKScrollViewShowScrollers(WKScrollViewRef view);
void WKScrollViewHideScrollers(WKScrollViewRef view);

#ifdef __cplusplus
}
#endif
