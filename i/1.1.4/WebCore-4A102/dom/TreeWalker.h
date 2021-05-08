/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#ifndef TreeWalker_h
#define TreeWalker_h

#include "Traversal.h"
#include <wtf/RefPtr.h>

namespace WebCore {

    typedef int ExceptionCode;

    class TreeWalker : public Traversal {
    public:
        TreeWalker(Node*, unsigned whatToShow, PassRefPtr<NodeFilter>, bool expandEntityReferences);

        Node* currentNode() const { return m_current.get(); }
        void setCurrentNode(Node*, ExceptionCode&);

        Node* parentNode();
        Node* firstChild();
        Node* lastChild();
        Node* previousSibling();
        Node* nextSibling();
        Node* previousNode();
        Node* nextNode();

    private:
        // convenience for when it is known there will be no exception
        void setCurrentNode(Node*);
        bool ancestorRejected(const Node*) const;

        RefPtr<Node> m_current;
    };

} // namespace WebCore

#endif // TreeWalker_h
