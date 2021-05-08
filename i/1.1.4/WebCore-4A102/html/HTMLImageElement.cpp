/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "HTMLImageElement.h"

#include "csshelper.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "EventNames.h"
#include "HTMLDocument.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "RenderImage.h"

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLImageElement::HTMLImageElement(Document* doc, HTMLFormElement* f)
    : HTMLElement(imgTag, doc)
    , m_imageLoader(this)
    , ismap(false)
    , m_form(f)
    , m_compositeOperator(CompositeSourceOver)
{
    if (f)
        f->registerImgElement(this);
}

HTMLImageElement::HTMLImageElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
    , m_imageLoader(this)
    , ismap(false)
    , m_form(0)
    , m_compositeOperator(CompositeSourceOver)
{
}

HTMLImageElement::~HTMLImageElement()
{
    if (m_form)
        m_form->removeImgElement(this);
}

bool HTMLImageElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr ||
        attrName == heightAttr ||
        attrName == vspaceAttr ||
        attrName == hspaceAttr ||
        attrName == valignAttr) {
        result = eUniversal;
        return false;
    }
    
    if (attrName == borderAttr || attrName == alignAttr) {
        result = eReplaced; // Shared with embed and iframe elements.
        return false;
    }

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLImageElement::parseMappedAttribute(MappedAttribute *attr)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == altAttr) {
        if (renderer())
            static_cast<RenderImage*>(renderer())->updateAltText();
    } else if (attrName == srcAttr)
        m_imageLoader.updateFromElement();
    else if (attrName == widthAttr)
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    else if (attrName == heightAttr)
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    else if (attrName == borderAttr) {
        // border="noborder" -> border="0"
        if(attr->value().toInt()) {
            addCSSLength(attr, CSS_PROP_BORDER_WIDTH, attr->value());
            addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
        }
    } else if (attrName == vspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attrName == hspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attrName == alignAttr)
        addHTMLAlignment(attr);
    else if (attrName == valignAttr)
        addCSSProperty(attr, CSS_PROP_VERTICAL_ALIGN, attr->value());
    else if (attrName == usemapAttr) {
        if (attr->value().domString()[0] == '#')
            usemap = attr->value();
        else
            usemap = document()->completeURL(parseURL(attr->value()));
        m_isLink = !attr->isNull();
    } else if (attrName == ismapAttr)
        ismap = true;
    else if (attrName == onabortAttr)
        setHTMLEventListener(abortEvent, attr);
    else if (attrName == onloadAttr)
        setHTMLEventListener(loadEvent, attr);
    else if (attrName == compositeAttr) {
        if (!parseCompositeOperator(attr->value(), m_compositeOperator))
            m_compositeOperator = CompositeSourceOver;
    } else if (attrName == nameAttr) {
        String newNameAttr = attr->value();
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* doc = static_cast<HTMLDocument*>(document());
            doc->removeNamedItem(oldNameAttr);
            doc->addNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else
        HTMLElement::parseMappedAttribute(attr);
}

String HTMLImageElement::altText() const
{
    // lets figure out the alt text.. magic stuff
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    String alt = getAttribute(altAttr);
    // fall back to title attribute
    if (alt.isNull())
        alt = getAttribute(titleAttr);
    return alt;
}

RenderObject *HTMLImageElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
     if (style->contentData())
        return RenderObject::createObject(this, style);
     
     return new (arena) RenderImage(this);
}

void HTMLImageElement::attach()
{
    HTMLElement::attach();

    if (RenderImage* imageObj = static_cast<RenderImage*>(renderer()))
        imageObj->setCachedImage(m_imageLoader.image());
}

void HTMLImageElement::insertedIntoDocument()
{
    Document* doc = document();
    if (doc->isHTMLDocument())
        static_cast<HTMLDocument*>(doc)->addNamedItem(oldNameAttr);

    HTMLElement::insertedIntoDocument();
}

