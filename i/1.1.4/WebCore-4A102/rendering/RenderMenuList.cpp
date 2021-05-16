/**
 * This file is part of the select element renderer in WebCore.
 *
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderMenuList.h"

#include "Document.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "CSSStyleSelector.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "RenderBR.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "HTMLDivElement.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderMenuList::RenderMenuList(HTMLSelectElement* element)
    : RenderFlexibleBox(element)
    , m_buttonText(0)
    , m_innerBlock(0)
    , m_optionsChanged(true)
    , m_optionsWidth(0)
    , m_popupIsVisible(false)
{
}

RenderMenuList::~RenderMenuList()
{
}

void RenderMenuList::createInnerBlock()
{
    if (m_innerBlock) {
        ASSERT(firstChild() == m_innerBlock);
        ASSERT(!m_innerBlock->nextSibling());
        return;
    }

    // Create an anonymous block.
    ASSERT(!firstChild());
    m_innerBlock = createAnonymousBlock();
    adjustInnerStyle();
    RenderFlexibleBox::addChild(m_innerBlock);
}

void RenderMenuList::adjustInnerStyle()
{
    m_innerBlock->style()->setBoxFlex(1.0f);
    
    m_innerBlock->style()->setPaddingLeft(Length(theme()->popupInternalPaddingLeft(style()), Fixed));
    m_innerBlock->style()->setPaddingRight(Length(theme()->popupInternalPaddingRight(style()), Fixed));
    m_innerBlock->style()->setPaddingTop(Length(theme()->popupInternalPaddingTop(style()), Fixed));
    m_innerBlock->style()->setPaddingBottom(Length(theme()->popupInternalPaddingBottom(style()), Fixed));
}

void RenderMenuList::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    createInnerBlock();
    m_innerBlock->addChild(newChild, beforeChild);
}

void RenderMenuList::removeChild(RenderObject* oldChild)
{
    if (oldChild == m_innerBlock || !m_innerBlock) {
        RenderFlexibleBox::removeChild(oldChild);
        m_innerBlock = 0;
    } else
        m_innerBlock->removeChild(oldChild);
}

void RenderMenuList::setStyle(RenderStyle* newStyle)
{
    RenderBlock::setStyle(newStyle);
    
    if (m_buttonText)
        m_buttonText->setStyle(newStyle);
    if (m_innerBlock) // RenderBlock handled updating the anonymous block's style.
        adjustInnerStyle();
    setReplaced(isInline());
}

void RenderMenuList::updateFromElement()
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());

    if (m_optionsChanged) {
        const Vector<HTMLElement*>& listItems = select->listItems();
        int size = listItems.size();
        
        float width = 0;
        TextStyle textStyle(0, 0, 0, false, false, false, false);
        for (int i = 0; i < size; ++i) {
            HTMLElement* element = listItems[i];
            if (element->hasTagName(optionTag)) {
                String text = static_cast<HTMLOptionElement*>(element)->optionText();
                if (!text.isEmpty())
                    width = max(width, style()->font().floatWidth(TextRun(text.impl()), textStyle));
            }
        }
        m_optionsWidth = static_cast<int>(ceilf(width));
        m_optionsChanged = false;
    }

    setTextFromOption(select->selectedIndex());
}

void RenderMenuList::setTextFromOption(int optionIndex)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    int size = listItems.size();

    int i = select->optionToListIndex(optionIndex);
    String text = "";
    if (i >= 0 && i < size) {
        HTMLElement* element = listItems[i];
        if (element->hasTagName(optionTag))
            text = static_cast<HTMLOptionElement*>(listItems[i])->optionText();
    }
    setText(text.stripWhiteSpace());
}


bool RenderMenuList::multiple() const
{
    return static_cast<HTMLSelectElement *>(element())->multiple();
}


void RenderMenuList::setText(const String& s)
{
    if (s.isEmpty()) {
        if (!m_buttonText || !m_buttonText->isBR()) {
            if (m_buttonText)
                m_buttonText->destroy();
            m_buttonText = new (renderArena()) RenderBR(document());
            m_buttonText->setStyle(style());
            addChild(m_buttonText);
        }
    } else {
        if (m_buttonText && !m_buttonText->isBR())
            m_buttonText->setText(s.impl());
        else {
            if (m_buttonText)
                m_buttonText->destroy();
            m_buttonText = new (renderArena()) RenderText(document(), s.impl());
            m_buttonText->setStyle(style());
            addChild(m_buttonText);
        }
        adjustInnerStyle();
    }
}


String RenderMenuList::text()
{
    return m_buttonText ? m_buttonText->data() : String();
}

void RenderMenuList::calcMinMaxWidth()
{
    if (m_optionsChanged)
        updateFromElement();

    m_minWidth = 0;
    m_maxWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minWidth = m_maxWidth = calcContentBoxWidth(style()->width().value());
    else
        m_maxWidth = max(m_optionsWidth, theme()->minimumMenuListSize(style()));

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxWidth = max(m_maxWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minWidth = max(m_minWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minWidth = 0;
    else
        m_minWidth = m_maxWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxWidth = min(m_maxWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minWidth = min(m_minWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }


    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight() + 
                m_innerBlock->paddingLeft() + m_innerBlock->paddingRight() + m_innerBlock->borderLeft() + m_innerBlock->borderRight();

    m_minWidth += toAdd;
    m_maxWidth += toAdd;

    setMinMaxKnown();
}

void RenderMenuList::showPopup()
{
    if (m_popupIsVisible)
        return;
    // Create m_innerBlock here so it ends up as the first child.
    // This is important because otherwise we might try to create m_innerBlock
    // inside the showPopup call and it would fail.
    createInnerBlock();
    m_popupIsVisible = true;
}

void RenderMenuList::hidePopup()
{
    m_popupIsVisible = false;
}

void RenderMenuList::valueChanged(unsigned listIndex, bool fireOnChange)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    select->setSelectedIndex(select->listToOptionIndex(listIndex), true, fireOnChange);
}

}
