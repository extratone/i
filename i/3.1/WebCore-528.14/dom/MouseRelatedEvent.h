/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef MouseRelatedEvent_h
#define MouseRelatedEvent_h

#include "UIEventWithKeyState.h"

namespace WebCore {

    // Internal only: Helper class for what's common between mouse and wheel events.
    class MouseRelatedEvent : public UIEventWithKeyState {
    public:
        int screenX() const { return m_screenX; }
        int screenY() const { return m_screenY; }
        int clientX() const { return m_clientX; }
        int clientY() const { return m_clientY; }
        int layerX() const { return m_layerX; }
        int layerY() const { return m_layerY; }
        int offsetX() const { return m_offsetX; }
        int offsetY() const { return m_offsetY; }
        bool isSimulated() const { return m_isSimulated; }
        virtual int pageX() const;
        virtual int pageY() const;
        int x() const;
        int y() const;
    
    protected:
        MouseRelatedEvent();
        MouseRelatedEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<AbstractView>,
                          int detail, int screenX, int screenY, int pageX, int pageY,
                          bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool isSimulated = false);

        void initCoordinates();
        void initCoordinates(int clientX, int clientY);
        virtual void receivedTarget();
        
        // Expose these so MouseEvent::initMouseEvent can set them.
        int m_screenX;
        int m_screenY;
        int m_clientX;
        int m_clientY;

    private:
        int m_pageX;
        int m_pageY;
        int m_layerX;
        int m_layerY;
        int m_offsetX;
        int m_offsetY;
        bool m_isSimulated;
    };

} // namespace WebCore

#endif // MouseRelatedEvent_h
