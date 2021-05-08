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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef UIEvent_h
#define UIEvent_h

#include "AtomicString.h"
#include "DOMWindow.h"
#include "Event.h"

namespace WebCore {

    typedef DOMWindow AbstractView;

    class UIEvent : public Event {
    public:
        UIEvent();
        UIEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view, int detail);

        void initUIEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view, int detail);

        AbstractView* view() const { return m_view.get(); }
        int detail() const { return m_detail; }
        
        virtual bool isUIEvent() const;

        virtual int keyCode() const;
        virtual int charCode() const;

        virtual int layerX() const;
        virtual int layerY() const;

        virtual int pageX() const;
        virtual int pageY() const;

        virtual int which() const;

    private:
        RefPtr<AbstractView> m_view;
        int m_detail;
    };

} // namespace WebCore

#endif // UIEvent_h
