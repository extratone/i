/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "GraphicsServices/GraphicsServices.h"
#import "WKWindow.h"
#import "config.h"
#import "PlatformMouseEvent.h"
#import "Screen.h"

#import <GraphicsServices/GraphicsServices.h>

namespace WebCore {

const PlatformMouseEvent::CurrentEventTag PlatformMouseEvent::currentEvent = {};

    static MouseButton mouseButtonForEvent(GSEventRef event)
    {
        switch (GSEventGetType (event)) {
            case kGSEventLeftMouseDown:
            case kGSEventLeftMouseUp:
            case kGSEventLeftMouseDragged:
            default:
                return LeftButton;
        }
    }
    
    static IntPoint positionForEvent(GSEventRef event)
    {
        if (GSEventIsMouseEventType (event) || GSEventGetType (event) == kGSEventScrollWheel)
            return IntPoint (GSEventGetLocationInWindow (event));
        return IntPoint ();
    }

static IntPoint globalPositionForEvent(GSEventRef event)
{
    if (GSEventIsMouseEventType(event) || GSEventGetType(event) == kGSEventScrollWheel)
        return IntPoint(GSEventGetLocationInWindow(event));
    else
        return IntPoint();
}
    
static int clickCountForEvent(GSEventRef event)
{
    if (GSEventIsMouseEventType (event)) {
        return GSEventGetClickCount (event);
    }
    return 0;
}
    
    PlatformMouseEvent::PlatformMouseEvent(GSEventRef event)
    : m_position(positionForEvent(event))
    , m_globalPosition(globalPositionForEvent(event))
    , m_button(mouseButtonForEvent(event))
    , m_clickCount(clickCountForEvent(event))
    , m_shiftKey(GSEventGetModifierFlags(event) & kGSEventFlagMaskShift)
    , m_ctrlKey(GSEventGetModifierFlags(event) & kGSEventFlagMaskControl)
    , m_altKey(GSEventGetModifierFlags(event) & kGSEventFlagMaskAlternate)
    , m_metaKey(GSEventGetModifierFlags(event) & kGSEventFlagMaskCommand)
{
}

PlatformMouseEvent::PlatformMouseEvent(const CurrentEventTag&)
    : m_button(LeftButton), m_clickCount(0), m_shiftKey(false), m_ctrlKey(false), m_altKey(false), m_metaKey(false)
{
        GSEventRef event = WKEventGetCurrentEvent();
        if (event) {
            m_position = positionForEvent(event);
            m_globalPosition = globalPositionForEvent(event);
            m_button = mouseButtonForEvent(event);
            m_clickCount = clickCountForEvent(event);
            m_shiftKey = GSEventGetModifierFlags(event) & kGSEventFlagMaskShift;
            m_ctrlKey = GSEventGetModifierFlags(event) & kGSEventFlagMaskControl;
            m_altKey = GSEventGetModifierFlags(event) & kGSEventFlagMaskAlternate;
            m_metaKey = GSEventGetModifierFlags(event) & kGSEventFlagMaskCommand;
        }
}

}
