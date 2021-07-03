/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ViewGestureController.h"

#if PLATFORM(MAC)

#import "FrameLoadState.h"
#import "NativeWebWheelEvent.h"
#import "ViewGestureControllerMessages.h"
#import "ViewGestureGeometryCollectorMessages.h"
#import "ViewSnapshotStore.h"
#import "WebBackForwardList.h"
#import "WebPageGroup.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessProxy.h"
#import <Cocoa/Cocoa.h>
#import <WebCore/IOSurface.h>
#import <WebCore/NSEventSPI.h>
#import <WebCore/QuartzCoreSPI.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>

using namespace WebCore;

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101000
#define ENABLE_LEGACY_SWIPE_SHADOW_STYLE 1
#else
#define ENABLE_LEGACY_SWIPE_SHADOW_STYLE 0
#endif

static const double minMagnification = 1;
static const double maxMagnification = 3;

static const double minElasticMagnification = 0.75;
static const double maxElasticMagnification = 4;

static const double zoomOutBoost = 1.6;
static const double zoomOutResistance = 0.10;

static const float smartMagnificationElementPadding = 0.05;
static const float smartMagnificationPanScrollThreshold = 100;

#if ENABLE(LEGACY_SWIPE_SHADOW_STYLE)
static const double swipeOverlayShadowOpacity = 0.66;
static const double swipeOverlayShadowRadius = 3;
#else
static const double swipeOverlayShadowOpacity = 0.06;
static const double swipeOverlayDimmingOpacity = 0.12;
static const CGFloat swipeOverlayShadowWidth = 81;
#endif

static const CGFloat minimumHorizontalSwipeDistance = 15;
static const float minimumScrollEventRatioForSwipe = 0.5;

static const float swipeSnapshotRemovalRenderTreeSizeTargetFraction = 0.5;
static const std::chrono::seconds swipeSnapshotRemovalWatchdogDuration = 5_s;
static const std::chrono::seconds swipeSnapshotRemovalWatchdogAfterFirstVisuallyNonEmptyLayoutDuration = 3_s;
static const std::chrono::milliseconds swipeSnapshotRemovalActiveLoadMonitoringInterval = 250_ms;

@interface WKSwipeCancellationTracker : NSObject {
@private
    BOOL _isCancelled;
}

@property (nonatomic) BOOL isCancelled;

@end

@implementation WKSwipeCancellationTracker
@synthesize isCancelled=_isCancelled;
@end

namespace WebKit {

ViewGestureController::ViewGestureController(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_swipeWatchdogTimer(RunLoop::main(), this, &ViewGestureController::swipeSnapshotWatchdogTimerFired)
    , m_swipeActiveLoadMonitoringTimer(RunLoop::main(), this, &ViewGestureController::activeLoadMonitoringTimerFired)
    , m_swipeWatchdogAfterFirstVisuallyNonEmptyLayoutTimer(RunLoop::main(), this, &ViewGestureController::swipeSnapshotWatchdogTimerFired)
{
    m_webPageProxy.process().addMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.pageID(), *this);
}

ViewGestureController::~ViewGestureController()
{
    if (m_swipeCancellationTracker)
        [m_swipeCancellationTracker setIsCancelled:YES];

    if (m_activeGestureType == ViewGestureType::Swipe)
        removeSwipeSnapshot();

    if (m_didMoveSwipeSnapshotCallback) {
        Block_release(m_didMoveSwipeSnapshotCallback);
        m_didMoveSwipeSnapshotCallback = nullptr;
    }

    m_webPageProxy.process().removeMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.pageID());
}

static double resistanceForDelta(double deltaScale, double currentScale)
{
    // Zoom out with slight acceleration, until we reach minimum scale.
    if (deltaScale < 0 && currentScale > minMagnification)
        return zoomOutBoost;

    // Zoom in with no acceleration, until we reach maximum scale.
    if (deltaScale > 0 && currentScale < maxMagnification)
        return 1;

    // Outside of the extremes, resist further scaling.
    double limit = currentScale < minMagnification ? minMagnification : maxMagnification;
    double scaleDistance = fabs(limit - currentScale);
    double scalePercent = std::min(std::max(scaleDistance / limit, 0.), 1.);
    double resistance = zoomOutResistance + scalePercent * (0.01 - zoomOutResistance);

    return resistance;
}

FloatPoint ViewGestureController::scaledMagnificationOrigin(FloatPoint origin, double scale)
{
    FloatPoint scaledMagnificationOrigin(origin);
    scaledMagnificationOrigin.moveBy(m_visibleContentRect.location());
    float magnificationOriginScale = 1 - (scale / m_webPageProxy.pageScaleFactor());
    scaledMagnificationOrigin.scale(magnificationOriginScale, magnificationOriginScale);
    return scaledMagnificationOrigin;
}

void ViewGestureController::didCollectGeometryForMagnificationGesture(FloatRect visibleContentRect, bool frameHandlesMagnificationGesture)
{
    m_activeGestureType = ViewGestureType::Magnification;
    m_visibleContentRect = visibleContentRect;
    m_visibleContentRectIsValid = true;
    m_frameHandlesMagnificationGesture = frameHandlesMagnificationGesture;
}

