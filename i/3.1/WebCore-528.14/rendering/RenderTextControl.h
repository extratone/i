/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/) 
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

#ifndef RenderTextControl_h
#define RenderTextControl_h

#include "RenderBlock.h"

namespace WebCore {

class FormControlElement;
class Selection;
class TextControlInnerElement;
class TextControlInnerTextElement;

class RenderTextControl : public RenderBlock {
public:
    virtual ~RenderTextControl();

    virtual const char* renderName() const { return "RenderTextControl"; }
    virtual bool hasControlClip() const { return false; }
    virtual IntRect controlClipRect(int tx, int ty) const;
    virtual void calcHeight();
    virtual void calcPrefWidths();
    virtual void removeLeftoverAnonymousBlock(RenderBlock*) { }
    virtual void updateFromElement();
    virtual bool canHaveChildren() const { return false; }
    virtual bool avoidsFloats() const { return true; }
    
    virtual bool isEdited() const { return m_edited; }
    virtual void setEdited(bool isEdited) { m_edited = isEdited; }

    bool isUserEdited() const { return m_userEdited; }
    void setUserEdited(bool isUserEdited);

    int selectionStart();
    int selectionEnd();
    void setSelectionStart(int);
    void setSelectionEnd(int);
    void select();
    void setSelectionRange(int start, int end);
    Selection selection(int start, int end) const;

    virtual void subtreeHasChanged();
    String text();
    String textWithHardLineBreaks();
    void selectionChanged(bool userTriggered);

    virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);

    virtual bool canBeProgramaticallyScrolled(bool) const { return true; }
    virtual void autoscroll();

    // Subclassed to forward to our inner div.
    virtual int scrollLeft() const;
    virtual int scrollTop() const;
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);
    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1.0f);

    bool canScroll() const;

    // Returns the line height of the inner renderer.
    virtual int innerLineHeight() const;

    VisiblePosition visiblePositionForIndex(int index);
    int indexForVisiblePosition(const VisiblePosition&);

protected:
    RenderTextControl(Node*);

    int scrollbarThickness() const;
    void adjustInnerTextStyle(const RenderStyle* startStyle, RenderStyle* textBlockStyle) const;
    void setInnerTextValue(const String&);

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    void createSubtreeIfNeeded(TextControlInnerElement* innerBlock);
    void hitInnerTextElement(HitTestResult&, int x, int y, int tx, int ty);
    void forwardEvent(Event*);

    int textBlockWidth() const;
    int textBlockHeight() const;

    virtual int preferredContentWidth(float charWidth) const = 0;
    virtual void adjustControlHeightBasedOnLineHeight(int lineHeight) = 0;
    virtual void cacheSelection(int start, int end) = 0;
    virtual PassRefPtr<RenderStyle> createInnerTextStyle(const RenderStyle* startStyle) const = 0;

    friend class TextIterator;
    HTMLElement* innerTextElement() const;

    FormControlElement* formControlElement() const;

private:
    String finishText(Vector<UChar>&) const;

    bool m_edited;
    bool m_userEdited;
    RefPtr<TextControlInnerTextElement> m_innerText;
};

} // namespace WebCore

#endif // RenderTextControl_h
