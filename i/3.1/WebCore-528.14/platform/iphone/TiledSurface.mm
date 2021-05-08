/*
 *  TiledSurface.mm
 *  WebCore
 *
 *  Copyright (C) 2009, Apple Inc.  All rights reserved.
 *
 */

#include "config.h"
#include "TiledSurface.h"

#include "WebCoreTextRenderer.h"
#include "WKGraphics.h"
#include "WAKWindow.h"
#include "SystemMemory.h"
#include "SystemTime.h"
#include <CoreGraphics/CoreGraphics.h>
#include <CoreGraphics/CGSRegion.h> 
#include <QuartzCore/QuartzCore.h>
#include <QuartzCore/QuartzCorePrivate.h>
#include <wtf/UnusedParam.h>

#define LOG_TILING 0

@interface WAKView (WebViewExtras)
- (void)_dispatchTileDidDraw:(CALayer*)tile;
- (void)_willStartScrollingOrZooming;
- (void)_didFinishScrollingOrZooming;
@end

@interface TileLayer : CALayer
{
    WebCore::TiledSurface* _tiledSurface;
}
@end

@implementation TileLayer
- (id)initWithTiledSurface:(WebCore::TiledSurface*)tiledSurface
{
    self = [super init];
    if (self)
        _tiledSurface = tiledSurface;
    return self;
}

- (void)removeFromSuperlayer
{
    _tiledSurface = 0;
    [super removeFromSuperlayer];
}

- (void)setNeedsDisplayInRect:(CGRect)rect
{
    [super setNeedsDisplayInRect:rect];
}

- (void)display
{
    ASSERT(WebThreadIsLockedOrDisabled());
    // This may trigger WebKit layout and generate more repaint rects.
    if (_tiledSurface)
        _tiledSurface->prepareToDraw();
    
    [super display];
}

- (void)drawInContext:(CGContextRef)context
{
    if (_tiledSurface)
        _tiledSurface->drawLayer(self, context);
}

- (id<CAAction>)actionForKey:(NSString *)key
{
    UNUSED_PARAM(key);
    // Disable all default actions
    return nil;
}
@end

@interface WebThreadCaller : NSObject
{
    WebCore::TiledSurface* _tiledSurface;
    bool _hasPendingDoLayoutTiles;
    bool _hasPendingUpdateTilingMode;
}
@end

@implementation WebThreadCaller
- (id)initWithTiledSurface:(WebCore::TiledSurface*)tiledSurface
{
    self = [super init];
    if (self)
        _tiledSurface = tiledSurface;
    return self;
}

- (void)resetTiledSurface
{
    _tiledSurface = 0;
}

- (void)doLayoutTiles
{
    if (!WebThreadIsCurrent() && WebThreadIsEnabled()) {
        if (_hasPendingDoLayoutTiles)
            return;
        _hasPendingDoLayoutTiles = true;
        NSInvocation *invocation = WebThreadCreateNSInvocation(self, _cmd);
        WebThreadCallAPI(invocation);
        return;
    }
    _hasPendingDoLayoutTiles = false;
    if (_tiledSurface)
        _tiledSurface->doLayoutTiles();
}

- (void)updateTilingMode
{
    if (!WebThreadIsCurrent() && WebThreadIsEnabled()) {
        if (_hasPendingUpdateTilingMode)
            return;
        _hasPendingUpdateTilingMode = true;
        NSInvocation *invocation = WebThreadCreateNSInvocation(self, _cmd);
        WebThreadCallAPI(invocation);
        return;
    }
    _hasPendingUpdateTilingMode = false;
    if (_tiledSurface)
        _tiledSurface->updateTilingMode();
}
@end

