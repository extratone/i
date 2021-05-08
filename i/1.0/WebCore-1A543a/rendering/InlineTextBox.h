/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef KHTML_InlineTextBox_H
#define KHTML_InlineTextBox_H

#include "DocumentMarker.h"
#include "RenderText.h"
#include "InlineRunBox.h"

namespace WebCore {

const int cNoTruncation = -1;
const int cFullTruncation = -2;

class String;
class StringImpl;
class MarkedTextUnderline;
class Position;

class InlineTextBox : public InlineRunBox
{
public:
    InlineTextBox(RenderObject* obj)
    :InlineRunBox(obj)
    {
        m_start = 0;
        m_len = 0;
        m_reversed = false;
        m_treatAsText = true;
        m_toAdd = 0;
        m_truncation = cNoTruncation;
    }
    
    InlineTextBox* nextTextBox() const { return static_cast<InlineTextBox*>(nextLineBox()); }
    InlineTextBox* prevTextBox() const { return static_cast<InlineTextBox*>(prevLineBox()); }
    
    unsigned start() const { return m_start; }
    unsigned end() const { return m_len ? m_start+m_len-1 : m_start; }
    unsigned len() const { return m_len; }
  
    void setStart(unsigned start) { m_start = start; }
    void setLen(unsigned len) { m_len = len; }

    void offsetRun(int d) { m_start += d; }

    void destroy(RenderArena* arena);
    
    IntRect selectionRect(int absx, int absy, int startPos, int endPos);
    bool isSelected(int startPos, int endPos) const;
    void selectionStartEnd(int& sPos, int& ePos);
    
    virtual void paint(RenderObject::PaintInfo& i, int tx, int ty);
    virtual bool nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty);

    RenderText* textObject() const;
    
    virtual void deleteLine(RenderArena* arena);
    virtual void extractLine();
    virtual void attachLine();

    virtual RenderObject::SelectionState selectionState();

    virtual void clearTruncation() { m_truncation = cNoTruncation; }
    virtual int placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool& foundBox);

    virtual bool isLineBreak() const;

    // Overloaded new operator.  Derived classes must override operator new
    // in order to allocate out of the RenderArena.
    void* operator new(size_t sz, RenderArena* renderArena) throw();    

    // Overridden to prevent the normal delete from being called.
    void operator delete(void* ptr, size_t sz);
    
private:
    // The normal operator new is disallowed.
    void* operator new(size_t sz) throw();

public:
    void setSpaceAdd(int add) { m_width -= m_toAdd; m_toAdd = add; m_width += m_toAdd; }
    int spaceAdd() { return m_toAdd; }

    virtual bool isInlineTextBox() { return true; }    
    virtual bool isText() const { return m_treatAsText; }
    void setIsText(bool b) { m_treatAsText = b; }
    
    void paintDecoration(GraphicsContext*, int tx, int ty, int decoration);
    void paintSelection(GraphicsContext*, int tx, int ty, RenderStyle*, const Font*);
    void paintMarkedTextBackground(GraphicsContext*, int tx, int ty, RenderStyle*, const Font*, int startPos, int endPos);
    void paintAllMarkersOfType(GraphicsContext*, int tx, int ty, DocumentMarker::MarkerType, RenderStyle*, const Font*);
    void paintSpellingMarker(GraphicsContext*, int tx, int ty, DocumentMarker);
    void paintTextMatchMarker(GraphicsContext*, int tx, int ty, DocumentMarker, RenderStyle*, const Font*);
    void paintMarkedTextUnderline(GraphicsContext*, int tx, int ty, MarkedTextUnderline&);
#if PLATFORM(MAC)
    void paintCustomHighlight(int tx, int ty, const AtomicString& type);
#endif
    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;
    
    int textPos() const;
    int offsetForPosition(int _x, bool includePartialGlyphs = true) const;
    int positionForOffset(int offset) const;

    bool containsCaretOffset(int offset) const; // false for offset after line break
    
    int m_start;
    unsigned short m_len;
    
    int m_truncation; // Where to truncate when text overflow is applied.  We use special constants to
                      // denote no truncation (the whole run paints) and full truncation (nothing paints at all).

    bool m_reversed : 1;
    bool m_dirOverride : 1;
    bool m_treatAsText : 1; // Whether or not to treat a <br> as text for the purposes of line height.
    int m_toAdd : 13; // for justified text

private:
    friend class RenderText;
};

inline RenderText *InlineTextBox::textObject() const
{
    return static_cast<RenderText *>(m_object);
}

}

#endif
