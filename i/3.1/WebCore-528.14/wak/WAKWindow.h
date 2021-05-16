//
//  WAKWindow.h
//
//  Copyright (C) 2005, 2006, 2007, 2009 Apple Inc.  All rights reserved.
//

#ifndef WAKWindow_h
#define WAKWindow_h

#import <Foundation/Foundation.h>

#import <CoreGraphics/CoreGraphics.h>

#import "WAKAppKitStubs.h"

#import "WAKResponder.h"
#import "WAKView.h"
#import "WKWindow.h"
#import "WKContentObservation.h"

@class CALayer;

typedef enum {
    kWAKWindowTilingModeNormal,
    kWAKWindowTilingModeMinimal,
    kWAKWindowTilingModePanning,
    kWAKWindowTilingModeZooming,
    kWAKWindowTilingModeDisabled
} WAKWindowTilingMode;

@interface WAKWindow : WAKResponder
{
    WKWindowRef window;
}
// Create layer hosted window
- (id)initWithLayer:(CALayer *)hostLayer;
// Create unhosted window for manual painting
- (id)initWithFrame:(CGRect)frame;

- (void)setContentView:(WAKView *)aView;
- (WAKView *)contentView;
- (void)close;
- (WAKResponder *)firstResponder;
- (NSPoint)convertBaseToScreen:(NSPoint)aPoint;
- (NSPoint)convertScreenToBase:(NSPoint)aPoint;
- (void)endEditingFor:(id)anObject;
- (int)windowNumber;
- (GSEventRef)currentEvent;
- (BOOL)isKeyWindow;
- (NSSelectionDirection)keyViewSelectionDirection;
- (BOOL)makeFirstResponder:(NSResponder *)aResponder;
- (WKWindowRef)_windowRef;
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag;
- (void)sendGSEvent:(id)aGSEventRef;
- (void)sendGSEvent:(id)aGSEventRef contentChange:(WKContentChange *)aContentChange;

- (id)attachedSheet;

- (BOOL)_needsToResetDragMargins;
- (void)_setNeedsToResetDragMargins:(BOOL)flag;

// Tiling support
- (void)layoutTiles;
- (void)layoutTilesNow;
- (void)setNeedsDisplay;
- (void)setNeedsDisplayInRect:(CGRect)rect;
- (BOOL)tilesOpaque;
- (void)setTilesOpaque:(BOOL)opaque;
- (NSString *)tileMinificationFilter;
- (void)setTileMinificationFilter:(NSString *)filter;
- (CGRect)visibleRect;
- (void)removeAllNonVisibleTiles;
- (void)removeAllTiles;
- (void)setTilingMode:(WAKWindowTilingMode)mode;
- (WAKWindowTilingMode)tilingMode;
- (BOOL)hasPendingDraw;

- (BOOL)useOrientationDependentFontAntialiasing;
- (void)setUseOrientationDependentFontAntialiasing:(BOOL)aa;
+ (BOOL)hasLandscapeOrientation;
+ (void)setOrientationProvider:(id)provider;

@end

#endif

