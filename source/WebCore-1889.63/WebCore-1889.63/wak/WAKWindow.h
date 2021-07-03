//
//  WAKWindow.h
//
//  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.  All rights reserved.
//

#ifndef WAKWindow_h
#define WAKWindow_h

#import "WAKAppKitStubs.h"
#import "WAKResponder.h"
#import "WAKView.h"
#import "WKContentObservation.h"
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

@class CALayer;
@class WebEvent;

#ifdef __cplusplus
namespace WebCore {
    class TileCache;
}
typedef WebCore::TileCache TileCache;
#else
typedef struct TileCache TileCache;
#endif

typedef enum {
    kWAKWindowTilingModeNormal,
    kWAKWindowTilingModeMinimal,
    kWAKWindowTilingModePanning,
    kWAKWindowTilingModeZooming,
    kWAKWindowTilingModeDisabled,
    kWAKWindowTilingModeScrollToTop,
} WAKWindowTilingMode;

typedef enum {
    kWAKTilingDirectionUp,
    kWAKTilingDirectionDown,
    kWAKTilingDirectionLeft,
    kWAKTilingDirectionRight,
} WAKTilingDirection;

extern NSString * const WAKWindowScreenScaleDidChangeNotification;
extern NSString * const WAKWindowVisibilityDidChangeNotification;

@interface WAKWindow : WAKResponder
{
    CALayer* _hostLayer;
    TileCache* _tileCache;
    CGRect _frozenVisibleRect;
    CALayer *_rootLayer;

    CGSize _screenSize;
    CGSize _availableScreenSize;
    CGFloat _screenScale;

    CGRect _frame;

    WAKView *_contentView;
    WAKView *_responderView;
    WAKView *_nextResponder;

    BOOL _visible;
    BOOL _useOrientationDependentFontAntialiasing;
}

@property (nonatomic, assign) BOOL useOrientationDependentFontAntialiasing;

// If non-NULL, contentReplacementImage will draw into tiles instead of web content.
@property (nonatomic) CGImageRef contentReplacementImage;

// Create layer hosted window
- (id)initWithLayer:(CALayer *)hostLayer;
// Create unhosted window for manual painting
- (id)initWithFrame:(CGRect)frame;

- (CALayer*)hostLayer;

- (void)setContentView:(WAKView *)aView;
- (WAKView *)contentView;
- (void)close;
- (WAKView *)firstResponder;

- (NSPoint)convertBaseToScreen:(NSPoint)aPoint;
- (NSPoint)convertScreenToBase:(NSPoint)aPoint;
- (BOOL)isKeyWindow;
- (void)makeKeyWindow;
- (BOOL)isVisible;
- (void)setVisible:(BOOL)visible;
- (NSSelectionDirection)keyViewSelectionDirection;
- (BOOL)makeFirstResponder:(NSResponder *)aResponder;
- (WAKView *)_newFirstResponderAfterResigning;
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag;
- (CGRect)frame;
- (void)setContentRect:(CGRect)rect;
- (void)setScreenSize:(CGSize)size;
- (CGSize)screenSize;
- (void)setAvailableScreenSize:(CGSize)size;
- (CGSize)availableScreenSize;
- (void)setScreenScale:(CGFloat)scale;
- (CGFloat)screenScale;
- (void)setRootLayer:(CALayer *)layer;
- (CALayer *)rootLayer;
- (void)sendEvent:(WebEvent *)anEvent;
- (void)sendEventSynchronously:(WebEvent *)anEvent;
- (void)sendMouseMoveEvent:(WebEvent *)anEvent contentChange:(WKContentChange *)aContentChange;

// Thread safe way of providing the "usable" rect of the WAKWindow in the viewport/scrollview.
- (CGRect)exposedScrollViewRect;
// setExposedScrollViewRect should only ever be called from UIKit.
- (void)setExposedScrollViewRect:(CGRect)exposedScrollViewRect;

// Tiling support
- (void)layoutTiles;
- (void)layoutTilesNow;
- (void)layoutTilesNowForRect:(CGRect)rect;
- (void)setNeedsDisplay;
- (void)setNeedsDisplayInRect:(CGRect)rect;
- (BOOL)tilesOpaque;
- (void)setTilesOpaque:(BOOL)opaque;
- (CGRect)visibleRect;
// The extended visible rect includes the area outside superviews with
// masksToBounds set to NO.
- (CGRect)extendedVisibleRect;
- (void)removeAllNonVisibleTiles;
- (void)removeAllTiles;
- (void)removeForegroundTiles;
- (void)setTilingMode:(WAKWindowTilingMode)mode;
- (WAKWindowTilingMode)tilingMode;
- (void)setTilingDirection:(WAKTilingDirection)tilingDirection;
- (WAKTilingDirection)tilingDirection;
- (BOOL)hasPendingDraw;
- (void)displayRect:(NSRect)rect;
- (void)setZoomedOutTileScale:(float)scale;
- (float)zoomedOutTileScale;
- (void)setCurrentTileScale:(float)scale;
- (float)currentTileScale;
- (void)setKeepsZoomedOutTiles:(BOOL)keepsZoomedOutTiles;
- (BOOL)keepsZoomedOutTiles;
- (TileCache *)tileCache;

- (void)setTileControllerShouldUseLowScaleTiles:(BOOL)lowScaleTiles;

- (void)dumpTiles;

- (void)setTileBordersVisible:(BOOL)visible;
- (void)setTilePaintCountsVisible:(BOOL)visible;
- (void)setAcceleratedDrawingEnabled:(BOOL)enabled;

- (void)freezeVisibleRect;
- (void)unfreezeVisibleRect;
- (void)willRotate;
- (void)didRotate;

- (BOOL)useOrientationDependentFontAntialiasing;
- (void)setUseOrientationDependentFontAntialiasing:(BOOL)aa;
+ (BOOL)hasLandscapeOrientation;
+ (void)setOrientationProvider:(id)provider;

+ (WebEvent *)currentEvent;

- (NSString *)recursiveDescription;
@end

#endif

