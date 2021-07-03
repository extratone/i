/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebInspectorClient.h"

#include "DrawingArea.h"
#include "WebInspector.h"
#include "WebPage.h"
#include <WebCore/InspectorController.h>
#include <WebCore/MainFrame.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <wtf/CurrentTime.h>

#if PLATFORM(IOS)
#include <WebCore/InspectorOverlay.h>
#endif

using namespace WebCore;

namespace WebKit {

class RepaintIndicatorLayerClient final : public GraphicsLayerClient {
public:
    RepaintIndicatorLayerClient(WebInspectorClient& inspectorClient)
        : m_inspectorClient(inspectorClient)
    {
    }
    virtual ~RepaintIndicatorLayerClient() { }
private:
    virtual void notifyAnimationEnded(const GraphicsLayer* layer, const String&) override
    {
        m_inspectorClient.animationEndedForLayer(layer);
    }
    
    WebInspectorClient& m_inspectorClient;
};

WebInspectorClient::WebInspectorClient(WebPage* page)
    : m_page(page)
    , m_highlightOverlay(nullptr)
{
}

WebInspectorClient::~WebInspectorClient()
{
    for (auto layer : m_paintRectLayers) {
        layer->removeFromParent();
        delete layer;
    }

    if (m_paintRectOverlay && m_page->mainFrame())
        m_page->mainFrame()->pageOverlayController().uninstallPageOverlay(m_paintRectOverlay.get(), PageOverlay::FadeMode::Fade);
}

void WebInspectorClient::inspectorDestroyed()
{
    closeInspectorFrontend();
    delete this;
}

WebCore::InspectorFrontendChannel* WebInspectorClient::openInspectorFrontend(InspectorController* controller)
{
    m_page->inspector()->createInspectorPage(controller->isUnderTest());

    return m_page->inspector();
}

void WebInspectorClient::closeInspectorFrontend()
{
    if (m_page->inspector())
        m_page->inspector()->closeFrontend();
}

void WebInspectorClient::bringFrontendToFront()
{
    if (m_page->inspector())
        m_page->inspector()->bringToFront();
}

void WebInspectorClient::didResizeMainFrame(Frame*)
{
    if (m_page->inspector())
        m_page->inspector()->updateDockingAvailability();
}

void WebInspectorClient::highlight()
{
#if !PLATFORM(IOS)
    if (!m_highlightOverlay) {
        RefPtr<PageOverlay> highlightOverlay = PageOverlay::create(*this);
        m_highlightOverlay = highlightOverlay.get();
        m_page->mainFrame()->pageOverlayController().installPageOverlay(highlightOverlay.release(), PageOverlay::FadeMode::Fade);
        m_highlightOverlay->setNeedsDisplay();
    } else {
        m_highlightOverlay->stopFadeOutAnimation();
        m_highlightOverlay->setNeedsDisplay();
    }
#else
    Highlight highlight;
    m_page->corePage()->inspectorController().getHighlight(highlight, InspectorOverlay::CoordinateSystem::Document);
    m_page->showInspectorHighlight(highlight);
#endif
}

void WebInspectorClient::hideHighlight()
{
#if !PLATFORM(IOS)
    if (m_highlightOverlay && m_page->mainFrame())
        m_page->mainFrame()->pageOverlayController().uninstallPageOverlay(m_highlightOverlay, PageOverlay::FadeMode::Fade);
#else
    m_page->hideInspectorHighlight();
#endif
}

void WebInspectorClient::showPaintRect(const FloatRect& rect)
{
    if (!m_paintRectOverlay) {
        m_paintRectOverlay = PageOverlay::create(*this, PageOverlay::OverlayType::Document);
        m_page->mainFrame()->pageOverlayController().installPageOverlay(m_paintRectOverlay, PageOverlay::FadeMode::DoNotFade);
    }

    if (!m_paintIndicatorLayerClient)
        m_paintIndicatorLayerClient = std::make_unique<RepaintIndicatorLayerClient>(*this);

    std::unique_ptr<GraphicsLayer> paintLayer = GraphicsLayer::create(m_page->drawingArea()->graphicsLayerFactory(), *m_paintIndicatorLayerClient);
    
    paintLayer->setAnchorPoint(FloatPoint3D());
    paintLayer->setPosition(rect.location());
    paintLayer->setSize(rect.size());
    paintLayer->setBackgroundColor(Color(1.0f, 0.0f, 0.0f, 0.2f));

    KeyframeValueList fadeKeyframes(AnimatedPropertyOpacity);
    fadeKeyframes.insert(std::make_unique<FloatAnimationValue>(0, 1));

    fadeKeyframes.insert(std::make_unique<FloatAnimationValue>(0.25, 0));
    
    RefPtr<Animation> opacityAnimation = Animation::create();
    opacityAnimation->setDuration(0.25);

    paintLayer->addAnimation(fadeKeyframes, FloatSize(), opacityAnimation.get(), ASCIILiteral("opacity"), 0);
    
    m_paintRectLayers.add(paintLayer.get());

    GraphicsLayer& overlayRootLayer = m_paintRectOverlay->layer();
    overlayRootLayer.addChild(paintLayer.release());
}

void WebInspectorClient::animationEndedForLayer(const GraphicsLayer* layer)
{
    const_cast<GraphicsLayer*>(layer)->removeFromParent();
    m_paintRectLayers.remove(const_cast<GraphicsLayer*>(layer));
    delete layer;
}

#if PLATFORM(IOS)
void WebInspectorClient::showInspectorIndication()
{
    m_page->showInspectorIndication();
}

void WebInspectorClient::hideInspectorIndication()
{
    m_page->hideInspectorIndication();
}

void WebInspectorClient::didSetSearchingForNode(bool enabled)
{
    if (enabled)
        m_page->enableInspectorNodeSearch();
    else
        m_page->disableInspectorNodeSearch();
}
#endif

void WebInspectorClient::pageOverlayDestroyed(PageOverlay&)
{
}

void WebInspectorClient::willMoveToPage(PageOverlay&, Page* page)
{
    if (page)
        return;

    // The page overlay is moving away from the web page, reset it.
    ASSERT(m_highlightOverlay);
    m_highlightOverlay = nullptr;
}

void WebInspectorClient::didMoveToPage(PageOverlay&, Page*)
{
}

void WebInspectorClient::drawRect(PageOverlay&, WebCore::GraphicsContext& context, const WebCore::IntRect& /*dirtyRect*/)
{
    m_page->corePage()->inspectorController().drawHighlight(context);
}

bool WebInspectorClient::mouseEvent(PageOverlay&, const PlatformMouseEvent&)
{
    return false;
}

} // namespace WebKit