void ViewGestureController::handleMagnificationGesture(double scale, FloatPoint origin)
{
    ASSERT(m_activeGestureType == ViewGestureType::None || m_activeGestureType == ViewGestureType::Magnification);

    if (m_activeGestureType == ViewGestureType::None) {
        // FIXME: We drop the first frame of the gesture on the floor, because we don't have the visible content bounds yet.
        m_magnification = m_webPageProxy.pageScaleFactor();
        m_webPageProxy.process().send(Messages::ViewGestureGeometryCollector::CollectGeometryForMagnificationGesture(), m_webPageProxy.pageID());
        m_lastMagnificationGestureWasSmartMagnification = false;

        return;
    }

    // We're still waiting for the DidCollectGeometry callback.
    if (!m_visibleContentRectIsValid)
        return;

    m_activeGestureType = ViewGestureType::Magnification;

    double scaleWithResistance = resistanceForDelta(scale, m_magnification) * scale;

    m_magnification += m_magnification * scaleWithResistance;
    m_magnification = std::min(std::max(m_magnification, minElasticMagnification), maxElasticMagnification);

    m_magnificationOrigin = origin;

    if (m_frameHandlesMagnificationGesture)
        m_webPageProxy.scalePage(m_magnification, roundedIntPoint(origin));
    else
        m_webPageProxy.drawingArea()->adjustTransientZoom(m_magnification, scaledMagnificationOrigin(origin, m_magnification));
}

void ViewGestureController::endMagnificationGesture()
{
    if (m_activeGestureType != ViewGestureType::Magnification)
        return;

    double newMagnification = std::min(std::max(m_magnification, minMagnification), maxMagnification);

    if (m_frameHandlesMagnificationGesture)
        m_webPageProxy.scalePage(newMagnification, roundedIntPoint(m_magnificationOrigin));
    else {
        if (auto drawingArea = m_webPageProxy.drawingArea())
            drawingArea->commitTransientZoom(newMagnification, scaledMagnificationOrigin(m_magnificationOrigin, newMagnification));
    }

    m_activeGestureType = ViewGestureType::None;
    m_visibleContentRectIsValid = false;
}

void ViewGestureController::handleSmartMagnificationGesture(FloatPoint origin)
{
    if (m_activeGestureType != ViewGestureType::None)
        return;

    m_webPageProxy.process().send(Messages::ViewGestureGeometryCollector::CollectGeometryForSmartMagnificationGesture(origin), m_webPageProxy.pageID());
}

static float maximumRectangleComponentDelta(FloatRect a, FloatRect b)
{
    return std::max(fabs(a.x() - b.x()), std::max(fabs(a.y() - b.y()), std::max(fabs(a.width() - b.width()), fabs(a.height() - b.height()))));
}

void ViewGestureController::didCollectGeometryForSmartMagnificationGesture(FloatPoint origin, FloatRect renderRect, FloatRect visibleContentRect, bool isReplacedElement, double viewportMinimumScale, double viewportMaximumScale)
{
    double currentScaleFactor = m_webPageProxy.pageScaleFactor();

    FloatRect unscaledTargetRect = renderRect;

    // If there was no usable element under the cursor, we'll scale towards the cursor instead.
    if (unscaledTargetRect.isEmpty())
        unscaledTargetRect.setLocation(origin);

    unscaledTargetRect.scale(1 / currentScaleFactor);
    unscaledTargetRect.inflateX(unscaledTargetRect.width() * smartMagnificationElementPadding);
    unscaledTargetRect.inflateY(unscaledTargetRect.height() * smartMagnificationElementPadding);

    double targetMagnification = visibleContentRect.width() / unscaledTargetRect.width();

    FloatRect unscaledVisibleContentRect = visibleContentRect;
    unscaledVisibleContentRect.scale(1 / currentScaleFactor);
    FloatRect viewportConstrainedUnscaledTargetRect = unscaledTargetRect;
    viewportConstrainedUnscaledTargetRect.intersect(unscaledVisibleContentRect);

    if (unscaledTargetRect.width() > viewportConstrainedUnscaledTargetRect.width())
        viewportConstrainedUnscaledTargetRect.setX(unscaledVisibleContentRect.x() + (origin.x() / currentScaleFactor) - viewportConstrainedUnscaledTargetRect.width() / 2);
    if (unscaledTargetRect.height() > viewportConstrainedUnscaledTargetRect.height())
        viewportConstrainedUnscaledTargetRect.setY(unscaledVisibleContentRect.y() + (origin.y() / currentScaleFactor) - viewportConstrainedUnscaledTargetRect.height() / 2);

    // For replaced elements like images, we want to fit the whole element
    // in the view, so scale it down enough to make both dimensions fit if possible.
    if (isReplacedElement)
        targetMagnification = std::min(targetMagnification, static_cast<double>(visibleContentRect.height() / viewportConstrainedUnscaledTargetRect.height()));

    targetMagnification = std::min(std::max(targetMagnification, minMagnification), maxMagnification);

    // Allow panning between elements via double-tap while magnified, unless the target rect is
    // similar to the last one or the mouse has not moved, in which case we'll zoom all the way out.
    if (currentScaleFactor > 1 && m_lastMagnificationGestureWasSmartMagnification) {
        if (maximumRectangleComponentDelta(m_lastSmartMagnificationUnscaledTargetRect, unscaledTargetRect) < smartMagnificationPanScrollThreshold)
            targetMagnification = 1;

        if (m_lastSmartMagnificationOrigin == origin)
            targetMagnification = 1;
    }

    FloatRect targetRect(viewportConstrainedUnscaledTargetRect);
    targetRect.scale(targetMagnification);
    FloatPoint targetOrigin(visibleContentRect.center());
    targetOrigin.moveBy(-targetRect.center());

    m_webPageProxy.drawingArea()->adjustTransientZoom(m_webPageProxy.pageScaleFactor(), scaledMagnificationOrigin(FloatPoint(), m_webPageProxy.pageScaleFactor()));
    m_webPageProxy.drawingArea()->commitTransientZoom(targetMagnification, targetOrigin);

    m_lastSmartMagnificationUnscaledTargetRect = unscaledTargetRect;
    m_lastSmartMagnificationOrigin = origin;

    m_lastMagnificationGestureWasSmartMagnification = true;
}

