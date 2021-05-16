/*
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

#ifndef RenderTextField_H
#define RenderTextField_H

#include "RenderBlock.h"

namespace WebCore {

class HTMLTextFieldInnerElement;
class HTMLTextFieldInnerTextElement;
class HTMLSearchFieldCancelButtonElement;
class HTMLSearchFieldResultsButtonElement;

class RenderTextControl : public RenderBlock {
public:
    RenderTextControl(Node*, bool multiLine);
    virtual ~RenderTextControl();

    virtual void calcHeight();
    virtual void calcMinMaxWidth();
    virtual const char* renderName() const { return "RenderTextField"; }
    virtual void removeLeftoverAnonymousBoxes() {}
    virtual void setStyle(RenderStyle*);
    virtual void updateFromElement();
    virtual bool canHaveChildren() const { return false; }
    virtual short baselinePosition( bool, bool ) const;
    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty, HitTestAction hitTestAction);
    virtual void layout();
    virtual bool avoidsFloats() const { return true; }

    RenderStyle* createDivStyle(RenderStyle* startStyle);

    bool isEdited() const { return m_dirty; };
    void setEdited(bool isEdited) { m_dirty = isEdited; };
    bool isTextField() const { return !m_multiLine; }
    bool isTextArea() const { return m_multiLine; }
    
    int selectionStart();
    int selectionEnd();
    void setSelectionStart(int);
    void setSelectionEnd(int);    
    void select();
    void setSelectionRange(int, int);

    void subtreeHasChanged();
    String text();
    String textWithHardLineBreaks();
    void forwardEvent(Event*);
    void selectionChanged(bool userTriggered);

    // Subclassed to forward to our inner div.
    virtual int scrollLeft() const;
    virtual int scrollTop() const;
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);

    // Returns the line height of the inner renderer.
    virtual short innerLineHeight() const;

    VisiblePosition visiblePositionForIndex(int index);
    int indexForVisiblePosition(const VisiblePosition&);
    
    bool canScroll() const;
 

    bool popupIsVisible() const { return m_searchPopupIsVisible; }
    
private:

    RenderStyle* createInnerBlockStyle(RenderStyle* startStyle);
    RenderStyle* createInnerTextStyle(RenderStyle* startStyle);

    void updatePlaceholder();
    void createSubtreeIfNeeded();
#if 0
    void updateCancelButtonVisibility(RenderStyle*);
    const AtomicString& autosaveName() const;
#endif
    RefPtr<HTMLTextFieldInnerElement> m_innerBlock;
    RefPtr<HTMLTextFieldInnerTextElement> m_innerText;
    
    bool m_dirty;
    bool m_multiLine;
    bool m_placeholderVisible;

    bool m_searchPopupIsVisible;
    mutable Vector<String> m_recentSearches;

};

}

#endif
