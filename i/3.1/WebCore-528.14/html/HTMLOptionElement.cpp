/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "config.h"
#include "HTMLOptionElement.h"

#include "CSSStyleSelector.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "RenderMenuList.h"
#include "Text.h"
#include "NodeRenderStyle.h"
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

using namespace HTMLNames;

HTMLOptionElement::HTMLOptionElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* f)
    : HTMLFormControlElement(tagName, doc, f)
    , m_data(this)
    , m_style(0)
{
    ASSERT(hasTagName(optionTag));
}

bool HTMLOptionElement::checkDTD(const Node* newChild)
{
    return newChild->isTextNode() || newChild->hasTagName(scriptTag);
}

void HTMLOptionElement::attach()
{
    if (parentNode()->renderStyle())
        setRenderStyle(styleForRenderer());
    HTMLFormControlElement::attach();
}

void HTMLOptionElement::detach()
{
    m_style.clear();
    HTMLFormControlElement::detach();
}

bool HTMLOptionElement::isFocusable() const
{
    return HTMLElement::isFocusable();
}

const AtomicString& HTMLOptionElement::type() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, option, ("option"));
    return option;
}

String HTMLOptionElement::text() const
{
    return OptionElement::collectOptionText(m_data, document());
}

void HTMLOptionElement::setText(const String &text, ExceptionCode& ec)
{
    // Handle the common special case where there's exactly 1 child node, and it's a text node.
    Node* child = firstChild();
    if (child && child->isTextNode() && !child->nextSibling()) {
        static_cast<Text *>(child)->setData(text, ec);
        return;
    }

    removeChildren();
    appendChild(new Text(document(), text), ec);
}

void HTMLOptionElement::accessKeyAction(bool)
{
    HTMLSelectElement* select = ownerSelectElement();
    if (select)
        select->accessKeySetSelectedIndex(index());
}

int HTMLOptionElement::index() const
{
    // Let's do this dynamically. Might be a bit slow, but we're sure
    // we won't forget to update a member variable in some cases...
    HTMLSelectElement* select = ownerSelectElement();
    if (select) {
        const Vector<HTMLElement*>& items = select->listItems();
        int l = items.size();
        int optionIndex = 0;
        for(int i = 0; i < l; i++) {
            if (items[i]->hasLocalName(optionTag)) {
                if (static_cast<HTMLOptionElement*>(items[i]) == this)
                    return optionIndex;
                optionIndex++;
            }
        }
    }
    return 0;
}

void HTMLOptionElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == selectedAttr)
        m_data.setSelected(!attr->isNull());
    else if (attr->name() == valueAttr)
        m_data.setValue(attr->value());
    else if (attr->name() == labelAttr)
        m_data.setLabel(attr->value());
    else
        HTMLFormControlElement::parseMappedAttribute(attr);
}

String HTMLOptionElement::value() const
{
    return OptionElement::collectOptionValue(m_data, document());
}

void HTMLOptionElement::setValue(const String& value)
{
    setAttribute(valueAttr, value);
}

bool HTMLOptionElement::selected() const
{
    return m_data.selected();
}

void HTMLOptionElement::setSelected(bool selected)
{
    if (m_data.selected() == selected)
        return;

    OptionElement::setSelectedState(m_data, selected);

    if (HTMLSelectElement* select = ownerSelectElement())
        select->setSelectedIndex(selected ? index() : -1, false);
}

void HTMLOptionElement::setSelectedState(bool selected)
{
    OptionElement::setSelectedState(m_data, selected);
}

void HTMLOptionElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
   HTMLSelectElement* select = ownerSelectElement();
   if (select)
       select->childrenChanged(changedByParser);
   HTMLFormControlElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

HTMLSelectElement* HTMLOptionElement::ownerSelectElement() const
{
    Node* select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();

    if (!select)
        return 0;
    
    return static_cast<HTMLSelectElement*>(select);
}

bool HTMLOptionElement::defaultSelected() const
{
    return !getAttribute(selectedAttr).isNull();
}

void HTMLOptionElement::setDefaultSelected(bool b)
{
    setAttribute(selectedAttr, b ? "" : 0);
}

String HTMLOptionElement::label() const
{
    return m_data.label();
}

void HTMLOptionElement::setLabel(const String& value)
{
    setAttribute(labelAttr, value);
}

void HTMLOptionElement::setRenderStyle(PassRefPtr<RenderStyle> newStyle)
{
    m_style = newStyle;
}

RenderStyle* HTMLOptionElement::nonRendererRenderStyle() const 
{ 
    return m_style.get(); 
}

String HTMLOptionElement::textIndentedToRespectGroupLabel() const
{
    return OptionElement::collectOptionTextRespectingGroupLabel(m_data, document());
}

bool HTMLOptionElement::disabled() const
{ 
    return HTMLFormControlElement::disabled() || (parentNode() && static_cast<HTMLFormControlElement*>(parentNode())->disabled()); 
}

void HTMLOptionElement::insertedIntoDocument()
{
    HTMLSelectElement* select;
    if (selected() && (select = ownerSelectElement()))
        select->scrollToSelection();
    
    HTMLFormControlElement::insertedIntoDocument();
}

} // namespace