bool ViewGestureController::scrollEventCanBecomeSwipe(NSEvent *event, ViewGestureController::SwipeDirection& potentialSwipeDirection)
{
    if (event.phase != NSEventPhaseBegan)
        return false;

    if (!event.hasPreciseScrollingDeltas)
        return false;

    if (![NSEvent isSwipeTrackingFromScrollEventsEnabled])
        return false;

    if (fabs(event.scrollingDeltaX) <= fabs(event.scrollingDeltaY))
        return false;

    bool isPinnedToLeft = m_shouldIgnorePinnedState || m_webPageProxy.isPinnedToLeftSide();
    bool isPinnedToRight = m_shouldIgnorePinnedState || m_webPageProxy.isPinnedToRightSide();

    bool willSwipeLeft = event.scrollingDeltaX > 0 && isPinnedToLeft && m_webPageProxy.backForwardList().backItem();
    bool willSwipeRight = event.scrollingDeltaX < 0 && isPinnedToRight && m_webPageProxy.backForwardList().forwardItem();
    if (!willSwipeLeft && !willSwipeRight)
        return false;

    potentialSwipeDirection = willSwipeLeft ? ViewGestureController::SwipeDirection::Back : ViewGestureController::SwipeDirection::Forward;

    return true;
}

bool ViewGestureController::deltaIsSufficientToBeginSwipe(NSEvent *event)
{
    if (m_pendingSwipeReason != PendingSwipeReason::InsufficientMagnitude)
        return false;

    m_cumulativeDeltaForPendingSwipe += FloatSize(event.scrollingDeltaX, event.scrollingDeltaY);

    // If the cumulative delta is ever "too vertical", we will stop tracking this
    // as a potential swipe until we get another "begin" event.
    if (fabs(m_cumulativeDeltaForPendingSwipe.height()) >= fabs(m_cumulativeDeltaForPendingSwipe.width()) * minimumScrollEventRatioForSwipe) {
        m_pendingSwipeReason = PendingSwipeReason::None;
        return false;
    }

    if (fabs(m_cumulativeDeltaForPendingSwipe.width()) < minimumHorizontalSwipeDistance)
        return false;

    return true;
}

void ViewGestureController::setDidMoveSwipeSnapshotCallback(void(^callback)(CGRect))
{
    if (m_didMoveSwipeSnapshotCallback)
        Block_release(m_didMoveSwipeSnapshotCallback);
    m_didMoveSwipeSnapshotCallback = Block_copy(callback);
}

bool ViewGestureController::handleScrollWheelEvent(NSEvent *event)
{
    if (event.phase == NSEventPhaseEnded) {
        m_cumulativeDeltaForPendingSwipe = FloatSize();
        m_pendingSwipeReason = PendingSwipeReason::None;
    }

    if (m_pendingSwipeReason == PendingSwipeReason::InsufficientMagnitude) {
        if (deltaIsSufficientToBeginSwipe(event)) {
            trackSwipeGesture(event, m_pendingSwipeDirection);
            return true;
        }
    }

    if (m_activeGestureType != ViewGestureType::None)
        return false;

    SwipeDirection direction;
    if (!scrollEventCanBecomeSwipe(event, direction))
        return false;

    if (!m_shouldIgnorePinnedState && m_webPageProxy.willHandleHorizontalScrollEvents()) {
        m_pendingSwipeReason = PendingSwipeReason::WebCoreMayScroll;
        m_pendingSwipeDirection = direction;
        return false;
    }

    if (!deltaIsSufficientToBeginSwipe(event)) {
        m_pendingSwipeReason = PendingSwipeReason::InsufficientMagnitude;
        m_pendingSwipeDirection = direction;
        return true;
    }

    trackSwipeGesture(event, direction);

    return true;
}

