/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 */
#include "config.h"
#include "HTMLMetaElement.h"

#include "Document.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLMetaElement::HTMLMetaElement(Document* doc)
    : HTMLElement(metaTag, doc)
{
}

HTMLMetaElement::~HTMLMetaElement()
{
}

void HTMLMetaElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == http_equivAttr) {
        m_equiv = attr->value();
        process();
    } else if (attr->name() == contentAttr) {
        m_content = attr->value();
        process();
    } else if (attr->name() == nameAttr) {
        if (equalIgnoringCase(attr->value(), "viewport"))
            process();
        // Do nothing.
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLMetaElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    process();
}

void HTMLMetaElement::process()
{
    if (inDocument() && equalIgnoringCase(name(), "viewport") && !m_content.isNull())
        document()->processViewport(m_content);
    if (inDocument() && equalIgnoringCase(name(), "format-detection") && !m_content.isNull())
        document()->processFormatDetection(m_content);
    // Get the document to process the tag, but only if we're actually part of DOM tree (changing a meta tag while
    // it's not in the tree shouldn't have any effect on the document)
    if (inDocument() && !m_equiv.isNull() && !m_content.isNull())
        document()->processHttpEquiv(m_equiv, m_content);
}

String HTMLMetaElement::content() const
{
    return getAttribute(contentAttr);
}

void HTMLMetaElement::setContent(const String& value)
{
    setAttribute(contentAttr, value);
}

String HTMLMetaElement::httpEquiv() const
{
    return getAttribute(http_equivAttr);
}

void HTMLMetaElement::setHttpEquiv(const String& value)
{
    setAttribute(http_equivAttr, value);
}

String HTMLMetaElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLMetaElement::setName(const String& value)
{
    setAttribute(nameAttr, value);
}

String HTMLMetaElement::scheme() const
{
    return getAttribute(schemeAttr);
}

void HTMLMetaElement::setScheme(const String &value)
{
    setAttribute(schemeAttr, value);
}

}
