/**
 * This file is part of the KDE project.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Nokia Corporation.
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
#include "RenderFrame.h"

#include "Document.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
#include "RenderFrameset.h"

namespace WebCore {

using namespace HTMLNames;

RenderFrame::RenderFrame(HTMLFrameElement* frame)
    : RenderPart(frame)
{
    setInline(false);
}

void RenderFrame::viewCleared()
{
    if (element() && m_widget && m_widget->isFrameView()) {
        FrameView* view = static_cast<FrameView*>(m_widget);
        HTMLFrameSetElement* frameSet = static_cast<HTMLFrameSetElement*>(element()->parentNode());
        bool hasBorder = element()->m_frameBorder && frameSet->frameBorder();
        int marginw = element()->m_marginWidth;
        int marginh = element()->m_marginHeight;

        view->setHasBorder(hasBorder);
        if (marginw != -1)
            view->setMarginWidth(marginw);
        if (marginh != -1)
            view->setMarginHeight(marginh);
    }
}

void RenderFrame::layoutWithFlattening(bool flexibleWidth, bool flexibleHeight)
{
    FrameView* childFrameView = static_cast<FrameView*>(m_widget);
    RenderObject* childRoot = childFrameView ? childFrameView->frame()->renderer() : 0;
    // don't expand frames set to have zero width or height
    if (!m_width || !m_height || !childRoot) {
        updateWidgetPosition();
        if (childFrameView)
            while (childFrameView->layoutPending())
                childFrameView->layout();
        setNeedsLayout(false);
        return;
    }

    // expand the frame by setting frame height = content height
    
    // need to update to calculate min/max correctly
    updateWidgetPosition();
    if (!childRoot->minMaxKnown())
        childRoot->calcMinMaxWidth();
    
    bool scrolling = element()->scrollingMode() != ScrollBarAlwaysOff;
    
    // if scrollbars are off assume it is ok for this frame to become really narrow
    if (scrolling || flexibleWidth || childFrameView->frame()->isFrameSet())
         m_width = max(m_width, childRoot->minWidth());
 
    // update again to pass the width to the child frame
    updateWidgetPosition();
     
    do
        childFrameView->layout();
    while (childFrameView->layoutPending() || childRoot->needsLayout());
        
    if (scrolling || flexibleHeight || childFrameView->frame()->isFrameSet())
        m_height = max(m_height, childFrameView->contentsHeight());
    if (scrolling || flexibleWidth || childFrameView->frame()->isFrameSet())
        m_width = max(m_width, childFrameView->contentsWidth());
    
    updateWidgetPosition();

    ASSERT(!childFrameView->layoutPending());
    ASSERT(!childRoot->needsLayout());
    ASSERT(!childRoot->firstChild() || !childRoot->firstChild()->firstChild() || !childRoot->firstChild()->firstChild()->needsLayout());
    
    setNeedsLayout(false);
}

}