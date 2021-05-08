/*
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2005 Apple Computer
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

#ifndef RenderButton_h
#define RenderButton_h

#include "RenderFlexibleBox.h"
#include "Timer.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class RenderTextFragment;

// RenderButtons are just like normal flexboxes except that they will generate an anonymous block child.
// For inputs, they will also generate an anonymous RenderText and keep its style and content up
// to date as the button changes.
class RenderButton : public RenderFlexibleBox {
public:
    RenderButton(Node*);

    virtual const char* renderName() const { return "RenderButton"; }

    virtual void addChild(RenderObject* newChild, RenderObject *beforeChild = 0);
    virtual void removeChild(RenderObject*);
    virtual void removeLeftoverAnonymousBlock(RenderBlock*) { }
    virtual bool createsAnonymousWrapper() const { return true; }

    void setupInnerStyle(RenderStyle*);
    virtual void updateFromElement();

    virtual void updateBeforeAfterContent(RenderStyle::PseudoId);

    virtual bool hasControlClip() const { return true; }
    virtual IntRect controlClipRect(int /*tx*/, int /*ty*/) const;

    void setText(const String&);
    
    virtual bool canHaveChildren() const;
    
    virtual void layout();

protected:
    virtual void styleWillChange(StyleDifference, const RenderStyle* newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    virtual bool hasLineIfEmpty() const { return true; }

    void timerFired(Timer<RenderButton>*);

    RenderTextFragment* m_buttonText;
    RenderBlock* m_inner;

    OwnPtr<Timer<RenderButton> > m_timer;
    bool m_default;
};

} // namespace WebCore

#endif // RenderButton_h