namespace WebCore {

static bool canTileAggresively()
{
#if PLATFORM(IPHONE_SIMULATOR)
    return true;
#endif
    static bool canTileAggresively = systemTotalMemory() > 128 << 20;
    return canTileAggresively;
}

#if LOG_TILING
static int totalTileCount;
#endif

TiledSurface::Tile::Tile(TiledSurface* surface, const IntRect& tileRect)
    : m_surface(surface)
    , m_tileLayer(AdoptNS,[[TileLayer alloc] initWithTiledSurface:surface])
    , m_rect(tileRect)
{
    TileLayer* layer = m_tileLayer.get();
    [layer setMinificationFilter:surface->m_tileMinificationFilter];
    [layer setOpaque:m_surface->m_tilesOpaque];
    [layer setEdgeAntialiasingMask:0];
    [m_surface->m_hostCALayer.get() insertSublayer:layer atIndex:0];
    [layer setFrame:m_rect];
    invalidateRect(m_rect);
#if LOG_TILING
    ++totalTileCount;
    NSLog(@"new Tile (%d,%d) %d %d, count %d", tileRect.x(), tileRect.y(), tileRect.width(), tileRect.height(), totalTileCount);
#endif
}
    
TiledSurface::Tile::~Tile() 
{
    [tileLayer() removeFromSuperlayer];
#if LOG_TILING
    --totalTileCount;
    NSLog(@"delete Tile (%d,%d) %d %d, count %d", rect.x(), rect.y(), rect.width(), rect.height(), totalTileCount);
#endif
}

void TiledSurface::Tile::invalidateRect(const IntRect& rectInSurface)
{
    IntRect rect = intersection(rectInSurface, m_rect);
    if (rect.isEmpty())
        return;
    rect.move(IntPoint() - m_rect.topLeft());
    [tileLayer() setNeedsDisplayInRect:rect];
}
    
void TiledSurface::Tile::setRect(const IntRect& tileRect)
{
    if (m_rect == tileRect)
        return;
    m_rect = tileRect;
    TileLayer* layer = m_tileLayer.get();
    [layer setFrame:m_rect];
    [layer setNeedsDisplay];
}

TiledSurface::TiledSurface(CALayer* hostLayer, WAKWindow* window)
    : m_hostCALayer(hostLayer)
    , m_window(window)
    , m_webThreadCaller(AdoptNS, [[WebThreadCaller alloc] initWithTiledSurface:this])
    , m_tilingMode(Normal)
    , m_tileMinificationFilter(kCAFilterLinear)
    , m_tileSize(512, 512)
    , m_tilesOpaque(true)
    , m_didCallWillStartScrollingOrZooming(false)
{
}
    
TiledSurface::~TiledSurface()
{
    [m_webThreadCaller.get() resetTiledSurface];
}

IntSize TiledSurface::size() const
{
    return IntSize([m_hostCALayer.get() size]);
}
    
IntRect TiledSurface::frame() const
{
    return IntRect(IntPoint(), size());
}

bool TiledSurface::tilesOpaque() const
{
    return m_tilesOpaque;
}

void TiledSurface::setTilesOpaque(bool opaque)
{
    if (m_tilesOpaque == opaque)
        return;
    
    MutexLocker locker(m_tileMutex);
    
    m_tilesOpaque = opaque;
    
    unsigned ySize = m_tileGrid.size();
    for (unsigned yIndex = 0; yIndex < ySize; ++yIndex) {
        unsigned xSize =  m_tileGrid[yIndex].size();
        for (unsigned xIndex = 0; xIndex < xSize; ++xIndex) {
            Tile* tile = tileForIndex(xIndex, yIndex);
            if (tile)
                [tile->tileLayer() setOpaque:opaque];
        }
    }
}

TileMinificationFilter TiledSurface::tileMinificationFilter() const
{
    return m_tileMinificationFilter;
}

void TiledSurface::setTileMinificationFilter(TileMinificationFilter filter)
{
    if (m_tileMinificationFilter == filter)
        return;
    
    MutexLocker locker(m_tileMutex);
    
    m_tileMinificationFilter = filter;
    
    unsigned ySize = m_tileGrid.size();
    for (unsigned yIndex = 0; yIndex < ySize; ++yIndex) {
        unsigned xSize =  m_tileGrid[yIndex].size();
        for (unsigned xIndex = 0; xIndex < xSize; ++xIndex) {
            Tile* tile = tileForIndex(xIndex, yIndex);
            if (tile)
                [tile->tileLayer() setMinificationFilter:m_tileMinificationFilter];
        }
    }
}

IntRect TiledSurface::visibleRect() const
{
    CALayer* layer = m_hostCALayer.get();
    CGRect bounds = [layer bounds];
    CGRect rect = bounds;
    CALayer* superlayer = [layer superlayer];
    
    while (superlayer) {
        CGRect rectInSuper = [superlayer convertRect:rect fromLayer:layer];
        rect = CGRectIntersection([superlayer bounds], rectInSuper);
        layer = superlayer;
        superlayer = [layer superlayer];
    }
    
    if (layer != m_hostCALayer)
        rect = [m_hostCALayer.get() convertRect:rect fromLayer:layer];
    
    CGRect visibleRect = CGRectIntegral(rect);
    visibleRect = CGRectIntersection(bounds, visibleRect);
    return enclosingIntRect(visibleRect);
}
    
TiledSurface::Tile* TiledSurface::tileForIndex(unsigned xIndex, unsigned yIndex) const
{
    if (m_tileGrid.size() <= yIndex)
        return 0;
    const Vector<RefPtr<Tile> >& column = m_tileGrid[yIndex];
    if (column.size() <= xIndex)
        return 0;
    return column[xIndex].get();
}
    
IntRect TiledSurface::tileRectForIndex(unsigned xIndex, unsigned yIndex) const
{
    IntRect rect(xIndex * m_tileSize.width() - (m_tileGridOrigin.x() ? m_tileSize.width() - m_tileGridOrigin.x() : 0),
        yIndex * m_tileSize.height() - (m_tileGridOrigin.y() ? m_tileSize.height() - m_tileGridOrigin.y() : 0),
        m_tileSize.width(),
        m_tileSize.height());
    rect.intersect(IntRect(IntPoint(), size()));
    return rect;
}
    
void TiledSurface::tileIndexForPoint(const IntPoint& point, unsigned& xIndex, unsigned& yIndex) const
{
    ASSERT(m_tileGridOrigin.x() < m_tileSize.width());
    ASSERT(m_tileGridOrigin.y() < m_tileSize.height());
    int x = (point.x() + (m_tileGridOrigin.x() ? m_tileSize.width() - m_tileGridOrigin.x() : 0)) / m_tileSize.width();
    int y = (point.y() + (m_tileGridOrigin.y() ? m_tileSize.height() - m_tileGridOrigin.y() : 0)) / m_tileSize.height();
    xIndex = std::max(x, 0);
    yIndex = std::max(y, 0);
}
    
bool TiledSurface::pointsOnSameTile(const IntPoint& a, const IntPoint& b) const
{
    unsigned aXIndex;
    unsigned aYIndex;
    tileIndexForPoint(a, aXIndex, aYIndex);
    unsigned bXIndex;
    unsigned bYIndex;
    tileIndexForPoint(b, bXIndex, bYIndex);
    return aXIndex == bXIndex && aYIndex == bYIndex;
}

void TiledSurface::centerTileGridOrigin(const IntRect& visibleRect)
{
    if (visibleRect.width() > m_tileSize.width() || visibleRect.height() > m_tileSize.height())
        return;
    // Only center if all corners of the visible rect fall to different tiles.
    if (pointsOnSameTile(visibleRect.topLeft(), visibleRect.topRight()) || pointsOnSameTile(visibleRect.topLeft(), visibleRect.bottomLeft()))
        return;
    IntSize size = this->size();
    int coverX = size.width() > m_tileSize.width() ? visibleRect.x() - (m_tileSize.width() - visibleRect.width()) / 2 : 0;
    int coverY = size.height() > m_tileSize.height() ? visibleRect.y() - (m_tileSize.height() - visibleRect.height()) / 2 : 0;
    if (coverX < 0)
        coverX += m_tileSize.width();
    if (coverY < 0)
        coverY += m_tileSize.height();
    coverX %= m_tileSize.width();
    coverY %= m_tileSize.height();
    IntPoint origin(coverX, coverY);
    if (origin != m_tileGridOrigin) {
        coverWithTiles(IntRect(), IntRect());
        m_tileGridOrigin = origin;
        coverWithTiles(visibleRect, visibleRect);
    }
}
    
TiledSurface::Tile* TiledSurface::tileForPoint(const IntPoint& point) const
{
    unsigned xIndex;
    unsigned yIndex;
    tileIndexForPoint(point, xIndex, yIndex);
    return tileForIndex(xIndex, yIndex);
}

bool TiledSurface::tilesCover(const IntRect& rect) const
{
    return tileForPoint(rect.topLeft()) && tileForPoint(rect.topRight()) &&
        tileForPoint(rect.bottomLeft()) && tileForPoint(rect.bottomRight());
}

void TiledSurface::adjustForPageBounds(IntRect& rect) const
{
    // Adjust the rect so that it stays within the bounds and keeps the pixel size.
    IntRect bounds = frame();
    rect.move(rect.x() < bounds.x() ? bounds.x() - rect.x() : 0, 
              rect.y() < bounds.y() ? bounds.y() - rect.y() : 0);
    rect.move(rect.right() > bounds.right() ? bounds.right() - rect.right() : 0, 
              rect.bottom() > bounds.bottom() ? bounds.bottom() - rect.bottom() : 0);
    IntRect intersectRect = intersection(bounds, rect);
    if (intersectRect == rect || m_tilingMode == Minimal)
        return;
    if (intersectRect.isEmpty()) {
        rect = IntRect();
        return;
    }
    int pixels = rect.width() * rect.height();
    if (intersectRect.width() != rect.width())
        intersectRect.inflateY((pixels / intersectRect.width() - intersectRect.height()) / 2);
    else if (intersectRect.height() != rect.height())
        intersectRect.inflateX((pixels / intersectRect.height() - intersectRect.width()) / 2);
    rect = intersection(intersectRect, bounds);
}
    
bool TiledSurface::checkDoSingleTileLayout()
{
    IntSize size = this->size();
    if (size.width() > m_tileSize.width() || size.height() > m_tileSize.height())
        return false;
    
    IntRect frame = this->frame();
    if (m_tileGridOrigin != IntPoint(0, 0)) {
        coverWithTiles(IntRect(), IntRect());
        m_tileGridOrigin = IntPoint(0, 0);
    }
    coverWithTiles(frame, frame);
    return true;
}

void TiledSurface::calculateCoverAndKeepRectForMemoryLevel(const IntRect& visibleRect, IntRect& coverRect, IntRect& keepRect, bool& centerGrid)
{
    // Estimate how large area we want to cover with tiles based on the current memory level.
    int level = systemMemoryLevel();
    coverRect = visibleRect;
    centerGrid = false;
    if (level <= 10 || m_tilingMode == Minimal) {
        centerGrid = true;
        keepRect = coverRect;
    } else if (level <= 15) {
        centerGrid = true;
        keepRect = coverRect;
        keepRect.inflateY(m_tileSize.height() / 3);
    } else if (level <= 20) {
        centerGrid = true;
        coverRect.inflateY(visibleRect.height() / 3);
        keepRect = coverRect;
        keepRect.inflateX(m_tileSize.width() / 3);
        keepRect.inflateY(m_tileSize.height() / 3);
    } else if (level <= 25) {
        coverRect.inflateY(visibleRect.height() / 2);
        keepRect = coverRect;
        keepRect.inflateX(m_tileSize.width() / 3);
        keepRect.inflateY(m_tileSize.height() / 3);
    } else if (level <= 30) {
        coverRect.inflateX(visibleRect.width() / 3);
        coverRect.inflateY(visibleRect.height() / 2);
        keepRect = coverRect;
        keepRect.inflateX(m_tileSize.width() / 3);
        keepRect.inflateY(m_tileSize.height() / 3);
    } else {
        if (canTileAggresively()) {
            // For fast devices only
            coverRect.inflateX(visibleRect.width() / 2);
            coverRect.inflateY(visibleRect.height());
        } else {
            coverRect.inflateX(visibleRect.width() / 3);
            coverRect.inflateY(visibleRect.height() / 2);
        }
        keepRect = coverRect;
        keepRect.inflateX(m_tileSize.width() / 2);
        keepRect.inflateY(m_tileSize.height());
    }
    
    adjustForPageBounds(coverRect);
    adjustForPageBounds(keepRect);
}
    
void TiledSurface::doLayoutTiles()
{
    if (isTileCreationSuspended())
        return;
    MutexLocker locker(m_tileMutex);

    if (checkDoSingleTileLayout())
        return;
    
    IntRect visibleRect = this->visibleRect();
    if (visibleRect.isEmpty()) {
        // Visible rect may become temporarily empty in some cases. Just keep the existing tiles.
        coverWithTiles(IntRect(), frame());
        return;
    }
    
    IntRect targetCoverRect;
    IntRect keepRect;
    bool centerGrid;
    calculateCoverAndKeepRectForMemoryLevel(visibleRect, targetCoverRect, keepRect, centerGrid);

    IntRect coverRect;
    if (!tilesCover(visibleRect)) {
        // If the visible area is not covered try to fix that as fast as possible and add surrounding
        // tiles with another layout.
        coverRect = visibleRect;
    } else
        coverRect = targetCoverRect;
    
    if (centerGrid)
        centerTileGridOrigin(visibleRect);

    coverWithTiles(coverRect, keepRect);
}

void TiledSurface::layoutTiles()
{
    // If the view has shrunk, drop any unneeded tiles synchronously
    IntRect frame = this->frame();
    IntRect tiledArea = unionRect(m_previousCoverRect, m_previousKeepRect);
    if (tiledArea.bottom() > frame.bottom() || tiledArea.right() > frame.right()) {
        MutexLocker locker(m_tileMutex);
        dropTilesOutsideRect(frame);
    }

    // Forward the call to the web thread for asynchronous tile creation and painting
    [m_webThreadCaller.get() doLayoutTiles];
}
    
void TiledSurface::layoutTilesNow()
{
    ASSERT(WebThreadIsLockedOrDisabled());
    MutexLocker locker(m_tileMutex);
    
    if (checkDoSingleTileLayout())
        return;
    IntRect visibleRect = this->visibleRect();
    if (visibleRect.isEmpty())
        return;
    IntRect unusedRect;
    IntRect keepRect;
    bool unusedBool;
    calculateCoverAndKeepRectForMemoryLevel(visibleRect, unusedRect, keepRect, unusedBool);
    keepRect.intersect(frame());

    coverWithTiles(visibleRect, keepRect);
}

void TiledSurface::removeAllNonVisibleTiles()
{
    MutexLocker locker(m_tileMutex);
    
    IntSize size = this->size();
    if (size.width() <= m_tileSize.width() && size.height() <= m_tileSize.height()) {
        dropTilesOutsideRect(IntRect(IntPoint(), size));
        return;
    }

    dropTilesOutsideRect(visibleRect());
}

void TiledSurface::removeAllTiles()
{
    MutexLocker locker(m_tileMutex);
    coverWithTiles(IntRect(), IntRect());
}

void TiledSurface::shrinkToMinimalTiles()
{
    if (checkDoSingleTileLayout())
        return;
    IntRect visibleRect = this->visibleRect();
    centerTileGridOrigin(visibleRect);
    coverWithTiles(visibleRect, visibleRect);
}
    
void TiledSurface::coverWithTiles(const IntRect& coverRect, const IntRect& keepRect)
{
    // Tile mutex must be held when calling this
    ASSERT(!m_tileMutex.tryLock());
    
    IntRect rectToIterate = unionRect(coverRect, keepRect);
    rectToIterate.unite(m_previousCoverRect);
    rectToIterate.unite(m_previousKeepRect);
    
    unsigned minXIndex;
    unsigned minYIndex;
    tileIndexForPoint(IntPoint(rectToIterate.x(), rectToIterate.y()), minXIndex, minYIndex);
    
    unsigned maxXIndex;
    unsigned maxYIndex;
    tileIndexForPoint(IntPoint(rectToIterate.right(), rectToIterate.bottom()), maxXIndex, maxYIndex);

    bool tileAddedOrChanged = false;
    m_tileGrid.resize(maxYIndex + 1);
    for (unsigned yIndex = minYIndex; yIndex <= maxYIndex; ++yIndex) {
        m_tileGrid[yIndex].resize(maxXIndex + 1);
        for (unsigned xIndex = minXIndex; xIndex <= maxXIndex; ++xIndex) {
            IntRect tileRect = tileRectForIndex(xIndex, yIndex);
            RefPtr<Tile>& tile = m_tileGrid[yIndex][xIndex];
            if (tile && (!tileRect.intersects(keepRect) || tile->rect() != tileRect))
                tile = 0;
            if (!tile && tileRect.intersects(coverRect)) {
                tile = Tile::create(this, tileRect);
                tileAddedOrChanged = true;
            }
        }
    }

    m_previousCoverRect = coverRect;
    m_previousKeepRect = keepRect;
    
    // If we created a new tile we need to ensure that all tiles are showing the same version of the content.
    if (tileAddedOrChanged && !m_savedDisplayRects.isEmpty())
        flushSavedDisplayRects();
}

void TiledSurface::dropTilesOutsideRect(const IntRect& keepRect)
{
    IntRect rectToIterate = unionRect(m_previousCoverRect, m_previousKeepRect);
    
    unsigned minXIndex;
    unsigned minYIndex;
    tileIndexForPoint(rectToIterate.topLeft(), minXIndex, minYIndex);
    unsigned maxXIndex;
    unsigned maxYIndex;
    tileIndexForPoint(rectToIterate.bottomRight(), maxXIndex, maxYIndex);
    
    for (unsigned yIndex = minYIndex; yIndex <= maxYIndex; ++yIndex) {
        for (unsigned xIndex = minXIndex; xIndex <= maxXIndex; ++xIndex) {
            Tile* tile = tileForIndex(xIndex, yIndex);
            if (tile && !tile->rect().intersects(keepRect))
                m_tileGrid[yIndex][xIndex] = 0;
        }
    }
}

static bool shouldRepaintInPieces(const CGRect& dirtyRect, CGSRegionObj dirtyRegion)
{
    // Estimate whether or not we should use the unioned rect or the individual rects.
    // We do this by computing the percentage of "wasted space" in the union. If that wasted space
    // is too large, then we will do individual rect painting instead.
    float singlePixels = 0;
    unsigned rectCount = 0;
    
    CGSRegionEnumeratorObj enumerator = CGSRegionEnumerator(dirtyRegion);
    CGRect *subRect;
    while ((subRect = CGSNextRect(enumerator))) {
        ++rectCount;
        singlePixels += subRect->size.width * subRect->size.height;
    }
    CGSReleaseRegionEnumerator(enumerator);
    
    const unsigned cRectThreshold = 10;
    if (rectCount < 2 || rectCount > cRectThreshold)
        return false;
    
    const float cWastedSpaceThreshold = 0.50f;
    float unionPixels = dirtyRect.size.width * dirtyRect.size.height;
    float wastedSpace = 1.f - (singlePixels / unionPixels);
    return wastedSpace > cWastedSpaceThreshold;
}

void TiledSurface::drawLayer(CALayer* layer, CGContextRef context)
{
    // The web lock unlock observer runs after CA commit observer.
    if (!WebThreadIsLockedOrDisabled()) {
        LOG_ERROR("Drawing without holding the web thread lock");
        ASSERT_NOT_REACHED();
    }

    WKSetCurrentGraphicsContext(context);
    
    CGContextSetShouldAntialias(context, NO);
    
    WKFontAntialiasingStateSaver fontAntialiasingState([m_window useOrientationDependentFontAntialiasing]);
    fontAntialiasingState.setup([WAKWindow hasLandscapeOrientation]);
    
    CGRect dirtyRect = CGContextGetClipBoundingBox(context);
    CGRect frame = [layer frame];
    CGContextTranslateCTM(context, -frame.origin.x, -frame.origin.y);
    
    CGSRegionObj drawRegion = (CGSRegionObj)[layer regionBeingDrawn];
    if (drawRegion && shouldRepaintInPieces(dirtyRect, drawRegion)) {
        // Use fine grained repaint rectangles to minimize the amount of painted pixels.
        CGSRegionEnumeratorObj enumerator = CGSRegionEnumerator(drawRegion);
        CGRect *subRect;
        while ((subRect = CGSNextRect(enumerator))) {
            CGRect flippedSubRect = *subRect;
            flippedSubRect.origin.y = frame.size.height - flippedSubRect.origin.y - flippedSubRect.size.height;
            CGRect subRectInSuper = [m_hostCALayer.get() convertRect:flippedSubRect fromLayer:layer];
            WKWindowDrawRect([m_window _windowRef], subRectInSuper);
        }
        CGSReleaseRegionEnumerator(enumerator);
    } else {
        // Simple repaint
        CGRect dirtyRectInSuper = [m_hostCALayer.get() convertRect:dirtyRect fromLayer:layer];
        WKWindowDrawRect([m_window _windowRef], dirtyRectInSuper);
    }
    
    fontAntialiasingState.restore();

    WAKView* view = [m_window contentView];
    if ([view respondsToSelector:@selector(_dispatchTileDidDraw:)])
        [view _dispatchTileDidDraw:layer];
}

void TiledSurface::setNeedsDisplay()
{
    setNeedsDisplayInRect(IntRect(0, 0, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));
}

void TiledSurface::setNeedsDisplayInRect(const IntRect& dirtyRect)
{
    {
        MutexLocker locker(m_savedDisplayRectMutex);
        if (isPaintingSuspended() || !m_savedDisplayRects.isEmpty()) {
            bool addedFirstRect = m_savedDisplayRects.isEmpty();
            m_savedDisplayRects.append(dirtyRect);
            // Invalidate the host layer layout as a way of signaling that we have new content. It might
            // be better to have a specific callback but this is simpler.
            if (addedFirstRect)
                [m_hostCALayer.get() performSelectorOnMainThread:@selector(setNeedsLayout) withObject:nil waitUntilDone:NO];
            return;
        }
    }
    
    MutexLocker locker(m_tileMutex);
    invalidateTiles(dirtyRect);
}
    
void TiledSurface::invalidateTiles(const IntRect& dirtyRect)
{
    ASSERT(!m_tileMutex.tryLock());

    IntRect coveredRect = unionRect(m_previousCoverRect, m_previousKeepRect);
    IntRect rectToIterate = intersection(dirtyRect, coveredRect);
    
    unsigned minXIndex;
    unsigned minYIndex;
    tileIndexForPoint(rectToIterate.topLeft(), minXIndex, minYIndex);
    unsigned maxXIndex;
    unsigned maxYIndex;
    tileIndexForPoint(rectToIterate.bottomRight(), maxXIndex, maxYIndex);
    
    for (unsigned yIndex = minYIndex; yIndex <= maxYIndex; ++yIndex) {
        for (unsigned xIndex = minXIndex; xIndex <= maxXIndex; ++xIndex) {
            Tile* tile = tileForIndex(xIndex, yIndex);
            if (!tile)
                continue;
            tile->invalidateRect(dirtyRect);
        }
    }
}
    
bool TiledSurface::isTileCreationSuspended() const 
{ 
    return m_tilingMode == Zooming || m_tilingMode == Disabled;
}

bool TiledSurface::isPaintingSuspended() const 
{ 
    return m_tilingMode == Zooming || m_tilingMode == Panning || m_tilingMode == Disabled; 
}

TiledSurface::TilingMode TiledSurface::tilingMode() const
{
    return m_tilingMode;
}

void TiledSurface::updateTilingMode()
{
    ASSERT(WebThreadIsCurrent() || !WebThreadIsEnabled());

    WAKView* view = [m_window contentView];
    if (m_tilingMode == Zooming || m_tilingMode == Panning) {
        if (!m_didCallWillStartScrollingOrZooming && [view respondsToSelector:@selector(_willStartScrollingOrZooming)]) {
            [view _willStartScrollingOrZooming];
            m_didCallWillStartScrollingOrZooming = true;
        }
    } else {
        if (m_didCallWillStartScrollingOrZooming && [view respondsToSelector:@selector(_didFinishScrollingOrZooming)]) {
            [view _didFinishScrollingOrZooming];
            m_didCallWillStartScrollingOrZooming = false;
        }
        if (m_tilingMode == Disabled)
            return;
        layoutTiles();
        if (!m_savedDisplayRects.isEmpty()) {
            MutexLocker locker(m_tileMutex);
            flushSavedDisplayRects();
        }
    }
}

void TiledSurface::setTilingMode(TilingMode tilingMode)
{
    if (tilingMode == m_tilingMode)
        return;
    m_tilingMode = tilingMode;
    [m_webThreadCaller.get() updateTilingMode];
}

void TiledSurface::flushSavedDisplayRects()
{
    ASSERT(!m_tileMutex.tryLock());

    Vector<IntRect> rects;
    {
        MutexLocker locker(m_savedDisplayRectMutex);
        m_savedDisplayRects.swap(rects);
    }
    size_t size = rects.size();
    for (size_t n = 0; n < size; ++n)
        invalidateTiles(rects[n]);
}
    
bool TiledSurface::hasPendingDraw() const
{
    return !m_savedDisplayRects.isEmpty();
}

void TiledSurface::prepareToDraw()
{
    // This will trigger document relayout if needed.
    [[m_window contentView] viewWillDraw];

    if (!m_savedDisplayRects.isEmpty()) {
        MutexLocker locker(m_tileMutex);
        flushSavedDisplayRects();
    }
}

}
