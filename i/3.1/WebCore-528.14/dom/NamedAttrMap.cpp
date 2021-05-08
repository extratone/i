/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
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
#include "NamedAttrMap.h"

#include "Document.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

static inline bool shouldIgnoreAttributeCase(const Element* e)
{
    return e && e->document()->isHTMLDocument() && e->isHTMLElement();
}

NamedAttrMap::~NamedAttrMap()
{
    NamedAttrMap::clearAttributes(); // virtual function, qualify to be explicit and slightly faster
}

bool NamedAttrMap::isMappedAttributeMap() const
{
    return false;
}

PassRefPtr<Node> NamedAttrMap::getNamedItem(const String& name) const
{
    Attribute* a = getAttributeItem(name, shouldIgnoreAttributeCase(m_element));
    if (!a)
        return 0;
    
    return a->createAttrIfNeeded(m_element);
}

PassRefPtr<Node> NamedAttrMap::getNamedItemNS(const String& namespaceURI, const String& localName) const
{
    return getNamedItem(QualifiedName(nullAtom, localName, namespaceURI));
}

PassRefPtr<Node> NamedAttrMap::removeNamedItem(const String& name, ExceptionCode& ec)
{
    Attribute* a = getAttributeItem(name, shouldIgnoreAttributeCase(m_element));
    if (!a) {
        ec = NOT_FOUND_ERR;
        return 0;
    }
    
    return removeNamedItem(a->name(), ec);
}

PassRefPtr<Node> NamedAttrMap::removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode& ec)
{
    return removeNamedItem(QualifiedName(nullAtom, localName, namespaceURI), ec);
}

PassRefPtr<Node> NamedAttrMap::getNamedItem(const QualifiedName& name) const
{
    Attribute* a = getAttributeItem(name);
    if (!a)
        return 0;

    return a->createAttrIfNeeded(m_element);
}

PassRefPtr<Node> NamedAttrMap::setNamedItem(Node* arg, ExceptionCode& ec)
{
    if (!m_element || !arg) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    // WRONG_DOCUMENT_ERR: Raised if arg was created from a different document than the one that created this map.
    if (arg->document() != m_element->document()) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    // Not mentioned in spec: throw a HIERARCHY_REQUEST_ERROR if the user passes in a non-attribute node
    if (!arg->isAttributeNode()) {
        ec = HIERARCHY_REQUEST_ERR;
        return 0;
    }
    Attr *attr = static_cast<Attr*>(arg);

    Attribute* a = attr->attr();
    Attribute* old = getAttributeItem(a->name());
    if (old == a)
        return RefPtr<Node>(arg); // we know about it already

    // INUSE_ATTRIBUTE_ERR: Raised if arg is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attr->ownerElement()) {
        ec = INUSE_ATTRIBUTE_ERR;
        return 0;
    }

    if (a->name() == idAttr)
        m_element->updateId(old ? old->value() : nullAtom, a->value());

    // ### slightly inefficient - resizes attribute array twice.
    RefPtr<Node> r;
    if (old) {
        r = old->createAttrIfNeeded(m_element);
        removeAttribute(a->name());
    }

    addAttribute(a);
    return r.release();
}

// The DOM2 spec doesn't say that removeAttribute[NS] throws NOT_FOUND_ERR
// if the attribute is not found, but at this level we have to throw NOT_FOUND_ERR
// because of removeNamedItem, removeNamedItemNS, and removeAttributeNode.
PassRefPtr<Node> NamedAttrMap::removeNamedItem(const QualifiedName& name, ExceptionCode& ec)
{
    Attribute* a = getAttributeItem(name);
    if (!a) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    RefPtr<Node> r = a->createAttrIfNeeded(m_element);

    if (name == idAttr)
        m_element->updateId(a->value(), nullAtom);

    removeAttribute(name);
    return r.release();
}

PassRefPtr<Node> NamedAttrMap::item (unsigned index) const
{
    if (index >= length())
        return 0;

    return m_attributes[index]->createAttrIfNeeded(m_element);
}

// We use a boolean parameter instead of calling shouldIgnoreAttributeCase so that the caller
// can tune the behaviour (hasAttribute is case sensitive whereas getAttribute is not).
Attribute* NamedAttrMap::getAttributeItem(const String& name, bool shouldIgnoreAttributeCase) const
{
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        if (!m_attributes[i]->name().hasPrefix() && 
            m_attributes[i]->name().localName() == name)
                return m_attributes[i].get();

        if (shouldIgnoreAttributeCase ? equalIgnoringCase(m_attributes[i]->name().toString(), name) : name == m_attributes[i]->name().toString())
            return m_attributes[i].get();
    }
    return 0;
}

