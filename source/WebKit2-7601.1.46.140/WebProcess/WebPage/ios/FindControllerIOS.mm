/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import <config.h>

#if PLATFORM(IOS)

#import "FindController.h"
#import "FindIndicatorOverlayClientIOS.h"
#import "SmartMagnificationControllerMessages.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/MainFrame.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>

using namespace WebCore;

const int cornerRadius = 3;
const int totalHorizontalMargin = 2;
const int totalVerticalMargin = 1;

static Color highlightColor()
{
    return Color(255, 228, 56, 255);
}

namespace WebKit {

void FindIndicatorOverlayClientIOS::drawRect(PageOverlay& overlay, GraphicsContext& context, const IntRect& dirtyRect)
{
    // FIXME: Support multiple text rects.

    IntRect overlayFrame = overlay.frame();
    IntRect overlayBounds = overlay.bounds();

    {
        GraphicsContextStateSaver stateSaver(context);
        Path clipPath;
        clipPath.addRoundedRect(overlayBounds, FloatSize(cornerRadius, cornerRadius));
        context.clip(clipPath);
        context.setFillColor(highlightColor(), ColorSpaceDeviceRGB);
        context.fillRect(overlayBounds);
    }

    {
        GraphicsContextStateSaver stateSaver(context);
        context.translate(-overlayFrame.x(), -overlayFrame.y());
        m_frame.view()->setPaintBehavior(PaintBehaviorSelectionOnly | PaintBehaviorForceBlackText | PaintBehaviorFlattenCompositingLayers);
        m_frame.view()->paintContents(&context, overlayFrame);
        m_frame.view()->setPaintBehavior(PaintBehaviorNormal);
    }
}

bool FindController::updateFindIndicator(Frame& selectedFrame, bool isShowingOverlay, bool shouldAnimate)
{
    IntRect matchRect = enclosingIntRect(selectedFrame.selection().selectionBounds(false));
    matchRect = selectedFrame.view()->contentsToRootView(matchRect);
    matchRect.inflateX(totalHorizontalMargin);
    matchRect.inflateY(totalVerticalMargin);

    if (m_findIndicatorOverlay)
        m_webPage->mainFrame()->pageOverlayController().uninstallPageOverlay(m_findIndicatorOverlay.get(), PageOverlay::FadeMode::DoNotFade);

    m_findIndicatorOverlayClient = std::make_unique<FindIndicatorOverlayClientIOS>(selectedFrame);
    m_findIndicatorOverlay = PageOverlay::create(*m_findIndicatorOverlayClient, PageOverlay::OverlayType::Document);
    m_webPage->mainFrame()->pageOverlayController().installPageOverlay(m_findIndicatorOverlay, PageOverlay::FadeMode::DoNotFade);

    m_findIndicatorOverlay->setFrame(matchRect);
    m_findIndicatorOverlay->setNeedsDisplay();

    if (isShowingOverlay || shouldAnimate) {
        FloatRect visibleContentRect = m_webPage->mainFrameView()->unobscuredContentRectIncludingScrollbars();

        bool isReplaced;
        const VisibleSelection& visibleSelection = selectedFrame.selection().selection();
        FloatRect renderRect = visibleSelection.start().containerNode()->renderRect(&isReplaced);

        IntRect startRect = visibleSelection.visibleStart().absoluteCaretBounds();

        m_webPage->send(Messages::SmartMagnificationController::Magnify(startRect.center(), renderRect, visibleContentRect, m_webPage->minimumPageScaleFactor(), m_webPage->maximumPageScaleFactor()));
    }

    m_findIndicatorRect = matchRect;
    m_isShowingFindIndicator = true;
    
    return true;
}

void FindController::hideFindIndicator()
{
    if (!m_isShowingFindIndicator)
        return;

    m_webPage->mainFrame()->pageOverlayController().uninstallPageOverlay(m_findIndicatorOverlay.get(), PageOverlay::FadeMode::DoNotFade);
    m_findIndicatorOverlay = nullptr;
    m_isShowingFindIndicator = false;
    m_foundStringMatchIndex = -1;
    didHideFindIndicator();
}

void FindController::willFindString()
{
    for (Frame* coreFrame = m_webPage->mainFrame(); coreFrame; coreFrame = coreFrame->tree().traverseNext()) {
        coreFrame->editor().setIgnoreCompositionSelectionChange(true);
        coreFrame->selection().setUpdateAppearanceEnabled(true);
    }
}

void FindController::didFailToFindString()
{
    for (Frame* coreFrame = m_webPage->mainFrame(); coreFrame; coreFrame = coreFrame->tree().traverseNext()) {
        coreFrame->selection().setUpdateAppearanceEnabled(false);
        coreFrame->editor().setIgnoreCompositionSelectionChange(false);
    }
}

void FindController::didHideFindIndicator()
{
    for (Frame* coreFrame = m_webPage->mainFrame(); coreFrame; coreFrame = coreFrame->tree().traverseNext()) {
        coreFrame->selection().setUpdateAppearanceEnabled(false);
        coreFrame->editor().setIgnoreCompositionSelectionChange(false);
    }
}

} // namespace WebKit

#endif
