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

#ifndef RegisteredEventListener_h
#define RegisteredEventListener_h

#include "AtomicString.h"
#include "Shared.h"

namespace WebCore {

    class EventListener;

    class RegisteredEventListener : public Shared<RegisteredEventListener> {
    public:
        RegisteredEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);

        const AtomicString& eventType() const { return m_eventType; }
        EventListener* listener() const { return m_listener.get(); }
        bool useCapture() const { return m_useCapture; }
        
        bool removed() const { return m_removed; }
        void setRemoved(bool removed) { m_removed = removed; }
    
    private:
        AtomicString m_eventType;
        RefPtr<EventListener> m_listener;
        bool m_useCapture;
        bool m_removed;
    };


    bool operator==(const RegisteredEventListener&, const RegisteredEventListener&);
    inline bool operator!=(const RegisteredEventListener& a, const RegisteredEventListener& b) { return !(a == b); }

} // namespace WebCore

#endif // RegisteredEventListener_h