Attribute* NamedAttrMap::getAttributeItem(const QualifiedName& name) const
{
    unsigned len = length();
    for (unsigned i = 0; i < len; ++i) {
        if (m_attributes[i]->name().matches(name))
            return m_attributes[i].get();
    }
    return 0;
}

void NamedAttrMap::clearAttributes()
{
    unsigned len = length();
    for (unsigned i = 0; i < len; i++)
        if (Attr* attr = m_attributes[i]->attr())
            attr->m_element = 0;

    m_attributes.clear();
}

void NamedAttrMap::detachFromElement()
{
    // we allow a NamedAttrMap w/o an element in case someone still has a reference
    // to if after the element gets deleted - but the map is now invalid
    m_element = 0;
    clearAttributes();
}

void NamedAttrMap::setAttributes(const NamedAttrMap& other)
{
    // clone all attributes in the other map, but attach to our element
    if (!m_element)
        return;

    // If assigning the map changes the id attribute, we need to call
    // updateId.
    Attribute *oldId = getAttributeItem(idAttr);
    Attribute *newId = other.getAttributeItem(idAttr);

    if (oldId || newId)
        m_element->updateId(oldId ? oldId->value() : nullAtom, newId ? newId->value() : nullAtom);

    clearAttributes();
    unsigned newLength = other.length();
    m_attributes.resize(newLength);
    for (unsigned i = 0; i < newLength; i++)
        m_attributes[i] = other.m_attributes[i]->clone();

    // FIXME: This is wasteful.  The class list could be preserved on a copy, and we
    // wouldn't have to waste time reparsing the attribute.
    // The derived class, HTMLNamedAttrMap, which manages a parsed class list for the CLASS attribute,
    // will update its member variable when parse attribute is called.
    for (unsigned i = 0; i < newLength; i++)
        m_element->attributeChanged(m_attributes[i].get(), true);
}

void NamedAttrMap::addAttribute(PassRefPtr<Attribute> prpAttribute)
{
    RefPtr<Attribute> attribute = prpAttribute;
    
    // Add the attribute to the list
    m_attributes.append(attribute);

    if (Attr* attr = attribute->attr())
        attr->m_element = m_element;

    // Notify the element that the attribute has been added, and dispatch appropriate mutation events
    // Note that element may be null here if we are called from insertAttr() during parsing
    if (m_element) {
        m_element->attributeChanged(attribute.get());
        // Because of our updateStyleAttribute() style modification events are never sent at the right time, so don't bother sending them.
        if (attribute->name() != styleAttr) {
            m_element->dispatchAttrAdditionEvent(attribute.get());
            m_element->dispatchSubtreeModifiedEvent();
        }
    }
}

void NamedAttrMap::removeAttribute(const QualifiedName& name)
{
    unsigned len = length();
    unsigned index = len + 1;
    for (unsigned i = 0; i < len; ++i)
        if (m_attributes[i]->name().matches(name)) {
            index = i;
            break;
        }

    if (index >= len)
        return;

    // Remove the attribute from the list
    RefPtr<Attribute> attr = m_attributes[index].get();
    if (Attr* a = m_attributes[index]->attr())
        a->m_element = 0;

    m_attributes.remove(index);

    // Notify the element that the attribute has been removed
    // dispatch appropriate mutation events
    if (m_element && !attr->m_value.isNull()) {
        AtomicString value = attr->m_value;
        attr->m_value = nullAtom;
        m_element->attributeChanged(attr.get());
        attr->m_value = value;
    }
    if (m_element) {
        m_element->dispatchAttrRemovalEvent(attr.get());
        m_element->dispatchSubtreeModifiedEvent();
    }
}

bool NamedAttrMap::mapsEquivalent(const NamedAttrMap* otherMap) const
{
    if (!otherMap)
        return false;
    
    unsigned len = length();
    if (len != otherMap->length())
        return false;
    
    for (unsigned i = 0; i < len; i++) {
        Attribute *attr = attributeItem(i);
        Attribute *otherAttr = otherMap->getAttributeItem(attr->name());
            
        if (!otherAttr || attr->value() != otherAttr->value())
            return false;
    }
    
    return true;
}

}
