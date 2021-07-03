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

#if PLATFORM(IOS)

#import "UIKitSPI.h"
#import "ViewGestureControllerMessages.h"
#import "ViewGestureGeometryCollectorMessages.h"
#import "ViewSnapshotStore.h"
#import "WKBackForwardListItemInternal.h"
#import "WKWebViewInternal.h"
#import "WebBackForwardList.h"
#import "WebPageGroup.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <UIKit/UIScreenEdgePanGestureRecognizer.h>
#import <WebCore/IOSurface.h>
#import <WebCore/QuartzCoreSPI.h>
#import <wtf/NeverDestroyed.h>

using namespace WebCore;

@interface WKSwipeTransitionController : NSObject <_UINavigationInteractiveTransitionBaseDelegate>
- (instancetype)initWithViewGestureController:(WebKit::ViewGestureController*)gestureController gestureRecognizerView:(UIView *)gestureRecognizerView;
- (void)invalidate;
@end

@interface _UIViewControllerTransitionContext (WKDetails)
@property (nonatomic, copy, setter=_setInteractiveUpdateHandler:)  void (^_interactiveUpdateHandler)(BOOL interactionIsOver, CGFloat percentComplete, BOOL transitionCompleted, _UIViewControllerTransitionContext *);
@end

@implementation WKSwipeTransitionController
{
    WebKit::ViewGestureController *_gestureController;
    RetainPtr<_UINavigationInteractiveTransitionBase> _backTransitionController;
    RetainPtr<_UINavigationInteractiveTransitionBase> _forwardTransitionController;
    WebKit::WeakObjCPtr<UIView> _gestureRecognizerView;
}

static const float swipeSnapshotRemovalRenderTreeSizeTargetFraction = 0.5;
static const std::chrono::seconds swipeSnapshotRemovalWatchdogDuration = 3_s;
static const std::chrono::milliseconds swipeSnapshotRemovalActiveLoadMonitoringInterval = 250_ms;

// The key in this map is the associated page ID.
static HashMap<uint64_t, WebKit::ViewGestureController*>& viewGestureControllersForAllPages()
{
    static NeverDestroyed<HashMap<uint64_t, WebKit::ViewGestureController*>> viewGestureControllers;
    return viewGestureControllers.get();
}

- (instancetype)initWithViewGestureController:(WebKit::ViewGestureController*)gestureController gestureRecognizerView:(UIView *)gestureRecognizerView
{
    self = [super init];
    if (self) {
        _gestureController = gestureController;
        _gestureRecognizerView = gestureRecognizerView;

        _backTransitionController = adoptNS([_UINavigationInteractiveTransitionBase alloc]);
        _backTransitionController = [_backTransitionController initWithGestureRecognizerView:gestureRecognizerView animator:nil delegate:self];
        
        _forwardTransitionController = adoptNS([_UINavigationInteractiveTransitionBase alloc]);
        _forwardTransitionController = [_forwardTransitionController initWithGestureRecognizerView:gestureRecognizerView animator:nil delegate:self];
        [_forwardTransitionController setShouldReverseTranslation:YES];
    }
    return self;
}

- (void)invalidate
{
    _gestureController = nullptr;
}

- (WebKit::ViewGestureController::SwipeDirection)directionForTransition:(_UINavigationInteractiveTransitionBase *)transition
{
    return transition == _backTransitionController ? WebKit::ViewGestureController::SwipeDirection::Back : WebKit::ViewGestureController::SwipeDirection::Forward;
}

- (void)startInteractiveTransition:(_UINavigationInteractiveTransitionBase *)transition
{
    _gestureController->beginSwipeGesture(transition, [self directionForTransition:transition]);
}

- (BOOL)shouldBeginInteractiveTransition:(_UINavigationInteractiveTransitionBase *)transition
{
    return _gestureController->canSwipeInDirection([self directionForTransition:transition]);
}

- (BOOL)interactiveTransition:(_UINavigationInteractiveTransitionBase *)transition gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return [otherGestureRecognizer isKindOfClass:[UITapGestureRecognizer class]];
}