void ViewGestureController::wheelEventWasNotHandledByWebCore(NSEvent *event)
{
    if (m_pendingSwipeReason != PendingSwipeReason::WebCoreMayScroll)
        return;

    m_pendingSwipeReason = PendingSwipeReason::None;

    SwipeDirection direction;
    if (!scrollEventCanBecomeSwipe(event, direction))
        return;

    if (!deltaIsSufficientToBeginSwipe(event)) {
        m_pendingSwipeReason = PendingSwipeReason::InsufficientMagnitude;
        return;
    }

    trackSwipeGesture(event, m_pendingSwipeDirection);
}

void ViewGestureController::trackSwipeGesture(NSEvent *event, SwipeDirection direction)
{
    ASSERT(m_activeGestureType == ViewGestureType::None);
    m_pendingSwipeReason = PendingSwipeReason::None;

    m_webPageProxy.recordNavigationSnapshot();

    CGFloat maxProgress = (direction == SwipeDirection::Back) ? 1 : 0;
    CGFloat minProgress = (direction == SwipeDirection::Forward) ? -1 : 0;
    RefPtr<WebBackForwardListItem> targetItem = (direction == SwipeDirection::Back) ? m_webPageProxy.backForwardList().backItem() : m_webPageProxy.backForwardList().forwardItem();
    if (!targetItem)
        return;
    
    __block bool swipeCancelled = false;

    ASSERT(!m_swipeCancellationTracker);
    RetainPtr<WKSwipeCancellationTracker> swipeCancellationTracker = adoptNS([[WKSwipeCancellationTracker alloc] init]);
    m_swipeCancellationTracker = swipeCancellationTracker;

    [event trackSwipeEventWithOptions:NSEventSwipeTrackingConsumeMouseEvents dampenAmountThresholdMin:minProgress max:maxProgress usingHandler:^(CGFloat progress, NSEventPhase phase, BOOL isComplete, BOOL *stop) {
        if ([swipeCancellationTracker isCancelled]) {
            *stop = YES;
            return;
        }
        if (phase == NSEventPhaseBegan)
            this->beginSwipeGesture(targetItem.get(), direction);
        CGFloat clampedProgress = std::min(std::max(progress, minProgress), maxProgress);
        this->handleSwipeGesture(targetItem.get(), clampedProgress, direction);
        if (phase == NSEventPhaseCancelled)
            swipeCancelled = true;
        if (phase == NSEventPhaseEnded || phase == NSEventPhaseCancelled)
            this->willEndSwipeGesture(*targetItem, swipeCancelled);
        if (isComplete)
            this->endSwipeGesture(targetItem.get(), swipeCancelled);
    }];
}

void ViewGestureController::willEndSwipeGesture(WebBackForwardListItem& targetItem, bool cancelled)
{
    m_webPageProxy.navigationGestureWillEnd(!cancelled, targetItem);
}

FloatRect ViewGestureController::windowRelativeBoundsForCustomSwipeViews() const
{
    FloatRect swipeArea;
    for (const auto& view : m_customSwipeViews)
        swipeArea.unite([view convertRect:[view bounds] toView:nil]);
    swipeArea.setHeight(swipeArea.height() - m_customSwipeViewsTopContentInset);
    return swipeArea;
}

static CALayer *leastCommonAncestorLayer(const Vector<RetainPtr<CALayer>>& layers)
{
    Vector<Vector<CALayer *>> liveLayerPathsFromRoot(layers.size());

    size_t shortestPathLength = std::numeric_limits<size_t>::max();

    for (size_t layerIndex = 0; layerIndex < layers.size(); layerIndex++) {
        CALayer *parent = [layers[layerIndex] superlayer];
        while (parent) {
            liveLayerPathsFromRoot[layerIndex].insert(0, parent);
            shortestPathLength = std::min(shortestPathLength, liveLayerPathsFromRoot[layerIndex].size());
            parent = parent.superlayer;
        }
    }

    for (size_t pathIndex = 0; pathIndex < shortestPathLength; pathIndex++) {
        CALayer *firstPathLayer = liveLayerPathsFromRoot[0][pathIndex];
        for (size_t layerIndex = 1; layerIndex < layers.size(); layerIndex++) {
            if (liveLayerPathsFromRoot[layerIndex][pathIndex] != firstPathLayer)
                return firstPathLayer;
        }
    }

    return liveLayerPathsFromRoot[0][shortestPathLength];
}

CALayer *ViewGestureController::determineSnapshotLayerParent() const
{
    if (m_currentSwipeLiveLayers.size() == 1)
        return [m_currentSwipeLiveLayers[0] superlayer];

    // We insert our snapshot into the first shared superlayer of the custom views' layer, above the frontmost or below the bottommost layer.
    return leastCommonAncestorLayer(m_currentSwipeLiveLayers);
}

