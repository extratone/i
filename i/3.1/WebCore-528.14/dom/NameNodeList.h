/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007m 2008 Apple Inc. All rights reserved.
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

#ifndef NameNodeList_h
#define NameNodeList_h

#include "AtomicString.h"
#include "DynamicNodeList.h"

namespace WebCore {

    class String;

    // NodeList which lists all Nodes in a Element with a given "name" attribute
    class NameNodeList : public DynamicNodeList {
    public:
        static PassRefPtr<NameNodeList> create(PassRefPtr<Node> rootNode, const String& name, Caches* caches)
        {
            return adoptRef(new NameNodeList(rootNode, name, caches));
        }

    private:
        NameNodeList(PassRefPtr<Node> rootNode, const String& name, Caches*);

        virtual bool nodeMatches(Element*) const;

        AtomicString m_nodeName;
    };

} // namespace WebCore

#endif // NameNodeList_h
