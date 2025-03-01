/*
 * Copyright (C) 2009, Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TileCache_h
#define TileCache_h

#if PLATFORM(IOS)

#include "Color.h"
#include "FloatRect.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#ifdef __OBJC__
@class CALayer;
@class TileCacheTombstone;
@class TileLayer;
@class WAKWindow;
#else
class CALayer;
class TileCacheTombstone;
class TileLayer;
class WAKWindow;
#endif

namespace WebCore {

class TileGrid;

class TileCache {
    WTF_MAKE_NONCOPYABLE(TileCache);
public:
    TileCache(WAKWindow*);
    ~TileCache();

    CGFloat screenScale() const;

    void setNeedsDisplay();
    void setNeedsDisplayInRect(const IntRect&);
    
    void layoutTiles();
    void layoutTilesNow();
    void layoutTilesNowForRect(const IntRect&);
    void removeAllNonVisibleTiles();
    void removeAllTiles();
    void removeForegroundTiles();

    // If 'contentReplacementImage' is not NULL, drawLayer() draws
    // contentReplacementImage instead of the page content.  We assume the
    // image is to be drawn at the origin and scaled to match device pixels.
    void setContentReplacementImage(RetainPtr<CGImageRef>);
    RetainPtr<CGImageRef> contentReplacementImage() const;
    
    void setTileBordersVisible(bool);
    bool tileBordersVisible() const { return m_tileBordersVisible; }
    
    void setTilePaintCountersVisible(bool);
    bool tilePaintCountersVisible() const { return m_tilePaintCountersVisible; }

    void setAcceleratedDrawingEnabled(bool enabled) { m_acceleratedDrawingEnabled = enabled; }
    bool acceleratedDrawingEnabled() const { return m_acceleratedDrawingEnabled; }

    void setKeepsZoomedOutTiles(bool);
    bool keepsZoomedOutTiles() const { return m_keepsZoomedOutTiles; }

    void setZoomedOutScale(float);
    float zoomedOutScale() const;
    
    void setCurrentScale(float);
    float currentScale() const;
    
    bool tilesOpaque() const;
    void setTilesOpaque(bool);
    
    enum TilingMode {
        Normal,
        Minimal,
        Panning,
        Zooming,
        Disabled,
        ScrollToTop
    };
    TilingMode tilingMode() const { return m_tilingMode; }
    void setTilingMode(TilingMode);

    typedef enum {
        TilingDirectionUp,
        TilingDirectionDown,
        TilingDirectionLeft,
        TilingDirectionRight,
    } TilingDirection;
    void setTilingDirection(TilingDirection);
    TilingDirection tilingDirection() const;

    bool hasPendingDraw() const;
    
    void hostLayerSizeChanged();
    
    static void setLayerPoolCapacity(unsigned capacity);
    static void drainLayerPool();

    // Logging
    void dumpTiles();

    // Internal
    void doLayoutTiles();
    void drawLayer(TileLayer* layer, CGContextRef context);
    void prepareToDraw();
    void finishedCreatingTiles(bool didCreateTiles, bool createMore);
    FloatRect visibleRectInLayer(CALayer *layer) const;
    CALayer* hostLayer() const;
    unsigned tileCapacityForGrid(TileGrid*);
    Color colorForGridTileBorder(TileGrid*) const;

    void doPendingRepaints();

    bool isSpeculativeTileCreationEnabled() const { return m_isSpeculativeTileCreationEnabled; }
    void setSpeculativeTileCreationEnabled(bool);
    
    enum SynchronousTileCreationMode { CoverVisibleOnly, CoverSpeculative };

    bool tileControllerShouldUseLowScaleTiles() const { return m_tileControllerShouldUseLowScaleTiles; }
    void setTileControllerShouldUseLowScaleTiles(bool flag) { m_tileControllerShouldUseLowScaleTiles = flag; }

private:
    TileGrid* activeTileGrid() const;
    TileGrid* inactiveTileGrid() const;

    void updateTilingMode();
    bool isTileInvalidationSuspended() const;
    bool isTileCreationSuspended() const;
    void flushSavedDisplayRects();
    void invalidateTiles(const IntRect& dirtyRect);
    void setZoomedOutScaleInternal(float scale);
    void commitScaleChange();
    void bringActiveTileGridToFront();
    void adjustTileGridTransforms();
    void removeAllNonVisibleTilesInternal();
    void createTilesInActiveGrid(SynchronousTileCreationMode);
    void scheduleLayerFlushForPendingRepaint();

    void tileCreationTimerFired(Timer<TileCache>*);

    void drawReplacementImage(TileLayer*, CGContextRef, CGImageRef);
    void drawWindowContent(TileLayer*, CGContextRef, CGRect dirtyRect);

    WAKWindow* m_window;

    RetainPtr<CGImageRef> m_contentReplacementImage;

    bool m_keepsZoomedOutTiles;
    
    bool m_hasPendingLayoutTiles;
    bool m_hasPendingUpdateTilingMode;
    // Ensure there are no async calls on a dead tile cache.
    RetainPtr<TileCacheTombstone> m_tombstone;
    
    TilingMode m_tilingMode;
    TilingDirection m_tilingDirection;
    
    IntSize m_tileSize;
    bool m_tilesOpaque;

    bool m_tileBordersVisible;
    bool m_tilePaintCountersVisible;
    bool m_acceleratedDrawingEnabled;
    bool m_isSpeculativeTileCreationEnabled;

    bool m_didCallWillStartScrollingOrZooming;
    OwnPtr<TileGrid> m_zoomedOutTileGrid;
    OwnPtr<TileGrid> m_zoomedInTileGrid;
    
    Timer<TileCache> m_tileCreationTimer;

    Vector<IntRect> m_savedDisplayRects;

    float m_currentScale;

    float m_pendingScale;
    float m_pendingZoomedOutScale;

    mutable Mutex m_tileMutex;
    mutable Mutex m_savedDisplayRectMutex;
    mutable Mutex m_contentReplacementImageMutex;

    bool m_tileControllerShouldUseLowScaleTiles;
};

}

#endif // PLATFORM(IOS)

#endif // TileCache_h

