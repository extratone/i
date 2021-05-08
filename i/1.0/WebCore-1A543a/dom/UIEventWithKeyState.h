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

#ifndef UIEventWithKeyState_h
#define UIEventWithKeyState_h

#include "AtomicString.h"
#include "DOMWindow.h"
#include "UIEvent.h"

namespace WebCore {
    
    typedef DOMWindow AbstractView;

    class UIEventWithKeyState : public UIEvent {
    public:
        UIEventWithKeyState()
            : m_ctrlKey(false)
            , m_altKey(false)
            , m_shiftKey(false)
            , m_metaKey(false)
        {
        }
        
        UIEventWithKeyState(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
                            int detail, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
            : UIEvent(type, canBubble, cancelable, view, detail)
            , m_ctrlKey(ctrlKey)
            , m_altKey(altKey)
            , m_shiftKey(shiftKey)
            , m_metaKey(metaKey)
        {
        }

        bool ctrlKey() const { return m_ctrlKey; }
        bool shiftKey() const { return m_shiftKey; }
        bool altKey() const { return m_altKey; }
        bool metaKey() const { return m_metaKey; }

    protected:
        // Expose these so init functions can set them.
        bool m_ctrlKey : 1;
        bool m_altKey : 1;
        bool m_shiftKey : 1;
        bool m_metaKey : 1;
    };

} // namespace WebCore

#endif // UIEventWithKeyState_h