- (BOOL)interactiveTransition:(_UINavigationInteractiveTransitionBase *)transition gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
    return YES;
}

- (UIPanGestureRecognizer *)gestureRecognizerForInteractiveTransition:(_UINavigationInteractiveTransitionBase *)transition WithTarget:(id)target action:(SEL)action
{
    UIScreenEdgePanGestureRecognizer *recognizer = [[UIScreenEdgePanGestureRecognizer alloc] initWithTarget:target action:action];

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 90000
    bool isLTR = [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:[_gestureRecognizerView.get() semanticContentAttribute]] == UIUserInterfaceLayoutDirectionLeftToRight;
#else
    bool isLTR = true;
#endif

    switch ([self directionForTransition:transition]) {
    case WebKit::ViewGestureController::SwipeDirection::Back:
        [recognizer setEdges:isLTR ? UIRectEdgeLeft : UIRectEdgeRight];
        break;
    case WebKit::ViewGestureController::SwipeDirection::Forward:
        [recognizer setEdges:isLTR ? UIRectEdgeRight : UIRectEdgeLeft];
        break;
    }
    return [recognizer autorelease];
}

@end

namespace WebKit {

ViewGestureController::ViewGestureController(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_swipeWatchdogTimer(RunLoop::main(), this, &ViewGestureController::swipeSnapshotWatchdogTimerFired)
    , m_swipeActiveLoadMonitoringTimer(RunLoop::main(), this, &ViewGestureController::activeLoadMonitoringTimerFired)
{
    viewGestureControllersForAllPages().add(webPageProxy.pageID(), this);
}

ViewGestureController::~ViewGestureController()
{
    [m_swipeTransitionContext _setTransitionIsInFlight:NO];
    [m_swipeTransitionContext _setInteractor:nil];
    [m_swipeTransitionContext _setAnimator:nil];
    [m_swipeInteractiveTransitionDelegate invalidate];
    viewGestureControllersForAllPages().remove(m_webPageProxy.pageID());
}

void ViewGestureController::setAlternateBackForwardListSourceView(WKWebView *view)
{
    m_alternateBackForwardListSourceView = view;
}

void ViewGestureController::installSwipeHandler(UIView *gestureRecognizerView, UIView *swipingView)
{
    ASSERT(!m_swipeInteractiveTransitionDelegate);
    m_swipeInteractiveTransitionDelegate = adoptNS([[WKSwipeTransitionController alloc] initWithViewGestureController:this gestureRecognizerView:gestureRecognizerView]);
    m_liveSwipeView = swipingView;
}

void ViewGestureController::beginSwipeGesture(_UINavigationInteractiveTransitionBase *transition, SwipeDirection direction)
{
    if (m_activeGestureType != ViewGestureType::None)
        return;

    m_webPageProxy.recordNavigationSnapshot();

    m_webPageProxyForBackForwardListForCurrentSwipe = m_alternateBackForwardListSourceView.get() ? m_alternateBackForwardListSourceView.get()->_page : &m_webPageProxy;
    m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureDidBegin();
    if (&m_webPageProxy != m_webPageProxyForBackForwardListForCurrentSwipe)
        m_webPageProxy.navigationGestureDidBegin();

    auto& backForwardList = m_webPageProxyForBackForwardListForCurrentSwipe->backForwardList();

    // Copy the snapshot from this view to the one that owns the back forward list, so that
    // swiping forward will have the correct snapshot.
    if (m_webPageProxyForBackForwardListForCurrentSwipe != &m_webPageProxy)
        backForwardList.currentItem()->setSnapshot(m_webPageProxy.backForwardList().currentItem()->snapshot());

    RefPtr<WebBackForwardListItem> targetItem = direction == SwipeDirection::Back ? backForwardList.backItem() : backForwardList.forwardItem();

    CGRect liveSwipeViewFrame = [m_liveSwipeView frame];

    RetainPtr<UIViewController> snapshotViewController = adoptNS([[UIViewController alloc] init]);
    m_snapshotView = adoptNS([[UIView alloc] initWithFrame:liveSwipeViewFrame]);

    RetainPtr<UIColor> backgroundColor = [UIColor whiteColor];
    if (ViewSnapshot* snapshot = targetItem->snapshot()) {
        float deviceScaleFactor = m_webPageProxy.deviceScaleFactor();
        FloatSize swipeLayerSizeInDeviceCoordinates(liveSwipeViewFrame.size);
        swipeLayerSizeInDeviceCoordinates.scale(deviceScaleFactor);
        if (snapshot->hasImage() && snapshot->size() == swipeLayerSizeInDeviceCoordinates && deviceScaleFactor == snapshot->deviceScaleFactor())
            [m_snapshotView layer].contents = snapshot->asLayerContents();
        Color coreColor = snapshot->backgroundColor();
        if (coreColor.isValid())
            backgroundColor = adoptNS([[UIColor alloc] initWithCGColor:cachedCGColor(coreColor, ColorSpaceDeviceRGB)]);
    }

    [m_snapshotView setBackgroundColor:backgroundColor.get()];
    [m_snapshotView layer].contentsGravity = kCAGravityTopLeft;
    [m_snapshotView layer].contentsScale = m_liveSwipeView.window.screen.scale;
    [snapshotViewController setView:m_snapshotView.get()];

    m_transitionContainerView = adoptNS([[UIView alloc] initWithFrame:liveSwipeViewFrame]);
    m_liveSwipeViewClippingView = adoptNS([[UIView alloc] initWithFrame:liveSwipeViewFrame]);
    [m_liveSwipeViewClippingView setClipsToBounds:YES];

    [m_liveSwipeView.superview insertSubview:m_transitionContainerView.get() belowSubview:m_liveSwipeView];
    [m_transitionContainerView addSubview:m_liveSwipeViewClippingView.get()];
    [m_liveSwipeViewClippingView addSubview:m_liveSwipeView];

    RetainPtr<UIViewController> targettedViewController = adoptNS([[UIViewController alloc] init]);
    [targettedViewController setView:m_liveSwipeViewClippingView.get()];

    UINavigationControllerOperation transitionOperation = direction == SwipeDirection::Back ? UINavigationControllerOperationPop : UINavigationControllerOperationPush;
    RetainPtr<_UINavigationParallaxTransition> animationController = adoptNS([[_UINavigationParallaxTransition alloc] initWithCurrentOperation:transitionOperation]);

    m_swipeTransitionContext = adoptNS([[_UIViewControllerOneToOneTransitionContext alloc] init]);
    [m_swipeTransitionContext _setFromViewController:targettedViewController.get()];
    [m_swipeTransitionContext _setToViewController:snapshotViewController.get()];
    [m_swipeTransitionContext _setContainerView:m_transitionContainerView.get()];
    [m_swipeTransitionContext _setFromStartFrame:liveSwipeViewFrame];
    [m_swipeTransitionContext _setToEndFrame:liveSwipeViewFrame];
    [m_swipeTransitionContext _setToStartFrame:CGRectZero];
    [m_swipeTransitionContext _setFromEndFrame:CGRectZero];
    [m_swipeTransitionContext _setAnimator:animationController.get()];
    [m_swipeTransitionContext _setInteractor:transition];
    [m_swipeTransitionContext _setTransitionIsInFlight:YES];
    [m_swipeTransitionContext _setInteractiveUpdateHandler:^(BOOL finish, CGFloat percent, BOOL transitionCompleted, _UIViewControllerTransitionContext *) {
        if (finish)
            m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureWillEnd(transitionCompleted, *targetItem);
    }];
    uint64_t pageID = m_webPageProxy.pageID();
    [m_swipeTransitionContext _setCompletionHandler:^(_UIViewControllerTransitionContext *context, BOOL didComplete) {
        auto gestureControllerIter = viewGestureControllersForAllPages().find(pageID);
        if (gestureControllerIter != viewGestureControllersForAllPages().end())
            gestureControllerIter->value->endSwipeGesture(targetItem.get(), context, !didComplete);
    }];
    [m_swipeTransitionContext _setInteractiveUpdateHandler:^(BOOL, CGFloat, BOOL, _UIViewControllerTransitionContext *) { }];

    [transition setAnimationController:animationController.get()];
    [transition startInteractiveTransition:m_swipeTransitionContext.get()];

    m_activeGestureType = ViewGestureType::Swipe;
}

bool ViewGestureController::canSwipeInDirection(SwipeDirection direction)
{
    auto& backForwardList = m_alternateBackForwardListSourceView.get() ? m_alternateBackForwardListSourceView.get()->_page->backForwardList() : m_webPageProxy.backForwardList();
    if (direction == SwipeDirection::Back)
        return !!backForwardList.backItem();
    return !!backForwardList.forwardItem();
}

void ViewGestureController::endSwipeGesture(WebBackForwardListItem* targetItem, _UIViewControllerTransitionContext *context, bool cancelled)
{
    [context _setTransitionIsInFlight:NO];
    [context _setInteractor:nil];
    [context _setAnimator:nil];
    
    [[m_transitionContainerView superview] insertSubview:m_snapshotView.get() aboveSubview:m_transitionContainerView.get()];
    [[m_transitionContainerView superview] insertSubview:m_liveSwipeView aboveSubview:m_transitionContainerView.get()];
    [m_liveSwipeViewClippingView removeFromSuperview];
    m_liveSwipeViewClippingView = nullptr;
    [m_transitionContainerView removeFromSuperview];
    m_transitionContainerView = nullptr;

    if (cancelled) {
        // removeSwipeSnapshot will clear m_webPageProxyForBackForwardListForCurrentSwipe, so hold on to it here.
        RefPtr<WebPageProxy> webPageProxyForBackForwardListForCurrentSwipe = m_webPageProxyForBackForwardListForCurrentSwipe;
        removeSwipeSnapshot();
        webPageProxyForBackForwardListForCurrentSwipe->navigationGestureDidEnd(false, *targetItem);
        if (&m_webPageProxy != webPageProxyForBackForwardListForCurrentSwipe)
            m_webPageProxy.navigationGestureDidEnd();
        return;
    }

    m_snapshotRemovalTargetRenderTreeSize = 0;
    if (ViewSnapshot* snapshot = targetItem->snapshot())
        m_snapshotRemovalTargetRenderTreeSize = snapshot->renderTreeSize() * swipeSnapshotRemovalRenderTreeSizeTargetFraction;

    m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureDidEnd(true, *targetItem);
    if (&m_webPageProxy != m_webPageProxyForBackForwardListForCurrentSwipe)
        m_webPageProxy.navigationGestureDidEnd();

    m_webPageProxyForBackForwardListForCurrentSwipe->goToBackForwardItem(targetItem);

    if (auto drawingArea = m_webPageProxy.drawingArea()) {
        uint64_t pageID = m_webPageProxy.pageID();
        uint64_t gesturePendingSnapshotRemoval = m_gesturePendingSnapshotRemoval;
        drawingArea->dispatchAfterEnsuringDrawing([pageID, gesturePendingSnapshotRemoval] (CallbackBase::Error error) {
            auto gestureControllerIter = viewGestureControllersForAllPages().find(pageID);
            if (gestureControllerIter != viewGestureControllersForAllPages().end() && gestureControllerIter->value->m_gesturePendingSnapshotRemoval == gesturePendingSnapshotRemoval)
                gestureControllerIter->value->willCommitPostSwipeTransitionLayerTree(error == CallbackBase::Error::None);
        });
        drawingArea->hideContentUntilPendingUpdate();
    } else {
        removeSwipeSnapshot();
        return;
    }

    m_swipeWaitingForRenderTreeSizeThreshold = true;
    m_swipeWaitingForRepaint = true;
    m_swipeWaitingForTerminalLoadingState = true;
    m_swipeWaitingForSubresourceLoads = true;
    m_swipeWaitingForScrollPositionRestoration = true;

    m_swipeWatchdogTimer.startOneShot(swipeSnapshotRemovalWatchdogDuration.count());

    if (ViewSnapshot* snapshot = targetItem->snapshot()) {
        m_backgroundColorForCurrentSnapshot = snapshot->backgroundColor();
        m_webPageProxy.didChangeBackgroundColor();
    }
}

void ViewGestureController::willCommitPostSwipeTransitionLayerTree(bool successful)
{
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (!successful) {
        removeSwipeSnapshot();
        return;
    }

    if (!m_swipeWaitingForRepaint)
        return;

    m_swipeWaitingForRepaint = false;
    removeSwipeSnapshotIfReady();
}

void ViewGestureController::setRenderTreeSize(uint64_t renderTreeSize)
{
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (!m_swipeWaitingForRenderTreeSizeThreshold)
        return;

    if (!m_snapshotRemovalTargetRenderTreeSize || renderTreeSize > m_snapshotRemovalTargetRenderTreeSize) {
        m_swipeWaitingForRenderTreeSizeThreshold = false;
        removeSwipeSnapshotIfReady();
    }
}

void ViewGestureController::didRestoreScrollPosition()
{
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (!m_swipeWaitingForScrollPositionRestoration)
        return;

    m_swipeWaitingForScrollPositionRestoration = false;
    removeSwipeSnapshotIfReady();
}

void ViewGestureController::mainFrameLoadDidReachTerminalState()
{
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (!m_swipeWaitingForTerminalLoadingState)
        return;

    m_swipeWaitingForTerminalLoadingState = false;

    if (m_webPageProxy.pageLoadState().isLoading()) {
        m_swipeActiveLoadMonitoringTimer.startRepeating(swipeSnapshotRemovalActiveLoadMonitoringInterval);
        return;
    }

    m_swipeWaitingForSubresourceLoads = false;
    removeSwipeSnapshotIfReady();
}

void ViewGestureController::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType type)
{
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    // This is nearly equivalent to didFinishLoad in the same document navigation case.
    if (!m_swipeWaitingForTerminalLoadingState)
        return;

    m_swipeWaitingForTerminalLoadingState = false;

    if (type != SameDocumentNavigationSessionStateReplace && type != SameDocumentNavigationSessionStatePop)
        return;

    m_swipeActiveLoadMonitoringTimer.startRepeating(swipeSnapshotRemovalActiveLoadMonitoringInterval);
}

void ViewGestureController::activeLoadMonitoringTimerFired()
{
    if (m_webPageProxy.pageLoadState().isLoading())
        return;

    if (!m_swipeWaitingForSubresourceLoads)
        return;

    m_swipeWaitingForSubresourceLoads = false;
    removeSwipeSnapshotIfReady();
}

void ViewGestureController::swipeSnapshotWatchdogTimerFired()
{
    removeSwipeSnapshot();
}

void ViewGestureController::removeSwipeSnapshotIfReady()
{
    if (m_swipeWaitingForRenderTreeSizeThreshold || m_swipeWaitingForRepaint || m_swipeWaitingForTerminalLoadingState || m_swipeWaitingForSubresourceLoads || m_swipeWaitingForScrollPositionRestoration)
        return;

    removeSwipeSnapshot();
}

void ViewGestureController::removeSwipeSnapshot()
{
    m_swipeWaitingForRenderTreeSizeThreshold = false;
    m_swipeWaitingForRepaint = false;
    m_swipeWaitingForTerminalLoadingState = false;
    m_swipeWaitingForSubresourceLoads = false;
    m_swipeWaitingForScrollPositionRestoration = false;

    m_swipeWatchdogTimer.stop();
    m_swipeActiveLoadMonitoringTimer.stop();

    if (m_activeGestureType != ViewGestureType::Swipe)
        return;
    
    ++m_gesturePendingSnapshotRemoval;
    
    [m_snapshotView removeFromSuperview];
    m_snapshotView = nullptr;
    
    m_snapshotRemovalTargetRenderTreeSize = 0;
    m_activeGestureType = ViewGestureType::None;

    m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureSnapshotWasRemoved();
    m_webPageProxyForBackForwardListForCurrentSwipe = nullptr;

    m_swipeTransitionContext = nullptr;

    m_backgroundColorForCurrentSnapshot = Color();
}

} // namespace WebKit

#endif // PLATFORM(IOS)
