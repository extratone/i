/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#include "HTMLObjectElement.h"

#include "CSSHelper.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "MIMETypeRegistry.h"
#include "RenderImage.h"
#include "RenderPartObject.h"
#include "RenderWidget.h"
#include "ScriptController.h"
#include "Text.h"


namespace WebCore {

using namespace HTMLNames;

HTMLObjectElement::HTMLObjectElement(const QualifiedName& tagName, Document* doc, bool createdByParser) 
    : HTMLPlugInImageElement(tagName, doc)
    , m_docNamedItem(true)
    , m_needWidgetUpdate(!createdByParser)
    , m_useFallbackContent(false)
{
    ASSERT(hasTagName(objectTag));
}

HTMLObjectElement::~HTMLObjectElement()
{
}

RenderWidget* HTMLObjectElement::renderWidgetForJSBindings() const
{
    RenderWidget* renderWidget = (renderer() && renderer()->isWidget()) ? static_cast<RenderWidget*>(renderer()) : 0;
    if (renderWidget && !renderWidget->widget()) {
        document()->updateLayoutIgnorePendingStylesheets();
        renderWidget = (renderer() && renderer()->isWidget()) ? static_cast<RenderWidget*>(renderer()) : 0;
    }
    return renderWidget;
}

void HTMLObjectElement::parseMappedAttribute(MappedAttribute *attr)
{
    String val = attr->value();
    int pos;
    if (attr->name() == typeAttr) {
        m_serviceType = val.lower();
        pos = m_serviceType.find(";");
        if (pos != -1)
          m_serviceType = m_serviceType.left(pos);
        if (renderer())
          m_needWidgetUpdate = true;
        if (!isImageType() && m_imageLoader)
          m_imageLoader.clear();
    } else if (attr->name() == dataAttr) {
        m_url = parseURL(val);
        if (renderer())
          m_needWidgetUpdate = true;
        if (renderer() && isImageType()) {
          if (!m_imageLoader)
              m_imageLoader.set(new HTMLImageLoader(this));
          m_imageLoader->updateFromElementIgnoringPreviousError();
        }
    } else if (attr->name() == classidAttr) {
        m_classId = val;
        if (renderer())
          m_needWidgetUpdate = true;
    } else if (attr->name() == onloadAttr) {
        setInlineEventListenerForTypeAndAttribute(eventNames().loadEvent, attr);
    } else if (attr->name() == nameAttr) {
        const AtomicString& newName = attr->value();
        if (isDocNamedItem() && inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
            document->removeNamedItem(m_name);
            document->addNamedItem(newName);
        }
        m_name = newName;
    } else if (attr->name() == idAttr) {
        const AtomicString& newId = attr->value();
        if (isDocNamedItem() && inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
            document->removeExtraNamedItem(m_id);
            document->addExtraNamedItem(newId);
        }
        m_id = newId;
        // also call superclass
        HTMLPlugInElement::parseMappedAttribute(attr);
    } else
        HTMLPlugInElement::parseMappedAttribute(attr);
}

bool HTMLObjectElement::rendererIsNeeded(RenderStyle* style)
{
    Frame* frame = document()->frame();
    if (!frame)
        return false;
    
    return HTMLPlugInElement::rendererIsNeeded(style);
}

RenderObject *HTMLObjectElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    if (m_useFallbackContent)
        return RenderObject::createObject(this, style);
    if (isImageType())
        return new (arena) RenderImage(this);
    return new (arena) RenderPartObject(this);
}

