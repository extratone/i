//
//  WKWindow.h
//
//  Copyright (C) 2005, 2006, 2007, 2008, Apple Inc.  All rights reserved.
//
#ifndef WKWindow_h
#define WKWindow_h

#import <CoreGraphics/CoreGraphics.h>
#import <CoreGraphics/CGSTypes.h>
#import <GraphicsServices/GSEvent.h>

#import "WebCoreThread.h"
#import "WKTypes.h"
#import "WKUtilities.h"

#ifdef __cplusplus
namespace WebCore {
    class TiledSurface;
}
typedef WebCore::TiledSurface TiledSurface;
#else
typedef struct TiledSurface TiledSurface;
#endif

#ifdef __OBJC__
@class WAKWindow;
#else
typedef struct WAKWindow WAKWindow;
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct WKWindow {
    WKObject obj;
    WAKWindow* wakWindow;
    CGRect frame;
    WKViewRef contentView;
    WKViewRef responderView;
    TiledSurface* tiledSurface;
    unsigned int useOrientationDependentFontAntialiasing:1;
    unsigned int isOffscreen:1;
};

extern WKClassInfo WKWindowClassInfo;

WKWindowRef WKWindowCreate(WAKWindow* wakWindow, CGRect contentRect);

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
void WKWindowSetNeedsDisplayInRect(WKWindowRef window, CGRect rect);
    
void WKWindowDrawRect(WKWindowRef window, CGRect dirtyRect);

void WKWindowSetOffscreen(WKWindowRef window, bool flag);
    
void WKWindowSetTiledSurface(WKWindowRef window, TiledSurface*);
TiledSurface* WKWindowGetTiledSurface(WKWindowRef window);

#ifdef __cplusplus
}
#endif

#endif
