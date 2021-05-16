/*
 *  WKScrollerView.h
 *  WebCore
 *
 *  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
 *
 */
#import "WKView.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum WKScrollerViewPart {
    WKScrollerViewNoPart = 0,
    WKScrollerViewKnobPart = 0x1,
    WKScrollerViewKnobSlotPart = 0x2,
    WKScrollerViewIncrementButtonPart = 0x4,
    WKScrollerViewDecrementButtonPart = 0x8
} WKScrollerViewPart;

#define WKScrollerViewAllParts (WKScrollerViewKnobPart | WKScrollerViewIncrementButtonPart | WKScrollerViewDecrementButtonPart)

typedef enum WKScrollerViewOrientation {
    WKScrollerViewHorizontalOrientation,
    WKScrollerViewVerticalOrientation
} WKScrollerViewOrientation;

typedef void(*WKScrollerChangedCallback)(WKScrollerViewRef view, void *userInfo); 

struct WKScrollerView {
    struct WKView view;

    WKScrollerViewPart parts;
    WKScrollerViewOrientation orientation;
    
    WKViewContext scrollerContext;

    float percent;
    
    WKScrollerViewPart hitPart;
    float mouseDownKnobOffset;
    
    WKScrollerChangedCallback scrollerChangedCallback;
    void *scrollChangedCallbackUserInfo;
    
    unsigned int enabled:1;
    
    float alpha;
};

extern WKClassInfo WKScrollerViewClassInfo;

float WKScrollerGetScrollerWidth(void);

WKScrollerViewRef WKScrollerViewCreateWithFrame (CGRect rect, WKScrollerViewOrientation orientation, WKScrollerViewPart parts);
void WKScrollerViewInitialize (WKScrollerViewRef view, WKScrollerViewOrientation orientation, WKScrollerViewPart part);

void WKScrollerViewSetScrollerChangedCallback (WKScrollerViewRef view, WKScrollerChangedCallback callback, void *userInfo);
float WKScrollerViewGetPercent (WKScrollerViewRef view);
void WKScrollerViewSetPercent (WKScrollerViewRef view, float percent);

void WKScrollerViewSetAlpha(WKScrollerViewRef view, float alpha);

#ifdef __cplusplus
}
#endif
