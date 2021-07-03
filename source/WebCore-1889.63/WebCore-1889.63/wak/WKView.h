//
//  WKView.h
//
//  Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc.  All rights reserved.
//

#ifndef WKView_h
#define WKView_h

#import "WKUtilities.h"
#import <CoreGraphics/CoreGraphics.h>

#ifdef __cplusplus
extern "C" {
#endif

@class WAKWindow;

enum {
    NSViewNotSizable = 0,
    NSViewMinXMargin = 1,
    NSViewWidthSizable = 2,
    NSViewMaxXMargin = 4,
    NSViewMinYMargin = 8,
    NSViewHeightSizable = 16,
    NSViewMaxYMargin = 32
};    
    
typedef enum {
    WKViewNotificationViewDidMoveToWindow,
    WKViewNotificationViewFrameSizeChanged,
    WKViewNotificationViewDidScroll
} WKViewNotificationType;

typedef enum {
    WKViewResponderAcceptsFirstResponder,
    WKViewResponderBecomeFirstResponder,
    WKViewResponderResignFirstResponder,
} WKViewResponderCallbackType;

typedef void (*WKViewDrawCallback)(WKViewRef view, CGRect dirtyRect, void *userInfo); 
typedef void (*WKViewNotificationCallback)(WKViewRef view, WKViewNotificationType type, void *userInfo);
typedef bool (*WKViewResponderCallback)(WKViewRef view, WKViewResponderCallbackType type, void *userInfo);
typedef void (*WKViewWillRemoveSubviewCallback)(WKViewRef view, WKViewRef subview);
typedef void (*WKViewInvalidateGStateCallback)(WKViewRef view);

typedef struct _WKViewContext {
    WKViewNotificationCallback notificationCallback;
    void *notificationUserInfo;
    WKViewResponderCallback responderCallback;
    void *responderUserInfo;
    WKViewWillRemoveSubviewCallback willRemoveSubviewCallback;
    WKViewInvalidateGStateCallback invalidateGStateCallback;
} WKViewContext;

struct WKView {
    WKObject isa;
    
    WKViewContext *context;
    
    __unsafe_unretained WAKWindow *window;

    WKViewRef superview;
    CFMutableArrayRef subviews;

    CGPoint origin;
    CGRect bounds;
    
    unsigned int autoresizingMask;
    
    float scale;

    // This is really a WAKView.
    void *wrapper;
};

extern WKClassInfo WKViewClassInfo;

WKViewRef WKViewCreateWithFrame (CGRect rect, WKViewContext *context);
void WKViewInitialize (WKViewRef view, CGRect rect, WKViewContext *context);

void WKViewSetViewContext (WKViewRef view, WKViewContext *context);
void WKViewGetViewContext (WKViewRef view, WKViewContext *context);

CGRect WKViewGetBounds (WKViewRef view);

void WKViewSetFrameOrigin (WKViewRef view, CGPoint newPoint);
void WKViewSetFrameSize (WKViewRef view, CGSize newSize);
void WKViewSetBoundsOrigin(WKViewRef view, CGPoint newOrigin);
void WKViewSetBoundsSize (WKViewRef view, CGSize newSize);

CGRect WKViewGetFrame (WKViewRef view);

void WKViewSetScale (WKViewRef view, float scale);
float WKViewGetScale (WKViewRef view);
CGAffineTransform _WKViewGetTransform(WKViewRef view);

WAKWindow *WKViewGetWindow (WKViewRef view);

CFArrayRef WKViewGetSubviews (WKViewRef view);

WKViewRef WKViewGetSuperview (WKViewRef view);

void WKViewAddSubview (WKViewRef view, WKViewRef subview);
void WKViewRemoveFromSuperview (WKViewRef view);

CGPoint WKViewConvertPointToSuperview (WKViewRef view, CGPoint aPoint);
CGPoint WKViewConvertPointFromSuperview (WKViewRef view, CGPoint aPoint);
CGPoint WKViewConvertPointToBase(WKViewRef view, CGPoint aPoint);
CGPoint WKViewConvertPointFromBase(WKViewRef view, CGPoint aPoint);

CGRect WKViewConvertRectToSuperview (WKViewRef view, CGRect aRect);
CGRect WKViewConvertRectFromSuperview (WKViewRef view, CGRect aRect);
CGRect WKViewConvertRectToBase (WKViewRef view, CGRect r);
CGRect WKViewConvertRectFromBase (WKViewRef view, CGRect aRect);

CGRect WKViewGetVisibleRect (WKViewRef view);

WKViewRef WKViewFirstChild (WKViewRef view);
WKViewRef WKViewNextSibling (WKViewRef view);
WKViewRef WKViewTraverseNext (WKViewRef view);

bool WKViewAcceptsFirstResponder (WKViewRef view);
bool WKViewBecomeFirstResponder (WKViewRef view);
bool WKViewResignFirstResponder (WKViewRef view);

unsigned int WKViewGetAutoresizingMask(WKViewRef view);
void WKViewSetAutoresizingMask (WKViewRef view, unsigned int mask);

void WKViewScrollToRect(WKViewRef view, CGRect rect);

#ifdef __cplusplus
}
#endif

#endif // WKView_h