CALayer *ViewGestureController::determineLayerAdjacentToSnapshotForParent(SwipeDirection direction, CALayer *snapshotLayerParent) const
{
    // If we have custom swiping views, we assume that the views were passed to us in back-to-front z-order.
    CALayer *layerAdjacentToSnapshot = direction == SwipeDirection::Back ? m_currentSwipeLiveLayers.first().get() : m_currentSwipeLiveLayers.last().get();

    if (m_currentSwipeLiveLayers.size() == 1)
        return layerAdjacentToSnapshot;

    // If the layers are not all siblings, find the child of the layer we're going to insert the snapshot into which has the frontmost/bottommost layer as a child.
    while (snapshotLayerParent != layerAdjacentToSnapshot.superlayer)
        layerAdjacentToSnapshot = layerAdjacentToSnapshot.superlayer;
    return layerAdjacentToSnapshot;
}

bool ViewGestureController::shouldUseSnapshotForSize(ViewSnapshot& snapshot, FloatSize swipeLayerSize, float topContentInset)
{
    float deviceScaleFactor = m_webPageProxy.deviceScaleFactor();
    if (snapshot.deviceScaleFactor() != deviceScaleFactor)
        return false;

    FloatSize unobscuredSwipeLayerSizeInDeviceCoordinates = swipeLayerSize - FloatSize(0, topContentInset);
    unobscuredSwipeLayerSizeInDeviceCoordinates.scale(deviceScaleFactor);
    if (snapshot.size() != unobscuredSwipeLayerSizeInDeviceCoordinates)
        return false;

    return true;
}

static bool layerGeometryFlippedToRoot(CALayer *layer)
{
    bool flipped = false;
    CALayer *parent = layer;
    while (parent) {
        if (parent.isGeometryFlipped)
            flipped = !flipped;
        parent = parent.superlayer;
    }
    return flipped;
}

void ViewGestureController::applyDebuggingPropertiesToSwipeViews()
{
    CAFilter* filter = [CAFilter filterWithType:kCAFilterColorInvert];
    [m_swipeLayer setFilters:@[ filter ]];
    [m_swipeLayer setBackgroundColor:[NSColor blueColor].CGColor];
    [m_swipeLayer setBorderColor:[NSColor yellowColor].CGColor];
    [m_swipeLayer setBorderWidth:4];

    [m_swipeSnapshotLayer setBackgroundColor:[NSColor greenColor].CGColor];
    [m_swipeSnapshotLayer setBorderColor:[NSColor redColor].CGColor];
    [m_swipeSnapshotLayer setBorderWidth:2];
}

