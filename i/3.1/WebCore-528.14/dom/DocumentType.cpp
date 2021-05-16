/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "DocumentType.h"

#include "DOMImplementation.h"
#include "Document.h"
#include "NamedNodeMap.h"

namespace WebCore {

DocumentType::DocumentType(Document* document, const String& n, const String& p, const String& s)
    : Node(document)
    , m_name(n)
    , m_publicId(p)
    , m_systemId(s)
{
}

KURL DocumentType::baseURI() const
{
    return KURL();
}

String DocumentType::nodeName() const
{
    return name();
}

Node::NodeType DocumentType::nodeType() const
{
    return DOCUMENT_TYPE_NODE;
}

PassRefPtr<Node> DocumentType::cloneNode(bool /*deep*/)
{
    return new DocumentType(document(), m_name, m_publicId, m_systemId);
}

void DocumentType::insertedIntoDocument()
{
    // Our document node can be null if we were created by a DOMImplementation.  We use the parent() instead.
    ASSERT(parent() && parent()->isDocumentNode());
    if (parent() && parent()->isDocumentNode()) {
        Document* doc = static_cast<Document*>(parent());
        if (!doc->doctype())
            doc->setDocType(this);
    }
    Node::insertedIntoDocument();
}

void DocumentType::removedFromDocument()
{
    if (document() && document()->doctype() == this)
        document()->setDocType(0);
    Node::removedFromDocument();
}

}
