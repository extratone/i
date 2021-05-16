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

#ifndef MutationEvent_h
#define MutationEvent_h

#include "Event.h"
#include "Node.h"

namespace WebCore {

    class MutationEvent : public Event {
    public:
        enum attrChangeType {
            MODIFICATION    = 1,
            ADDITION        = 2,
            REMOVAL         = 3
        };

        MutationEvent();
        MutationEvent(const AtomicString& type, bool canBubble, bool cancelable, Node* relatedNode,
                      const String& prevValue, const String& newValue,
                      const String& attrName, unsigned short attrChange);

        void initMutationEvent(const AtomicString& type, bool canBubble, bool cancelable, Node* relatedNode,
                               const String& prevValue, const String& newValue,
                               const String& attrName, unsigned short attrChange);

        Node* relatedNode() const { return m_relatedNode.get(); }
        String prevValue() const { return m_prevValue.get(); }
        String newValue() const { return m_newValue.get(); }
        String attrName() const { return m_attrName.get(); }
        unsigned short attrChange() const { return m_attrChange; }

        virtual bool isMutationEvent() const;

    private:
        RefPtr<Node> m_relatedNode;
        RefPtr<StringImpl> m_prevValue;
        RefPtr<StringImpl> m_newValue;
        RefPtr<StringImpl> m_attrName;
        unsigned short m_attrChange;
    };

} // namespace WebCore

#endif // MutationEvent_h
