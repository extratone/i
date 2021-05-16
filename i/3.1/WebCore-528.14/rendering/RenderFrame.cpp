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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderFrame.h"
#include "RenderFrameSet.h"
#include "Document.h"
#include "Frame.h"
#include "RenderView.h"
#include "FrameView.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

RenderFrame::RenderFrame(HTMLFrameElement* frame)
    : RenderPart(frame)
{
    setInline(false);
}

FrameEdgeInfo RenderFrame::edgeInfo() const
{
    return FrameEdgeInfo(element()->noResize(), element()->hasFrameBorder());
}

void RenderFrame::viewCleared()
{
    if (element() && m_widget && m_widget->isFrameView()) {
        FrameView* view = static_cast<FrameView*>(m_widget);
        int marginw = element()->getMarginWidth();
        int marginh = element()->getMarginHeight();

        if (marginw != -1)
            view->setMarginWidth(marginw);
        if (marginh != -1)
            view->setMarginHeight(marginh);
    }
}

void RenderFrame::layoutWithFlattening(bool flexibleWidth, bool flexibleHeight)
{
    FrameView* childFrameView = static_cast<FrameView*>(m_widget);
    RenderView* childRoot = childFrameView ? childFrameView->frame()->contentRenderer() : 0;
    // don't expand frames set to have zero width or height
    if (!width() || !height() || !childRoot) {
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
    if (childRoot->prefWidthsDirty())
        childRoot->calcPrefWidths();
    
    bool scrolling = element()->scrollingMode() != ScrollbarAlwaysOff;
    
    // if scrollbars are off assume it is ok for this frame to become really narrow
    if (scrolling || flexibleWidth || childFrameView->frame()->isFrameSet())
         setWidth(max(width(), childRoot->minPrefWidth()));
 
    // update again to pass the width to the child frame
    updateWidgetPosition();
     
    do
        childFrameView->layout();
    while (childFrameView->layoutPending() || childRoot->needsLayout());
        
    if (scrolling || flexibleHeight || childFrameView->frame()->isFrameSet())
        setHeight(max(height(), childFrameView->contentsHeight()));
    if (scrolling || flexibleWidth || childFrameView->frame()->isFrameSet())
        setWidth(max(width(), childFrameView->contentsWidth()));
    
    updateWidgetPosition();

    ASSERT(!childFrameView->layoutPending());
    ASSERT(!childRoot->needsLayout());
    ASSERT(!childRoot->firstChild() || !childRoot->firstChild()->firstChild() || !childRoot->firstChild()->firstChild()->needsLayout());
    
    setNeedsLayout(false);
}

} // namespace WebCore