void HTMLImageElement::removedFromDocument()
{
    Document* doc = document();
    if (doc->isHTMLDocument())
        static_cast<HTMLDocument*>(doc)->removeNamedItem(oldNameAttr);

    HTMLElement::removedFromDocument();
}

int HTMLImageElement::width(bool ignorePendingStylesheets) const
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int width = getAttribute(widthAttr).toInt(&ok);
        if (ok)
            return width;
        
        // if the image is available, use its width
        if (m_imageLoader.image())
            return m_imageLoader.image()->imageSize().width();
    }

    Document* doc = document();
    if (ignorePendingStylesheets)
        doc->updateLayoutIgnorePendingStylesheets();
    else
        doc->updateLayout();

    return renderer() ? renderer()->contentWidth() : 0;
}

int HTMLImageElement::height(bool ignorePendingStylesheets) const
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int height = getAttribute(heightAttr).toInt(&ok);
        if (ok)
            return height;
        
        // if the image is available, use its height
        if (m_imageLoader.image())
            return m_imageLoader.image()->imageSize().height();        
    }

    Document* doc = document();
    if (ignorePendingStylesheets)
        doc->updateLayoutIgnorePendingStylesheets();
    else
        doc->updateLayout();

    return renderer() ? renderer()->contentHeight() : 0;
}

bool HTMLImageElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr || (attr->name() == usemapAttr && attr->value().domString()[0] != '#');
}

String HTMLImageElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLImageElement::setName(const String& value)
{
    setAttribute(nameAttr, value);
}

String HTMLImageElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLImageElement::setAlign(const String& value)
{
    setAttribute(alignAttr, value);
}

String HTMLImageElement::alt() const
{
    return getAttribute(altAttr);
}

void HTMLImageElement::setAlt(const String& value)
{
    setAttribute(altAttr, value);
}

int HTMLImageElement::border() const
{
    // ### return value in pixels
    return getAttribute(borderAttr).toInt();
}

void HTMLImageElement::setBorder(int value)
{
    setAttribute(borderAttr, String::number(value));
}

void HTMLImageElement::setHeight(int value)
{
    setAttribute(heightAttr, String::number(value));
}

int HTMLImageElement::hspace() const
{
    // ### return actual value
    return getAttribute(hspaceAttr).toInt();
}

void HTMLImageElement::setHspace(int value)
{
    setAttribute(hspaceAttr, String::number(value));
}

bool HTMLImageElement::isMap() const
{
    return !getAttribute(ismapAttr).isNull();
}

void HTMLImageElement::setIsMap(bool isMap)
{
    setAttribute(ismapAttr, isMap ? "" : 0);
}

String HTMLImageElement::longDesc() const
{
    return getAttribute(longdescAttr);
}

void HTMLImageElement::setLongDesc(const String& value)
{
    setAttribute(longdescAttr, value);
}

String HTMLImageElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLImageElement::setSrc(const String& value)
{
    setAttribute(srcAttr, value);
}

String HTMLImageElement::useMap() const
{
    return getAttribute(usemapAttr);
}

void HTMLImageElement::setUseMap(const String& value)
{
    setAttribute(usemapAttr, value);
}

int HTMLImageElement::vspace() const
{
    // ### return actual vspace
    return getAttribute(vspaceAttr).toInt();
}

void HTMLImageElement::setVspace(int value)
{
    setAttribute(vspaceAttr, String::number(value));
}

void HTMLImageElement::setWidth(int value)
{
    setAttribute(widthAttr, String::number(value));
}

int HTMLImageElement::x() const
{
    RenderObject *r = renderer();
    if (!r)
        return 0;
    int x, y;
    r->absolutePosition(x, y);
    return x;
}

int HTMLImageElement::y() const
{
    RenderObject *r = renderer();
    if (!r)
        return 0;
    int x, y;
    r->absolutePosition(x, y);
    return y;
}

bool HTMLImageElement::complete() const
{
    return m_imageLoader.imageComplete();
}

}
