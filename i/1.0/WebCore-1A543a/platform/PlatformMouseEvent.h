/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PlatformMouseEvent_h
#define PlatformMouseEvent_h

#include <GraphicsServices/GSEvent.h>

#include "IntPoint.h"

#if __APPLE__
#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent;
#endif
#endif

#if WIN32
typedef struct HWND__* HWND;
typedef unsigned WPARAM;
typedef long LPARAM;
#endif

#if PLATFORM(GDK)
typedef union _GdkEvent GdkEvent;
#endif

namespace WebCore {

    // These button numbers match the one used in the DOM API.
    enum MouseButton { LeftButton, MiddleButton, RightButton };

    class PlatformMouseEvent {
    public:
        static const struct CurrentEventTag {} currentEvent;
    
        PlatformMouseEvent()
            : m_button(LeftButton)
            , m_clickCount(0)
            , m_shiftKey(false)
            , m_ctrlKey(false)
            , m_altKey(false)
            , m_metaKey(false)
        {
        }

        PlatformMouseEvent(const CurrentEventTag&);

        PlatformMouseEvent(const IntPoint& pos, const IntPoint& globalPos, MouseButton button,
                           int clickCount, bool shift, bool ctrl, bool alt, bool meta)
            : m_position(pos), m_globalPosition(globalPos), m_button(button)
            , m_clickCount(clickCount)
            , m_shiftKey(shift)
            , m_ctrlKey(ctrl)
            , m_altKey(alt)
            , m_metaKey(meta)
        {
        }

        const IntPoint& pos() const { return m_position; }
        int x() const { return m_position.x(); }
        int y() const { return m_position.y(); }
        int globalX() const { return m_globalPosition.x(); }
        int globalY() const { return m_globalPosition.y(); }
        MouseButton button() const { return m_button; }
        int clickCount() const { return m_clickCount; }
        bool shiftKey() const { return m_shiftKey; }
        bool ctrlKey() const { return m_ctrlKey; }
        bool altKey() const { return m_altKey; }
        bool metaKey() const { return m_metaKey; }

#if __APPLE__
        PlatformMouseEvent(GSEventRef);
#endif
#if WIN32
        PlatformMouseEvent(HWND, WPARAM, LPARAM, int clickCount);
#endif
#if PLATFORM(GDK) 
        PlatformMouseEvent(GdkEvent*);
#endif

    private:
        IntPoint m_position;
        IntPoint m_globalPosition;
        MouseButton m_button;
        int m_clickCount;
        bool m_shiftKey;
        bool m_ctrlKey;
        bool m_altKey;
        bool m_metaKey;
    };

} // namespace WebCore

#endif // PlatformMouseEvent_h
