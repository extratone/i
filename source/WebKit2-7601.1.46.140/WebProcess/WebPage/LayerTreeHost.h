/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef LayerTreeHost_h
#define LayerTreeHost_h

#include "LayerTreeContext.h"
#include <WebCore/Color.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace IPC {
class Connection;
class MessageDecoder;
}

namespace WebCore {
class FloatPoint;
class FloatRect;
class IntRect;
class IntSize;
class GraphicsLayer;
class GraphicsLayerFactory;
#if USE(COORDINATED_GRAPHICS_THREADED)
class ViewportAttributes;
#endif
}

namespace WebKit {

class UpdateInfo;
class WebPage;

class LayerTreeHost : public RefCounted<LayerTreeHost> {
public:
    static PassRefPtr<LayerTreeHost> create(WebPage*);
    virtual ~LayerTreeHost();

    virtual const LayerTreeContext& layerTreeContext() = 0;
    virtual void scheduleLayerFlush() = 0;
    virtual void setLayerFlushSchedulingEnabled(bool) = 0;
    virtual void setShouldNotifyAfterNextScheduledLayerFlush(bool) = 0;
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) = 0;
    virtual void invalidate() = 0;

    virtual void setNonCompositedContentsNeedDisplay() = 0;
    virtual void setNonCompositedContentsNeedDisplayInRect(const WebCore::IntRect&) = 0;
    virtual void scrollNonCompositedContents(const WebCore::IntRect& scrollRect) = 0;
    virtual void forceRepaint() = 0;
    virtual bool forceRepaintAsync(uint64_t /*callbackID*/) { return false; }
    virtual void sizeDidChange(const WebCore::IntSize& newSize) = 0;
    virtual void deviceOrPageScaleFactorChanged() = 0;
    virtual void pageBackgroundTransparencyChanged() = 0;

    virtual void pauseRendering() { }
    virtual void resumeRendering() { }

    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() { return 0; }

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    virtual void didReceiveCoordinatedLayerTreeHostMessage(IPC::Connection&, IPC::MessageDecoder&) = 0;
#endif

#if USE(COORDINATED_GRAPHICS_THREADED)
    virtual void viewportSizeChanged(const WebCore::IntSize&) = 0;
    virtual void didChangeViewportProperties(const WebCore::ViewportAttributes&) = 0;
#endif

#if USE(COORDINATED_GRAPHICS) && ENABLE(REQUEST_ANIMATION_FRAME)
    virtual void scheduleAnimation() = 0;
#endif

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK)
    virtual void setNativeSurfaceHandleForCompositing(uint64_t) = 0;
#endif

    virtual void setViewOverlayRootLayer(WebCore::GraphicsLayer*) = 0;

protected:
    explicit LayerTreeHost(WebPage*);

    WebPage* m_webPage;
};

} // namespace WebKit

#endif // LayerTreeHost_h
