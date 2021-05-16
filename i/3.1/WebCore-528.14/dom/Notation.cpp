/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Apple Computer, Inc.
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
 */
#include "config.h"
#include "Notation.h"

namespace WebCore {

Notation::Notation(Document* doc) : ContainerNode(doc)
{
}

Notation::Notation(Document* doc, const String& name, const String& publicId, const String& systemId)
    : ContainerNode(doc)
    , m_name(name)
    , m_publicId(publicId)
    , m_systemId(systemId)
{
}

String Notation::nodeName() const
{
    return m_name;
}

Node::NodeType Notation::nodeType() const
{
    return NOTATION_NODE;
}

PassRefPtr<Node> Notation::cloneNode(bool /*deep*/)
{
    // Spec says cloning Notation nodes is "implementation dependent". We do not support it.
    return 0;
}

// DOM Section 1.1.1
bool Notation::childTypeAllowed(NodeType)
{
    return false;
}

} // namespace
