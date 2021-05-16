/*
 * This file is part of the DOM implementation for KDE.
 *
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "HTMLOptGroupElement.h"

#include "Document.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "RenderMenuList.h"
#include "cssstyleselector.h"

namespace WebCore {

using namespace HTMLNames;

HTMLOptGroupElement::HTMLOptGroupElement(Document* doc, HTMLFormElement* f)
    : HTMLGenericFormElement(optgroupTag, doc, f)
    , m_style(0)
{
}

bool HTMLOptGroupElement::isFocusable() const
{
    return false;
}

const AtomicString& HTMLOptGroupElement::type() const
{
    static const AtomicString optgroup("optgroup");
    return optgroup;
}

bool HTMLOptGroupElement::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode& ec)
{
    bool result = HTMLGenericFormElement::insertBefore(newChild, refChild, ec);
    if (result)
        recalcSelectOptions();
    return result;
}

bool HTMLOptGroupElement::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode& ec)
{
    bool result = HTMLGenericFormElement::replaceChild(newChild, oldChild, ec);
    if (result)
        recalcSelectOptions();
    return result;
}

bool HTMLOptGroupElement::removeChild(Node* oldChild, ExceptionCode& ec)
{
    bool result = HTMLGenericFormElement::removeChild(oldChild, ec);
    if (result)
        recalcSelectOptions();
    return result;
}

bool HTMLOptGroupElement::appendChild(PassRefPtr<Node> newChild, ExceptionCode& ec)
{
    bool result = HTMLGenericFormElement::appendChild(newChild, ec);
    if (result)
        recalcSelectOptions();
    return result;
}

ContainerNode* HTMLOptGroupElement::addChild(PassRefPtr<Node> newChild)
{
    ContainerNode* result = HTMLGenericFormElement::addChild(newChild);
    if (result)
        recalcSelectOptions();
    return result;
}

void HTMLOptGroupElement::parseMappedAttribute(MappedAttribute *attr)
{
    HTMLGenericFormElement::parseMappedAttribute(attr);
    recalcSelectOptions();
}

void HTMLOptGroupElement::recalcSelectOptions()
{
    Node *select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();
    if (select)
        static_cast<HTMLSelectElement*>(select)->setRecalcListItems();
}

String HTMLOptGroupElement::label() const
{
    return getAttribute(labelAttr);
}

void HTMLOptGroupElement::setLabel(const String &value)
{
    setAttribute(labelAttr, value);
}

bool HTMLOptGroupElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(HTMLNames::optionTag) || newChild->hasTagName(HTMLNames::hrTag);
}

void HTMLOptGroupElement::attach()
{
    RenderStyle* style = styleForRenderer(0);
    setRenderStyle(style);
    style->deref(document()->renderArena());
    HTMLGenericFormElement::attach();
}

void HTMLOptGroupElement::detach()
{
    if (m_style) {
        m_style->deref(document()->renderArena());
        m_style = 0;
    }
    HTMLGenericFormElement::detach();
}

void HTMLOptGroupElement::setRenderStyle(RenderStyle* newStyle)
{
    RenderStyle* oldStyle = m_style;
    m_style = newStyle;
    if (newStyle)
        newStyle->ref();
    if (oldStyle)
        oldStyle->deref(document()->renderArena());
}

String HTMLOptGroupElement::groupLabelText()
{
    DeprecatedString itemText = getAttribute(labelAttr).deprecatedString();
    
    itemText.replace('\\', document()->backslashAsCurrencySymbol());
    // In WinIE, leading and trailing whitespace is ignored in options and optgroups. We match this behavior.
    itemText = itemText.stripWhiteSpace();
    // We want to collapse our whitespace too.  This will match other browsers.
    itemText = itemText.simplifyWhiteSpace();
        
    return itemText;
}
 
} // namespace
