/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
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
#include "RenderBR.h"

#include "Document.h"
#include "InlineTextBox.h"
#include "VisiblePosition.h"

namespace WebCore {

RenderBR::RenderBR(Node* node)
    : RenderText(node, new StringImpl("\n")), m_lineHeight(-1)
{
}

RenderBR::~RenderBR()
{
}

InlineBox* RenderBR::createInlineBox(bool makePlaceholder, bool isRootLineBox, bool isOnlyRun)
{
    // We only treat a box as text for a <br> if we are on a line by ourself or in strict mode
    // (Note the use of strict mode.  In "almost strict" mode, we don't treat the box for <br> as text.)
    InlineTextBox* box = static_cast<InlineTextBox*>(RenderText::createInlineBox(makePlaceholder, isRootLineBox, isOnlyRun));
    box->setIsText(isOnlyRun || document()->inStrictMode());
    return box;
}

short RenderBR::baselinePosition( bool firstLine, bool isRootLineBox) const
{
    if (firstTextBox() && !firstTextBox()->isText())
        return 0;
    return RenderText::baselinePosition(firstLine, isRootLineBox);
}

short RenderBR::lineHeight(bool firstLine, bool isRootLineBox) const
{
    if (firstTextBox() && !firstTextBox()->isText())
        return 0;

    if (firstLine) {
        RenderStyle* s = style(firstLine);
        Length lh = s->lineHeight();
        if (lh.value() < 0) {
            if (s == style()) {
                if (m_lineHeight == -1)
                    m_lineHeight = RenderObject::lineHeight(false);
                return m_lineHeight;
            }
            return s->font().lineSpacing();
        }
        if (lh.isPercent())
            return lh.calcMinValue(s->fontSize());
        return lh.value();
    }
    
    if (m_lineHeight == -1)
        m_lineHeight = RenderObject::lineHeight(false);
    return m_lineHeight;
}

void RenderBR::setStyle(RenderStyle* _style)
{
    RenderText::setStyle(_style);
    m_lineHeight = -1;
}

int RenderBR::caretMinOffset() const 
{ 
    return 0; 
}

int RenderBR::caretMaxOffset() const 
{ 
    return 1;
}

unsigned RenderBR::caretMaxRenderedOffset() const
{
    return 1;
}

VisiblePosition RenderBR::positionForCoordinates(int _x, int _y)
{
    return VisiblePosition(element(), 0, DOWNSTREAM);
}

InlineBox *RenderBR::inlineBox(int offset, EAffinity affinity)
{
    return firstTextBox();
}

}
