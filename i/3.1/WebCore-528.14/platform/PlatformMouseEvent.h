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
#include <wtf/Platform.h>

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSEvent;
@class NSScreen;
@class NSWindow;
#else
class NSEvent;
class NSScreen;
class NSWindow;
#endif
#endif

#if PLATFORM(WIN)
typedef struct HWND__* HWND;
typedef unsigned UINT;
typedef unsigned WPARAM;
typedef long LPARAM;
#endif

#if PLATFORM(GTK)
typedef struct _GdkEventButton GdkEventButton;
typedef struct _GdkEventMotion GdkEventMotion;
#endif

#if PLATFORM(QT)
QT_BEGIN_NAMESPACE
class QInputEvent;
QT_END_NAMESPACE
#endif

#if PLATFORM(WX)
class wxMouseEvent;
#endif

namespace WebCore {
    
    // These button numbers match the ones used in the DOM API, 0 through 2, except for NoButton which isn't specified.
    enum MouseButton { NoButton = -1, LeftButton, MiddleButton, RightButton };
    enum MouseEventType { MouseEventMoved, MouseEventPressed, MouseEventReleased, MouseEventScroll };
    
    class PlatformMouseEvent {
    public:
        PlatformMouseEvent()
            : m_button(NoButton)
            , m_eventType(MouseEventMoved)
            , m_clickCount(0)
            , m_shiftKey(false)
            , m_ctrlKey(false)
            , m_altKey(false)
            , m_metaKey(false)
            , m_timestamp(0)
            , m_modifierFlags(0)
        {
        }

        PlatformMouseEvent(const IntPoint& pos, const IntPoint& globalPos, MouseButton button, MouseEventType eventType,
                           int clickCount, bool shift, bool ctrl, bool alt, bool meta, double timestamp)
            : m_position(pos), m_globalPosition(globalPos), m_button(button)
            , m_eventType(eventType)
            , m_clickCount(clickCount)
            , m_shiftKey(shift)
            , m_ctrlKey(ctrl)
            , m_altKey(alt)
            , m_metaKey(meta)
            , m_timestamp(timestamp)
            , m_modifierFlags(0)
        {
        }

        const IntPoint& pos() const { return m_position; }
        int x() const { return m_position.x(); }
        int y() const { return m_position.y(); }
        int globalX() const { return m_globalPosition.x(); }
        int globalY() const { return m_globalPosition.y(); }
        MouseButton button() const { return m_button; }
        MouseEventType eventType() const { return m_eventType; }
        int clickCount() const { return m_clickCount; }
        bool shiftKey() const { return m_shiftKey; }
        bool ctrlKey() const { return m_ctrlKey; }
        bool altKey() const { return m_altKey; }
        bool metaKey() const { return m_metaKey; }
        unsigned modifierFlags() const { return m_modifierFlags; }
        
        //time in seconds
        double timestamp() const { return m_timestamp; }

#if PLATFORM(MAC)
        PlatformMouseEvent(GSEventRef);
#endif
#if PLATFORM(WIN)
        PlatformMouseEvent(HWND, UINT, WPARAM, LPARAM, bool activatedWebView = false);
        void setClickCount(int count) { m_clickCount = count; }
        bool activatedWebView() const { return m_activatedWebView; }
#endif
#if PLATFORM(GTK) 
        PlatformMouseEvent(GdkEventButton*);
        PlatformMouseEvent(GdkEventMotion*);
#endif
#if PLATFORM(QT)
        PlatformMouseEvent(QInputEvent*, int clickCount);
#endif

#if PLATFORM(WX)
        PlatformMouseEvent(const wxMouseEvent&, const wxPoint& globalPoint);
#endif


    protected:
        IntPoint m_position;
        IntPoint m_globalPosition;
        MouseButton m_button;
        MouseEventType m_eventType;
        int m_clickCount;
        bool m_shiftKey;
        bool m_ctrlKey;
        bool m_altKey;
        bool m_metaKey;
        double m_timestamp; // unit: seconds
        unsigned m_modifierFlags;
#if PLATFORM(WIN)
        bool m_activatedWebView;
#endif
    };

#if PLATFORM(MAC)
    IntPoint pointForEvent(GSEventRef event);
    IntPoint globalPointForEvent(GSEventRef event);
#endif

} // namespace WebCore

#endif // PlatformMouseEvent_h
