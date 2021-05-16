/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * All rights reserved.
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

#include "config.h"
#include "PlatformWheelEvent.h"

#include <gdk/gdk.h>

// GTK_CHECK_VERSION is defined in gtk/gtkversion.h
#include <gtk/gtk.h>

namespace WebCore {

// Keep this in sync with the other platform event constructors
PlatformWheelEvent::PlatformWheelEvent(GdkEventScroll* event)
{
    static const float delta = 1;

    m_deltaX = 0;
    m_deltaY = 0;

    // Docs say an upwards scroll (away from the user) has a positive delta
    switch (event->direction) {
        case GDK_SCROLL_UP:
            m_deltaY = delta;
            break;
        case GDK_SCROLL_DOWN:
            m_deltaY = -delta;
            break;
        case GDK_SCROLL_LEFT:
            m_deltaX = -delta;
            break;
        case GDK_SCROLL_RIGHT:
            m_deltaX = delta;
            break;
    }

    m_position = IntPoint((int)event->x, (int)event->y);
    m_globalPosition = IntPoint((int)event->x_root, (int)event->y_root);
    m_granularity = ScrollByLineWheelEvent;
    m_isAccepted = false;
    m_shiftKey = event->state & GDK_SHIFT_MASK;
    m_ctrlKey = event->state & GDK_CONTROL_MASK;
    m_altKey = event->state & GDK_MOD1_MASK;
#if GTK_CHECK_VERSION(2,10,0)
    m_metaKey = event->state & GDK_META_MASK;
#else
    // GDK_MOD2_MASK doesn't always mean meta so we can't use it
    m_metaKey = false;
#endif

    // FIXME: retrieve the user setting for the number of lines to scroll on each wheel event
    m_deltaX *= horizontalLineMultiplier();
    m_deltaY *= verticalLineMultiplier();
}

}