void ViewGestureController::beginSwipeGesture(WebBackForwardListItem* targetItem, SwipeDirection direction)
{
    ASSERT(m_currentSwipeLiveLayers.isEmpty());

    m_webPageProxy.navigationGestureDidBegin();

    m_activeGestureType = ViewGestureType::Swipe;
    m_swipeInProgress = true;

    CALayer *rootContentLayer = m_webPageProxy.acceleratedCompositingRootLayer();

    m_swipeLayer = adoptNS([[CALayer alloc] init]);
    m_swipeSnapshotLayer = adoptNS([[CALayer alloc] init]);
    m_currentSwipeCustomViewBounds = windowRelativeBoundsForCustomSwipeViews();

    FloatRect swipeArea;
    float topContentInset = 0;
    if (!m_customSwipeViews.isEmpty()) {
        topContentInset = m_customSwipeViewsTopContentInset;
        swipeArea = m_currentSwipeCustomViewBounds;
        swipeArea.expand(0, topContentInset);

        for (const auto& view : m_customSwipeViews) {
            CALayer *layer = [view layer];
            ASSERT(layer);
            m_currentSwipeLiveLayers.append(layer);
        }
    } else {
        swipeArea = [rootContentLayer convertRect:CGRectMake(0, 0, m_webPageProxy.viewSize().width(), m_webPageProxy.viewSize().height()) toLayer:nil];
        topContentInset = m_webPageProxy.topContentInset();
        m_currentSwipeLiveLayers.append(rootContentLayer);
    }

    CALayer *snapshotLayerParent = determineSnapshotLayerParent();
    bool geometryIsFlippedToRoot = layerGeometryFlippedToRoot(snapshotLayerParent);

    RetainPtr<CGColorRef> backgroundColor = CGColorGetConstantColor(kCGColorWhite);
    if (ViewSnapshot* snapshot = targetItem->snapshot()) {
        if (shouldUseSnapshotForSize(*snapshot, swipeArea.size(), topContentInset))
            [m_swipeSnapshotLayer setContents:snapshot->asLayerContents()];

        Color coreColor = snapshot->backgroundColor();
        if (coreColor.isValid())
            backgroundColor = cachedCGColor(coreColor, ColorSpaceDeviceRGB);
#if USE_IOSURFACE_VIEW_SNAPSHOTS
        m_currentSwipeSnapshot = snapshot;
#endif
    }

    [m_swipeLayer setBackgroundColor:backgroundColor.get()];
    [m_swipeLayer setAnchorPoint:CGPointZero];
    [m_swipeLayer setFrame:[snapshotLayerParent convertRect:swipeArea fromLayer:nil]];
    [m_swipeLayer setName:@"Gesture Swipe Root Layer"];
    [m_swipeLayer setGeometryFlipped:geometryIsFlippedToRoot];
    [m_swipeLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];

    float deviceScaleFactor = m_webPageProxy.deviceScaleFactor();
    [m_swipeSnapshotLayer setContentsGravity:kCAGravityTopLeft];
    [m_swipeSnapshotLayer setContentsScale:deviceScaleFactor];
    [m_swipeSnapshotLayer setAnchorPoint:CGPointZero];
    [m_swipeSnapshotLayer setFrame:CGRectMake(0, 0, swipeArea.width(), swipeArea.height() - topContentInset)];
    [m_swipeSnapshotLayer setName:@"Gesture Swipe Snapshot Layer"];
    [m_swipeSnapshotLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];

    [m_swipeLayer addSublayer:m_swipeSnapshotLayer.get()];

    if (m_webPageProxy.preferences().viewGestureDebuggingEnabled())
        applyDebuggingPropertiesToSwipeViews();

    CALayer *layerAdjacentToSnapshot = determineLayerAdjacentToSnapshotForParent(direction, snapshotLayerParent);
    if (direction == SwipeDirection::Back)
        [snapshotLayerParent insertSublayer:m_swipeLayer.get() below:layerAdjacentToSnapshot];
    else
        [snapshotLayerParent insertSublayer:m_swipeLayer.get() above:layerAdjacentToSnapshot];

    // We don't know enough about the custom views' hierarchy to apply a shadow.
    if (m_swipeTransitionStyle == SwipeTransitionStyle::Overlap && m_customSwipeViews.isEmpty()) {
#if ENABLE(LEGACY_SWIPE_SHADOW_STYLE)
        if (direction == SwipeDirection::Back) {
            float topContentInset = m_webPageProxy.topContentInset();
            FloatRect shadowRect(FloatPoint(0, topContentInset), m_webPageProxy.viewSize() - FloatSize(0, topContentInset));
            RetainPtr<CGPathRef> shadowPath = adoptCF(CGPathCreateWithRect(shadowRect, 0));
            [rootContentLayer setShadowColor:CGColorGetConstantColor(kCGColorBlack)];
            [rootContentLayer setShadowOpacity:swipeOverlayShadowOpacity];
            [rootContentLayer setShadowRadius:swipeOverlayShadowRadius];
            [rootContentLayer setShadowPath:shadowPath.get()];
        } else {
            RetainPtr<CGPathRef> shadowPath = adoptCF(CGPathCreateWithRect([m_swipeLayer bounds], 0));
            [m_swipeLayer setShadowColor:CGColorGetConstantColor(kCGColorBlack)];
            [m_swipeLayer setShadowOpacity:swipeOverlayShadowOpacity];
            [m_swipeLayer setShadowRadius:swipeOverlayShadowRadius];
            [m_swipeLayer setShadowPath:shadowPath.get()];
        }
#else
        FloatRect dimmingRect(FloatPoint(), m_webPageProxy.viewSize());
        m_swipeDimmingLayer = adoptNS([[CALayer alloc] init]);
        [m_swipeDimmingLayer setName:@"Gesture Swipe Dimming Layer"];
        [m_swipeDimmingLayer setBackgroundColor:[NSColor blackColor].CGColor];
        [m_swipeDimmingLayer setOpacity:swipeOverlayDimmingOpacity];
        [m_swipeDimmingLayer setAnchorPoint:CGPointZero];
        [m_swipeDimmingLayer setFrame:dimmingRect];
        [m_swipeDimmingLayer setGeometryFlipped:geometryIsFlippedToRoot];
        [m_swipeDimmingLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];

        FloatRect shadowRect(-swipeOverlayShadowWidth, topContentInset, swipeOverlayShadowWidth, m_webPageProxy.viewSize().height() - topContentInset);
        m_swipeShadowLayer = adoptNS([[CAGradientLayer alloc] init]);
        [m_swipeShadowLayer setName:@"Gesture Swipe Shadow Layer"];
        [m_swipeShadowLayer setColors:@[
            (id)adoptCF(CGColorCreateGenericGray(0, 1.)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.99)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.98)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.95)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.92)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.82)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.71)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.46)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.35)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.25)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.17)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.11)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.07)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.04)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.01)).get(),
            (id)adoptCF(CGColorCreateGenericGray(0, 0.)).get(),
        ]];
        [m_swipeShadowLayer setLocations:@[
            @0,
            @0.03125,
            @0.0625,
            @0.0938,
            @0.125,
            @0.1875,
            @0.25,
            @0.375,
            @0.4375,
            @0.5,
            @0.5625,
            @0.625,
            @0.6875,
            @0.75,
            @0.875,
            @1,
        ]];
        [m_swipeShadowLayer setStartPoint:CGPointMake(1, 0)];
        [m_swipeShadowLayer setEndPoint:CGPointMake(0, 0)];
        [m_swipeShadowLayer setOpacity:swipeOverlayShadowOpacity];
        [m_swipeShadowLayer setAnchorPoint:CGPointZero];
        [m_swipeShadowLayer setFrame:shadowRect];
        [m_swipeShadowLayer setGeometryFlipped:geometryIsFlippedToRoot];
        [m_swipeShadowLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];

        if (direction == SwipeDirection::Back)
            [snapshotLayerParent insertSublayer:m_swipeDimmingLayer.get() above:m_swipeLayer.get()];
        else
            [snapshotLayerParent insertSublayer:m_swipeDimmingLayer.get() below:m_swipeLayer.get()];

        [snapshotLayerParent insertSublayer:m_swipeShadowLayer.get() above:m_swipeLayer.get()];
