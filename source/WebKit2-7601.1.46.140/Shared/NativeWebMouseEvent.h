/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef NativeWebMouseEvent_h
#define NativeWebMouseEvent_h

#include "WebEvent.h"

#if USE(APPKIT)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSView;
#endif

#if PLATFORM(EFL)
#include "WebEventFactory.h"
#include <Evas.h>
#include <WebCore/AffineTransform.h>
#endif

#if PLATFORM(GTK)
#include <WebCore/GUniquePtrGtk.h>
typedef union _GdkEvent GdkEvent;
#endif

namespace WebKit {

class NativeWebMouseEvent : public WebMouseEvent {
public:
#if USE(APPKIT)
    NativeWebMouseEvent(NSEvent *, NSEvent *lastPressureEvent, NSView *);
#elif PLATFORM(GTK)
    NativeWebMouseEvent(const NativeWebMouseEvent&);
    NativeWebMouseEvent(GdkEvent*, int);
#elif PLATFORM(EFL)
    template <typename EvasEventMouse>
    NativeWebMouseEvent(const EvasEventMouse*, const WebCore::AffineTransform&, const WebCore::AffineTransform&);
#endif

#if USE(APPKIT)
    NSEvent* nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(GTK)
    const GdkEvent* nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(EFL)
    const void* nativeEvent() const { return m_nativeEvent; }
#elif PLATFORM(IOS)
    const void* nativeEvent() const { return 0; }
#endif

private:
#if USE(APPKIT)
    RetainPtr<NSEvent> m_nativeEvent;
#elif PLATFORM(GTK)
    GUniquePtr<GdkEvent> m_nativeEvent;
#elif PLATFORM(EFL)
    const void* m_nativeEvent;
#endif
};

#if PLATFORM(EFL)
template <typename EvasEventMouse>
NativeWebMouseEvent::NativeWebMouseEvent(const EvasEventMouse* event, const WebCore::AffineTransform& toWebContent, const WebCore::AffineTransform& toDeviceScreen)
    : WebMouseEvent(WebEventFactory::createWebMouseEvent(event, toWebContent, toDeviceScreen))
    , m_nativeEvent(event)
{
}
#endif

} // namespace WebKit

#endif // NativeWebMouseEvent_h