void HTMLObjectElement::attach()
{
    bool isImage = isImageType();

    if (!isImage)
        queuePostAttachCallback(&HTMLPlugInElement::updateWidgetCallback, this);

    HTMLPlugInElement::attach();

    if (isImage && renderer() && !m_useFallbackContent) {
        if (!m_imageLoader)
            m_imageLoader.set(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
        // updateForElement() may have changed us to use fallback content and called detach() and attach().
        if (m_useFallbackContent)
            return;

        if (renderer())
            static_cast<RenderImage*>(renderer())->setCachedImage(m_imageLoader->image());
    }
}

void HTMLObjectElement::updateWidget()
{
    document()->updateRendering();
    if (m_needWidgetUpdate && renderer() && !m_useFallbackContent && !isImageType())
        static_cast<RenderPartObject*>(renderer())->updateWidget(true);
}

void HTMLObjectElement::finishParsingChildren()
{
    HTMLPlugInElement::finishParsingChildren();
    if (!m_useFallbackContent) {
        m_needWidgetUpdate = true;
        if (inDocument())
            setChanged();
    }
}

void HTMLObjectElement::detach()
{
    if (attached() && renderer() && !m_useFallbackContent)
        // Update the widget the next time we attach (detaching destroys the plugin).
        m_needWidgetUpdate = true;
    HTMLPlugInElement::detach();
}

void HTMLObjectElement::insertedIntoDocument()
{
    if (isDocNamedItem() && document()->isHTMLDocument()) {
        HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
        document->addNamedItem(m_name);
        document->addExtraNamedItem(m_id);
    }

    HTMLPlugInElement::insertedIntoDocument();
}

void HTMLObjectElement::removedFromDocument()
{
    if (isDocNamedItem() && document()->isHTMLDocument()) {
        HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
        document->removeNamedItem(m_name);
        document->removeExtraNamedItem(m_id);
    }

    HTMLPlugInElement::removedFromDocument();
}

void HTMLObjectElement::recalcStyle(StyleChange ch)
{
    if (!m_useFallbackContent && m_needWidgetUpdate && renderer() && !isImageType()) {
        detach();
        attach();
    }
    HTMLPlugInElement::recalcStyle(ch);
}

void HTMLObjectElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    updateDocNamedItem();
    if (inDocument() && !m_useFallbackContent) {
        m_needWidgetUpdate = true;
        setChanged();
    }
    HTMLPlugInElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

bool HTMLObjectElement::isURLAttribute(Attribute *attr) const
{
    return (attr->name() == dataAttr || (attr->name() == usemapAttr && attr->value().string()[0] != '#'));
}

const QualifiedName& HTMLObjectElement::imageSourceAttributeName() const
{
    return dataAttr;
}

void HTMLObjectElement::renderFallbackContent()
{
    if (m_useFallbackContent)
        return;

    // Before we give up and use fallback content, check to see if this is a MIME type issue.
    if (m_imageLoader && m_imageLoader->image()) {
        m_serviceType = m_imageLoader->image()->response().mimeType();
        if (!isImageType()) {
            // If we don't think we have an image type anymore, then ditch the image loader.
            m_imageLoader.clear();        
            detach();
            attach();
            return;
        }
    }

    // Mark ourselves as using the fallback content.
    m_useFallbackContent = true;

    // Now do a detach and reattach.    
    // FIXME: Style gets recalculated which is suboptimal.
    detach();
    attach();
}

void HTMLObjectElement::updateDocNamedItem()
{
    // The rule is "<object> elements with no children other than
    // <param> elements, unknown elements and whitespace can be
    // found by name in a document, and other <object> elements cannot."
    bool wasNamedItem = m_docNamedItem;
    bool isNamedItem = true;
    Node* child = firstChild();
    while (child && isNamedItem) {
        if (child->isElementNode()) {
            Element* element = static_cast<Element*>(child);
            if (HTMLElement::isRecognizedTagName(element->tagQName()) && !element->hasTagName(paramTag))
                isNamedItem = false;
        } else if (child->isTextNode()) {
            if (!static_cast<Text*>(child)->containsOnlyWhitespace())
                isNamedItem = false;
        } else
            isNamedItem = false;
        child = child->nextSibling();
    }
    if (isNamedItem != wasNamedItem && document()->isHTMLDocument()) {
        HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
        if (isNamedItem) {
            document->addNamedItem(m_name);
            document->addExtraNamedItem(m_id);
        } else {
            document->removeNamedItem(m_name);
            document->removeExtraNamedItem(m_id);
        }
    }
    m_docNamedItem = isNamedItem;
}

String HTMLObjectElement::code() const
{
    return getAttribute(codeAttr);
}

void HTMLObjectElement::setCode(const String& value)
{
    setAttribute(codeAttr, value);
}

String HTMLObjectElement::archive() const
{
    return getAttribute(archiveAttr);
}

void HTMLObjectElement::setArchive(const String& value)
{
    setAttribute(archiveAttr, value);
}

String HTMLObjectElement::border() const
{
    return getAttribute(borderAttr);
}

void HTMLObjectElement::setBorder(const String& value)
{
    setAttribute(borderAttr, value);
}

String HTMLObjectElement::codeBase() const
{
    return getAttribute(codebaseAttr);
}

void HTMLObjectElement::setCodeBase(const String& value)
{
    setAttribute(codebaseAttr, value);
}

String HTMLObjectElement::codeType() const
{
    return getAttribute(codetypeAttr);
}

void HTMLObjectElement::setCodeType(const String& value)
{
    setAttribute(codetypeAttr, value);
}

KURL HTMLObjectElement::data() const
{
    return document()->completeURL(getAttribute(dataAttr));
}

void HTMLObjectElement::setData(const String& value)
{
    setAttribute(dataAttr, value);
}

bool HTMLObjectElement::declare() const
{
    return !getAttribute(declareAttr).isNull();
}

void HTMLObjectElement::setDeclare(bool declare)
{
    setAttribute(declareAttr, declare ? "" : 0);
}

int HTMLObjectElement::hspace() const
{
    return getAttribute(hspaceAttr).toInt();
}

void HTMLObjectElement::setHspace(int value)
{
    setAttribute(hspaceAttr, String::number(value));
}

String HTMLObjectElement::standby() const
{
    return getAttribute(standbyAttr);
}

void HTMLObjectElement::setStandby(const String& value)
{
    setAttribute(standbyAttr, value);
}

String HTMLObjectElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLObjectElement::setType(const String& value)
{
    setAttribute(typeAttr, value);
}

String HTMLObjectElement::useMap() const
{
    return getAttribute(usemapAttr);
}

void HTMLObjectElement::setUseMap(const String& value)
{
    setAttribute(usemapAttr, value);
}

int HTMLObjectElement::vspace() const
{
    return getAttribute(vspaceAttr).toInt();
}

void HTMLObjectElement::setVspace(int value)
{
    setAttribute(vspaceAttr, String::number(value));
}

bool HTMLObjectElement::containsJavaApplet() const
{
    if (MIMETypeRegistry::isJavaAppletMIMEType(type()))
        return true;
        
    Node* child = firstChild();
    while (child) {
        if (child->isElementNode()) {
            Element* e = static_cast<Element*>(child);
            if (e->hasTagName(paramTag) && equalIgnoringCase(e->getAttribute(nameAttr), "type") && MIMETypeRegistry::isJavaAppletMIMEType(e->getAttribute(valueAttr).string()))
                return true;
            else if (e->hasTagName(objectTag) && static_cast<HTMLObjectElement*>(e)->containsJavaApplet())
                return true;
            else if (e->hasTagName(appletTag))
                return true;
        }
        child = child->nextSibling();
    }
    
    return false;
}

void HTMLObjectElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLPlugInImageElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, data());
    if (useMap().startsWith("#"))
        addSubresourceURL(urls, document()->completeURL(useMap()));
}

}