#endif
    }
}

void ViewGestureController::handleSwipeGesture(WebBackForwardListItem* targetItem, double progress, SwipeDirection direction)
{
    ASSERT(m_activeGestureType == ViewGestureType::Swipe);

    if (!m_webPageProxy.drawingArea())
        return;

    double width;
    if (!m_customSwipeViews.isEmpty())
        width = m_currentSwipeCustomViewBounds.width();
    else
        width = m_webPageProxy.drawingArea()->size().width();

    double swipingLayerOffset = floor(width * progress);

#if !ENABLE(LEGACY_SWIPE_SHADOW_STYLE)
    double dimmingProgress = (direction == SwipeDirection::Back) ? 1 - progress : -progress;
    dimmingProgress = std::min(1., std::max(dimmingProgress, 0.));
    [m_swipeDimmingLayer setOpacity:dimmingProgress * swipeOverlayDimmingOpacity];

    double absoluteProgress = fabs(progress);
    double remainingSwipeDistance = width - fabs(absoluteProgress * width);
    double shadowFadeDistance = [m_swipeShadowLayer bounds].size.width;
    if (remainingSwipeDistance < shadowFadeDistance)
        [m_swipeShadowLayer setOpacity:(remainingSwipeDistance / shadowFadeDistance) * swipeOverlayShadowOpacity];
    else
        [m_swipeShadowLayer setOpacity:swipeOverlayShadowOpacity];

    if (m_swipeTransitionStyle == SwipeTransitionStyle::Overlap)
        [m_swipeShadowLayer setTransform:CATransform3DMakeTranslation((direction == SwipeDirection::Back ? 0 : width) + swipingLayerOffset, 0, 0)];
#endif

    if (m_swipeTransitionStyle == SwipeTransitionStyle::Overlap) {
        if (direction == SwipeDirection::Forward) {
            [m_swipeLayer setTransform:CATransform3DMakeTranslation(width + swipingLayerOffset, 0, 0)];
            didMoveSwipeSnapshotLayer();
        }
    } else if (m_swipeTransitionStyle == SwipeTransitionStyle::Push)
        [m_swipeLayer setTransform:CATransform3DMakeTranslation((direction == SwipeDirection::Back ? -width : width) + swipingLayerOffset, 0, 0)];

    for (const auto& layer : m_currentSwipeLiveLayers) {
        if (m_swipeTransitionStyle == SwipeTransitionStyle::Overlap) {
            if (direction == SwipeDirection::Back)
                [layer setTransform:CATransform3DMakeTranslation(swipingLayerOffset, 0, 0)];
        } else if (m_swipeTransitionStyle == SwipeTransitionStyle::Push)
            [layer setTransform:CATransform3DMakeTranslation(swipingLayerOffset, 0, 0)];
    }
}

void ViewGestureController::didMoveSwipeSnapshotLayer()
{
    if (!m_didMoveSwipeSnapshotCallback)
        return;

    m_didMoveSwipeSnapshotCallback(m_webPageProxy.boundsOfLayerInLayerBackedWindowCoordinates(m_swipeLayer.get()));
}

void ViewGestureController::endSwipeGesture(WebBackForwardListItem* targetItem, bool cancelled)
{
    ASSERT(m_activeGestureType == ViewGestureType::Swipe);

    m_swipeCancellationTracker = nullptr;

    m_swipeInProgress = false;

    CALayer *rootLayer = m_webPageProxy.acceleratedCompositingRootLayer();

    [rootLayer setShadowOpacity:0];
    [rootLayer setShadowRadius:0];

    if (cancelled) {
        removeSwipeSnapshot();
        m_webPageProxy.navigationGestureDidEnd(false, *targetItem);
        return;
    }

    uint64_t renderTreeSize = 0;
    if (ViewSnapshot* snapshot = targetItem->snapshot())
        renderTreeSize = snapshot->renderTreeSize();

    m_webPageProxy.process().send(Messages::ViewGestureGeometryCollector::SetRenderTreeSizeNotificationThreshold(renderTreeSize * swipeSnapshotRemovalRenderTreeSizeTargetFraction), m_webPageProxy.pageID());

    m_swipeWaitingForVisuallyNonEmptyLayout = true;
    m_swipeWaitingForRenderTreeSizeThreshold = true;

    m_webPageProxy.navigationGestureDidEnd(true, *targetItem);
    m_webPageProxy.goToBackForwardItem(targetItem);

    // FIXME: Like on iOS, we should ensure that even if one of the timeouts fires,
    // we never show the old page content, instead showing white (or the snapshot background color).
    m_swipeWatchdogTimer.startOneShot(swipeSnapshotRemovalWatchdogDuration.count());

    if (ViewSnapshot* snapshot = targetItem->snapshot())
        m_backgroundColorForCurrentSnapshot = snapshot->backgroundColor();
}

