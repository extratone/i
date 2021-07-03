
/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef PageClientImpl_h
#define PageClientImpl_h

#include "DefaultUndoController.h"
#include "PageClient.h"
#include "WebFullScreenManagerProxy.h"
#include "WebPageProxy.h"
#include <WebCore/IntSize.h>
#include <gtk/gtk.h>
#include <memory>

namespace WebKit {

class DrawingAreaProxy;
class WebPageNamespace;

class PageClientImpl : public PageClient
#if ENABLE(FULLSCREEN_API)
    , public WebFullScreenManagerProxyClient
#endif
{
public:
    explicit PageClientImpl(GtkWidget*);

    GtkWidget* viewWidget() { return m_viewWidget; }

private:
    // PageClient
    virtual std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy() override;
    virtual void setViewNeedsDisplay(const WebCore::IntRect&) override;
    virtual void displayView() override;
    virtual bool canScrollView() override { return false; }
    virtual void scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset) override;
    virtual void requestScroll(const WebCore::FloatPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin, bool isProgrammaticScroll) override;
    virtual WebCore::IntSize viewSize() override;
    virtual bool isViewWindowActive() override;
    virtual bool isViewFocused() override;
    virtual bool isViewVisible() override;
    virtual bool isViewInWindow() override;
    virtual void processDidExit() override;
    virtual void didRelaunchProcess() override;
    virtual void pageClosed() override;
    virtual void preferencesDidChange() override;
    virtual void toolTipChanged(const WTF::String&, const WTF::String&) override;
    virtual void setCursor(const WebCore::Cursor&) override;
    virtual void setCursorHiddenUntilMouseMoves(bool) override;
    virtual void didChangeViewportProperties(const WebCore::ViewportAttributes&) override;
    virtual void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo) override;
    virtual void clearAllEditCommands() override;
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo) override;
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo) override;
    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) override;
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) override;
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) override;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) override;
    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) override;
    virtual PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*) override;
    virtual PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*) override;
#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassRefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color& intialColor, const WebCore::IntRect&) override;
#endif
    virtual void setTextIndicator(Ref<WebCore::TextIndicator>, WebCore::TextIndicatorLifetime = WebCore::TextIndicatorLifetime::Permanent) override;
    virtual void clearTextIndicator(WebCore::TextIndicatorDismissalAnimation = WebCore::TextIndicatorDismissalAnimation::FadeOut) override;
    virtual void setTextIndicatorAnimationProgress(float) override;
    virtual void updateTextInputState() override;
#if ENABLE(DRAG_SUPPORT)
    virtual void startDrag(const WebCore::DragData&, PassRefPtr<ShareableBitmap> dragImage) override;
#endif

    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    virtual void exitAcceleratedCompositingMode() override;
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&) override;

    virtual void handleDownloadRequest(DownloadProxy*) override;
    virtual void didChangeContentSize(const WebCore::IntSize&) override { }
    virtual void didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) override;
    virtual void didFailLoadForMainFrame() override { }

    // Auxiliary Client Creation
#if ENABLE(FULLSCREEN_API)
    virtual WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() final;
#endif

#if ENABLE(FULLSCREEN_API)
    // WebFullScreenManagerProxyClient
    virtual void closeFullScreenManager() override;
    virtual bool isFullScreen() override;
    virtual void enterFullScreen() override;
    virtual void exitFullScreen() override;
    virtual void beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame) override;
    virtual void beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame) override;
#endif

    virtual void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&) override;

    virtual void navigationGestureDidBegin() override;
    virtual void navigationGestureWillEnd(bool, WebBackForwardListItem&) override;
    virtual void navigationGestureDidEnd(bool, WebBackForwardListItem&) override;
    virtual void navigationGestureDidEnd() override;
    virtual void willRecordNavigationSnapshot(WebBackForwardListItem&) override;

    virtual void didFirstVisuallyNonEmptyLayoutForMainFrame() override;
    virtual void didFinishLoadForMainFrame() override;
    virtual void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) override;

    virtual void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled) override;

    virtual void didChangeBackgroundColor() override;

#if ENABLE(VIDEO)
    virtual void mediaDocumentNaturalSizeChanged(const WebCore::IntSize&) override { }
#endif

    virtual void refView() override;
    virtual void derefView() override;

    // Members of PageClientImpl class
    GtkWidget* m_viewWidget;
    DefaultUndoController m_undoController;
};

} // namespace WebKit

#endif // PageClientImpl_h
