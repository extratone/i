/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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
#include "RenderButton.h"

#include "Document.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "RenderText.h"
#include "HTMLNames.h"

#include "RenderTheme.h"

namespace WebCore {

using namespace HTMLNames;

RenderButton::RenderButton(Node* node)
    : RenderFlexibleBox(node)
    , m_buttonText(0)
    , m_inner(0)
{
}

void RenderButton::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (!m_inner) {
        // Create an anonymous block.
        assert(!m_first);
        m_inner = createAnonymousBlock();
        m_inner->style()->setBoxFlex(1.0f);
        RenderFlexibleBox::addChild(m_inner);
    }
    
    m_inner->addChild(newChild, beforeChild);
}

void RenderButton::removeChild(RenderObject* oldChild)
{
    if (oldChild == m_inner || !m_inner) {
        RenderFlexibleBox::removeChild(oldChild);
        m_inner = 0;
    }
    else
        m_inner->removeChild(oldChild);
}

void RenderButton::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    if (m_buttonText)
        m_buttonText->setStyle(style);
    if (m_inner)
        m_inner->style()->setBoxFlex(1.0f);
    setReplaced(isInline());
}

void RenderButton::updateFromElement()
{
    // If we're an input element, we may need to change our button text.
    if (element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(element());
        
        String value = input->valueWithDefault();
        setText(value);
    }
}

void RenderButton::setText(const String& str)
{
    if (str.isEmpty()) {
        if (m_buttonText) {
            m_buttonText->destroy();
            m_buttonText = 0;
        }
    } else {
        if (m_buttonText)
            m_buttonText->setText(str.impl());
        else {
            m_buttonText = new (renderArena()) RenderText(document(), str.impl());
            m_buttonText->setStyle(style());
            addChild(m_buttonText);
        }
    }
}

void RenderButton::updatePseudoChild(RenderStyle::PseudoId type)
{
    if (m_inner)
        m_inner->updatePseudoChildForObject(type, this);
    else
        updatePseudoChildForObject(type, this);
}

void RenderButton::layout()
{
    RenderFlexibleBox::layout();

    if (style()->appearance() == NoAppearance || style()->backgroundLayers()->hasImage()) return;
    
    IntSize radius(MIN(width(), height()) / 2.0, height() / 2.0);
    
    style()->setBorderTopLeftRadius(radius);
    style()->setBorderTopRightRadius(radius);
    style()->setBorderBottomLeftRadius(radius);
    style()->setBorderBottomRightRadius(radius);
}

void RenderButton::paintObject(PaintInfo& i, int _tx, int _ty)
{
    // Push a clip.
    if (m_inner && i.phase == PaintPhaseForeground) {
        IntRect clipRect(_tx + borderLeft(), _ty + borderTop(),
            width() - borderLeft() - borderRight(), height() - borderBottom() - borderTop());
        if (clipRect.width() == 0 || clipRect.height() == 0)
            return;
        i.p->save();
        i.p->addClip(clipRect);
    }
    
    // Paint the children.
    RenderBlock::paintObject(i, _tx, _ty);
    
    // Pop the clip.
    if (m_inner && i.phase == PaintPhaseForeground)
        i.p->restore();
}

}