void ViewGestureController::didHitRenderTreeSizeThreshold()
{
    if (m_activeGestureType != ViewGestureType::Swipe || m_swipeInProgress)
        return;

    m_swipeWaitingForRenderTreeSizeThreshold = false;

    if (!m_swipeWaitingForVisuallyNonEmptyLayout) {
        // FIXME: Ideally we would call removeSwipeSnapshotAfterRepaint() here, but sometimes
        // scroll position isn't done restoring until didFinishLoadForFrame, so we flash the wrong content.
    }
}

void ViewGestureController::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    if (m_activeGestureType != ViewGestureType::Swipe || m_swipeInProgress)
        return;

    m_swipeWaitingForVisuallyNonEmptyLayout = false;

    if (!m_swipeWaitingForRenderTreeSizeThreshold) {
        // FIXME: Ideally we would call removeSwipeSnapshotAfterRepaint() here, but sometimes
        // scroll position isn't done restoring until didFinishLoadForFrame, so we flash the wrong content.
    } else {
        m_swipeWatchdogAfterFirstVisuallyNonEmptyLayoutTimer.startOneShot(swipeSnapshotRemovalWatchdogAfterFirstVisuallyNonEmptyLayoutDuration.count());
        m_swipeWatchdogTimer.stop();
    }
}

void ViewGestureController::mainFrameLoadDidReachTerminalState()
{
    if (m_activeGestureType != ViewGestureType::Swipe || m_swipeInProgress)
        return;

    if (m_webPageProxy.pageLoadState().isLoading()) {
        m_swipeActiveLoadMonitoringTimer.startRepeating(swipeSnapshotRemovalActiveLoadMonitoringInterval);
        return;
    }

    removeSwipeSnapshotAfterRepaint();
}

void ViewGestureController::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType type)
{
    if (m_activeGestureType != ViewGestureType::Swipe || m_swipeInProgress)
        return;

    if (type != SameDocumentNavigationSessionStateReplace && type != SameDocumentNavigationSessionStatePop)
        return;

    m_swipeActiveLoadMonitoringTimer.startRepeating(swipeSnapshotRemovalActiveLoadMonitoringInterval);
}

void ViewGestureController::activeLoadMonitoringTimerFired()
{
    if (m_webPageProxy.pageLoadState().isLoading())
        return;

    removeSwipeSnapshotAfterRepaint();
}

void ViewGestureController::swipeSnapshotWatchdogTimerFired()
{
    removeSwipeSnapshotAfterRepaint();
}

void ViewGestureController::removeSwipeSnapshotAfterRepaint()
{
    m_swipeActiveLoadMonitoringTimer.stop();

    if (m_activeGestureType != ViewGestureType::Swipe || m_swipeInProgress)
        return;

    if (m_swipeWaitingForRepaint)
        return;

    m_swipeWaitingForRepaint = true;

    WebPageProxy* webPageProxy = &m_webPageProxy;
    m_webPageProxy.forceRepaint(VoidCallback::create([webPageProxy] (CallbackBase::Error error) {
        webPageProxy->removeNavigationGestureSnapshot();
    }));
}

void ViewGestureController::removeSwipeSnapshot()
{
    m_swipeWaitingForRepaint = false;

    m_swipeWatchdogTimer.stop();
    m_swipeWatchdogAfterFirstVisuallyNonEmptyLayoutTimer.stop();

    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

#if USE_IOSURFACE_VIEW_SNAPSHOTS
    if (m_currentSwipeSnapshot && m_currentSwipeSnapshot->surface())
        m_currentSwipeSnapshot->surface()->setIsVolatile(true);
    m_currentSwipeSnapshot = nullptr;
#endif

    for (const auto& layer : m_currentSwipeLiveLayers)
        [layer setTransform:CATransform3DIdentity];

    [m_swipeSnapshotLayer removeFromSuperlayer];
    m_swipeSnapshotLayer = nullptr;

    [m_swipeLayer removeFromSuperlayer];
    m_swipeLayer = nullptr;

#if !ENABLE(LEGACY_SWIPE_SHADOW_STYLE)
    [m_swipeShadowLayer removeFromSuperlayer];
    m_swipeShadowLayer = nullptr;

    [m_swipeDimmingLayer removeFromSuperlayer];
    m_swipeDimmingLayer = nullptr;
#endif

    m_currentSwipeLiveLayers.clear();

    m_activeGestureType = ViewGestureType::None;

    m_webPageProxy.navigationGestureSnapshotWasRemoved();

    m_backgroundColorForCurrentSnapshot = Color();
}

double ViewGestureController::magnification() const
{
    if (m_activeGestureType == ViewGestureType::Magnification)
        return m_magnification;

    return m_webPageProxy.pageScaleFactor();
}

} // namespace WebKit

#endif // !PLATFORM(IOS)
