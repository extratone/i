/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011, 2012 Igalia S.L
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

#ifndef NativeWebKeyboardEvent_h
#define NativeWebKeyboardEvent_h

#include "WebEvent.h"

#if USE(APPKIT)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSView;

namespace WebCore {
struct KeypressCommand;
}
#endif

#if PLATFORM(EFL)
#include <Evas.h>
#endif

#if PLATFORM(GTK)
#include "InputMethodFilter.h"
#include <WebCore/CompositionResults.h>
#include <WebCore/GUniquePtrGtk.h>
typedef union _GdkEvent GdkEvent;
#endif

#if PLATFORM(IOS)
#include <wtf/RetainPtr.h>
OBJC_CLASS WebIOSEvent;
#endif

namespace WebKit {

class NativeWebKeyboardEvent : public WebKeyboardEvent {
public:
#if USE(APPKIT)
    NativeWebKeyboardEvent(NSEvent *, bool handledByInputMethod, const Vector<WebCore::KeypressCommand>&);
#elif PLATFORM(GTK)
    NativeWebKeyboardEvent(const NativeWebKeyboardEvent&);
    NativeWebKeyboardEvent(GdkEvent*, const WebCore::CompositionResults&, InputMethodFilter::EventFakedForComposition, Vector<String>&& commands);
#elif PLATFORM(EFL)
    NativeWebKeyboardEvent(const Evas_Event_Key_Down*, bool);
    NativeWebKeyboardEvent(const Evas_Event_Key_Up*);
#elif PLATFORM(IOS)
    NativeWebKeyboardEvent(WebIOSEvent *);
#endif

#if USE(APPKIT)
    NSEvent *nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(GTK)
    GdkEvent* nativeEvent() const { return m_nativeEvent.get(); }
    const WebCore::CompositionResults& compositionResults() const  { return m_compositionResults; }
    bool isFakeEventForComposition() const { return m_fakeEventForComposition; }
#elif PLATFORM(EFL)
    const void* nativeEvent() const { return m_nativeEvent; }
    bool isFiltered() const { return m_isFiltered; }
#elif PLATFORM(IOS)
    WebIOSEvent* nativeEvent() const { return m_nativeEvent.get(); }
#endif

private:
#if USE(APPKIT)
    RetainPtr<NSEvent> m_nativeEvent;
#elif PLATFORM(GTK)
    GUniquePtr<GdkEvent> m_nativeEvent;
    WebCore::CompositionResults m_compositionResults;
    bool m_fakeEventForComposition;
#elif PLATFORM(EFL)
    const void* m_nativeEvent;
    bool m_isFiltered;
#elif PLATFORM(IOS)
    RetainPtr<WebIOSEvent> m_nativeEvent;
#endif
};

} // namespace WebKit

#endif // NativeWebKeyboardEvent_h
