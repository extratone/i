/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef Attr_h
#define Attr_h

#include "ContainerNode.h"
#include "Attribute.h"

namespace WebCore {

// Attr can have Text and EntityReference children
// therefore it has to be a fullblown Node. The plan
// is to dynamically allocate a textchild and store the
// resulting nodevalue in the Attribute upon
// destruction. however, this is not yet implemented.

class Attr : public ContainerNode {
    friend class NamedAttrMap;
public:
    Attr(Element*, Document*, PassRefPtr<Attribute>);
    ~Attr();

    // Call this after calling the constructor so the
    // Attr node isn't floating when we append the text node.
    void createTextChild();
    
    // DOM methods & attributes for Attr
    String name() const { return qualifiedName().toString(); }
    bool specified() const { return m_specified; }
    Element* ownerElement() const { return m_element; }

    String value() const { return m_attribute->value(); }
    void setValue(const String&, ExceptionCode&);

    // DOM methods overridden from parent classes
    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    const AtomicString& localName() const;
    const AtomicString& namespaceURI() const;
    const AtomicString& prefix() const;
    virtual void setPrefix(const AtomicString&, ExceptionCode&);

    virtual String nodeValue() const;
    virtual void setNodeValue(const String&, ExceptionCode&);
    virtual PassRefPtr<Node> cloneNode(bool deep);

    // Other methods (not part of DOM)
    virtual bool isAttributeNode() const { return true; }
    virtual bool childTypeAllowed(NodeType);

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    Attribute* attr() const { return m_attribute.get(); }
    const QualifiedName& qualifiedName() const { return m_attribute->name(); }

    // An extension to get presentational information for attributes.
    CSSStyleDeclaration* style() { return m_attribute->style(); }

    void setSpecified(bool specified) { m_specified = specified; }

private:
    virtual const AtomicString& virtualPrefix() const { return prefix(); }
    virtual const AtomicString& virtualLocalName() const { return localName(); }
    virtual const AtomicString& virtualNamespaceURI() const { return namespaceURI(); }

    Element* m_element;
    RefPtr<Attribute> m_attribute;
    unsigned m_ignoreChildrenChanged : 31;
    bool m_specified : 1;
};

} // namespace WebCore

#endif // Attr_h
