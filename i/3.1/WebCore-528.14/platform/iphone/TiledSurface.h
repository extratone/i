/*
 *  TiledSurface.h
 *  WebCore
 *
 *  Copyright (C) 2009, Apple Inc.  All rights reserved.
 *
 */

#ifndef TiledSurface_h
#define TiledSurface_h

#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Timer.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#ifdef __OBJC__
@class CALayer;
@class NSString;
@class TileLayer;
@class WAKWindow;
@class WebThreadCaller;
#else
class CALayer;
class NSString;
class TileLayer;
class WAKWindow;
class WebThreadCaller;
#endif

namespace WebCore {

    typedef const NSString* TileMinificationFilter;

    class TiledSurface : Noncopyable {
    public:
        TiledSurface(CALayer* hostLayer, WAKWindow* window);
        ~TiledSurface();

        IntRect visibleRect() const;
        
        IntSize size() const;
        IntRect frame() const;
        
        void setNeedsDisplay();
        void setNeedsDisplayInRect(const IntRect&);
        void drawLayer(CALayer* layer, CGContextRef context);
        
        void layoutTiles();
        void layoutTilesNow();
        
        bool tilesOpaque() const;
        void setTilesOpaque(bool);
        
        TileMinificationFilter tileMinificationFilter() const;
        void setTileMinificationFilter(TileMinificationFilter);
        
        void removeAllNonVisibleTiles();
        void removeAllTiles();
        
        enum TilingMode {
            Normal,
            Minimal,
            Panning,
            Zooming,
            Disabled
        };
        TilingMode tilingMode() const;
        void setTilingMode(TilingMode);
        
        void updateTilingMode();
        void doLayoutTiles();
        void prepareToDraw();
        
        bool hasPendingDraw() const;

    private:
        // Refcount the tiles so they work nicely in vector and we know when to remove the tile layer from the parent.
        class Tile : public RefCounted<Tile> {
        public:
            static PassRefPtr<Tile> create(TiledSurface* surface, const IntRect& rect) { return adoptRef<Tile>(new Tile(surface, rect)); }
            ~Tile();

            TileLayer* tileLayer() const { return m_tileLayer.get(); }
            void invalidateRect(const IntRect& rectInSurface);
            IntRect rect() const { return m_rect; }
            void setRect(const IntRect& tileRect);

        private:
            Tile(TiledSurface* surface, const IntRect& rect);
            
            TiledSurface* m_surface;
            RetainPtr<TileLayer> m_tileLayer;
            IntRect m_rect;
        };

        void coverWithTiles(const IntRect& coverRect, const IntRect& keepRect);
        void dropTilesOutsideRect(const IntRect& keepRect);
        Tile* tileForIndex(unsigned xIndex, unsigned yIndex) const;
        IntRect tileRectForIndex(unsigned xIndex, unsigned yIndex) const;
        TiledSurface::Tile* tileForPoint(const IntPoint& point) const;
        bool tilesCover(const IntRect& rect) const;
        void centerTileGridOrigin(const IntRect& visibleRect);
        void tileIndexForPoint(const IntPoint& point, unsigned& xIndex, unsigned& yIndex) const;
        void coverVisibleAreaWithTiles();
        void shrinkToMinimalTiles();
        bool pointsOnSameTile(const IntPoint&, const IntPoint&) const;
        void adjustForPageBounds(IntRect& rect) const;
        bool isPaintingSuspended() const;
        bool isTileCreationSuspended() const;
        bool checkDoSingleTileLayout();
        void flushSavedDisplayRects();
        void invalidateTiles(const IntRect& dirtyRect);
        void calculateCoverAndKeepRectForMemoryLevel(const IntRect& visibleRect, IntRect& keepRect, IntRect& coverRect, bool& centerGrid);
        
        RetainPtr<CALayer> m_hostCALayer;
        WAKWindow* m_window;
        
        RetainPtr<WebThreadCaller> m_webThreadCaller;
        
        TilingMode m_tilingMode;
        TileMinificationFilter m_tileMinificationFilter;
        
        IntPoint m_tileGridOrigin;
        IntSize m_tileSize;
        bool m_tilesOpaque;
        IntRect m_previousCoverRect;
        IntRect m_previousKeepRect;
        bool m_didCallWillStartScrollingOrZooming;
        
        Vector<Vector<RefPtr<Tile> > > m_tileGrid;
        
        Vector<IntRect> m_savedDisplayRects;
        
        mutable Mutex m_tileMutex;
        mutable Mutex m_savedDisplayRectMutex;
    };

}

#endif

