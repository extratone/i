//
//  WKWindow.h
//
//  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
//
#import <CoreGraphics/CoreGraphics.h>
#import <CoreGraphics/CGSTypes.h>

#import "GraphicsServices/GSEvent.h"
#import "WKTypes.h"
#import "WKUtilities.h"
#import "WebCoreThread.h"

#ifdef __cplusplus
extern "C" {
#endif
    
typedef void (*WKWindowNeedsDisplayCallback)(WKWindowRef window, void *userInfo);

struct WKWindow {
    WKObject obj;
    CGRect frame;
    WKViewRef contentView;
    WKViewRef responderView;
    GSEventRef currentEvent;
    WKWindowNeedsDisplayCallback needsDisplayCallback;
    void *needsDisplayUserInfo;
    unsigned int needsDisplay:1;
    unsigned int isSuspendedWindow:1;
    unsigned int isOffscreen:1;
};

extern WKClassInfo WKWindowClassInfo;

WKWindowRef WKWindowCreate(CGRect contentRect);

void WKWindowSetContentView (WKWindowRef window, WKViewRef aView);
WKViewRef WKWindowGetContentView (WKWindowRef window);

void WKWindowSetContentRect(WKWindowRef window, CGRect contentRect);
CGRect WKWindowGetContentRect(WKWindowRef window);

void WKWindowClose (WKWindowRef window);

bool WKWindowMakeFirstResponder (WKWindowRef window, WKViewRef view);
WKViewRef WKWindowFirstResponder (WKWindowRef window);
void WKWindowSendEvent (WKWindowRef window, GSEventRef event);

CGPoint WKWindowConvertBaseToScreen (WKWindowRef window, CGPoint point);
CGPoint WKWindowConvertScreenToBase (WKWindowRef window, CGPoint point);

void WKWindowSetFrame(WKWindowRef window, CGRect frame, bool display);

GSEventRef WKEventGetCurrentEvent(void);

void WKWindowPrepareForDrawing(WKWindowRef window);

void WKWindowSetNeedsDisplay(WKWindowRef window, bool flag);
bool WKWindowNeedsDisplay(WKWindowRef window);

void WKWindowSetNeedsDisplayCallback(WKWindowRef window, WKWindowNeedsDisplayCallback, void *userInfo);
void WKWindowDrawRect(WKWindowRef window, CGRect dirtyRect);

void WKWindowSetIsSuspendedWindow(WKWindowRef window, bool flag);
bool WKWindowIsSuspendedWindow(WKWindowRef window);

void WKWindowSetOffscreen(WKWindowRef window, bool flag);

#ifdef __cplusplus
}
#endif
