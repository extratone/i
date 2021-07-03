/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RemoteLayerBackingStore_h
#define RemoteLayerBackingStore_h

#include "ShareableBitmap.h"
#include <WebCore/FloatRect.h>
#include <WebCore/IOSurface.h>
#include <WebCore/Region.h>
#include <chrono>

OBJC_CLASS CALayer;

// FIXME: Make PlatformCALayerRemote.cpp Objective-C so we can include WebLayer.h here and share the typedef.
namespace WebCore {
typedef Vector<WebCore::FloatRect, 5> RepaintRectList;
}

namespace WebKit {

class PlatformCALayerRemote;

class RemoteLayerBackingStore {
    WTF_MAKE_NONCOPYABLE(RemoteLayerBackingStore);
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteLayerBackingStore(PlatformCALayerRemote*);
    ~RemoteLayerBackingStore();

    void ensureBackingStore(WebCore::FloatSize, float scale, bool acceleratesDrawing, bool isOpaque);

    void setNeedsDisplay(const WebCore::IntRect);
    void setNeedsDisplay();

    bool display();

    WebCore::FloatSize size() const { return m_size; }
    float scale() const { return m_scale; }
    bool acceleratesDrawing() const { return m_acceleratesDrawing; }
    bool isOpaque() const { return m_isOpaque; }
    unsigned bytesPerPixel() const;

    PlatformCALayerRemote* layer() const { return m_layer; }

    void applyBackingStoreToLayer(CALayer *);

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, RemoteLayerBackingStore&);

    void enumerateRectsBeingDrawn(CGContextRef, void (^)(CGRect));

    bool hasFrontBuffer() const
    {
#if USE(IOSURFACE)
        if (m_acceleratesDrawing)
            return !!m_frontBuffer.surface;
#endif
        return !!m_frontBuffer.bitmap;
    }

    RetainPtr<CGContextRef> takeFrontContextPendingFlush();

    enum class BufferType {
        Front,
        Back,
        SecondaryBack
    };

    bool setBufferVolatility(BufferType type, bool isVolatile);

    std::chrono::steady_clock::time_point lastDisplayTime() const { return m_lastDisplayTime; }

private:
    void drawInContext(WebCore::GraphicsContext&, CGImageRef backImage);
    void clearBackingStore();
    void swapToValidFrontBuffer();
    
    WebCore::IntSize backingStoreSize() const;

    PlatformCALayerRemote* m_layer;

    WebCore::FloatSize m_size;
    float m_scale;
    bool m_isOpaque;

    WebCore::Region m_dirtyRegion;

    struct Buffer {
        RefPtr<ShareableBitmap> bitmap;
#if USE(IOSURFACE)
        std::unique_ptr<WebCore::IOSurface> surface;
        bool isVolatile = false;
#endif

        explicit operator bool() const
        {
#if USE(IOSURFACE)
            if (surface)
                return true;
#endif
            if (bitmap)
                return true;

            return false;
        }

        void discard();
    };

    Buffer m_frontBuffer;
    Buffer m_backBuffer;
#if USE(IOSURFACE)
    Buffer m_secondaryBackBuffer;
#endif

    RetainPtr<CGContextRef> m_frontContextPendingFlush;

    bool m_acceleratesDrawing;

    WebCore::RepaintRectList m_paintingRects;

    std::chrono::steady_clock::time_point m_lastDisplayTime;
};

} // namespace WebKit

#endif // RemoteLayerBackingStore_h
