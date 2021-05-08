/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 */

#ifndef RenderTable_H
#define RenderTable_H

#include "RenderBlock.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderTableCol;
class RenderTableCell;
class RenderTableSection;
class TableLayout;

class RenderTable : public RenderBlock {
public:
    enum Rules {
        None    = 0x00,
        RGroups = 0x01,
        CGroups = 0x02,
        Groups  = 0x03,
        Rows    = 0x05,
        Cols    = 0x0a,
        All     = 0x0f
    };
    enum Frame {
        Void   = 0x00,
        Above  = 0x01,
        Below  = 0x02,
        Lhs    = 0x04,
        Rhs    = 0x08,
        Hsides = 0x03,
        Vsides = 0x0c,
        Box    = 0x0f
    };

    RenderTable(Node*);
    ~RenderTable();

    virtual const char* renderName() const { return "RenderTable"; }

    virtual void setStyle(RenderStyle*);

    virtual bool isTable() const { return true; }

    int getColumnPos(int col) const { return columnPos[col]; }

    int hBorderSpacing() const { return hspacing; }
    int vBorderSpacing() const { return vspacing; }
    
    bool collapseBorders() const { return style()->borderCollapse(); }
    int borderLeft() const { return m_borderLeft; }
    int borderRight() const { return m_borderRight; }
    int borderTop() const;
    int borderBottom() const;
    
    Rules getRules() const { return static_cast<Rules>(rules); }

    const Color& bgColor() const { return style()->backgroundColor(); }

    unsigned cellPadding() const { return padding; }
    void setCellPadding(unsigned p) { padding = p; }

    int outerBorderTop() const;
    int outerBorderBottom() const;
    int outerBorderLeft() const;
    int outerBorderRight() const;
    
    int calcBorderLeft() const;
    int calcBorderRight() const;
    void recalcHorizontalBorders();

    // overrides
    virtual int overflowHeight(bool includeInterior = true) const { return height(); }
    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0);
    virtual void paint(PaintInfo&, int tx, int ty);
    virtual void paintBoxDecorations(PaintInfo&, int _tx, int _ty);
    virtual void layout();
    virtual void calcMinMaxWidth();

    virtual RenderBlock* firstLineBlock() const;
    virtual void updateFirstLetter();
    
    virtual void setCellWidths();

    virtual void calcWidth();

#ifndef NDEBUG
    virtual void dump(TextStream *stream, DeprecatedString ind = "") const;
#endif
    struct ColumnStruct {
        enum {
            WidthUndefined = 0xffff
        };
        ColumnStruct() {
            span = 1;
            width = WidthUndefined;
        }
        unsigned short span;
        unsigned width; // the calculated position of the column
    };

    Vector<int> columnPos;
    Vector<ColumnStruct> columns;

    void splitColumn(int pos, int firstSpan);
    void appendColumn(int span);
    int numEffCols() const { return columns.size(); }
    int spanOfEffCol(int effCol) const { return columns[effCol].span; }
    
    int colToEffCol(int col) const
    {
        int i = 0;
        int effCol = numEffCols();
        for (int c = 0; c < col && i < effCol; ++i)
            c += columns[i].span;
        return i;
    }
    
    int effColToCol(int effCol) const
    {
        int c = 0;
        for (int i = 0; i < effCol; i++)
            c += columns[i].span;
        return c;
    }

    int bordersPaddingAndSpacing() const {
        return borderLeft() + borderRight() + 
               (collapseBorders() ? 0 : (paddingLeft() + paddingRight() + (numEffCols() + 1) * hBorderSpacing()));
    }

    RenderTableCol* colElement(int col) const;

    void setNeedSectionRecalc() { needSectionRecalc = true; }

    virtual RenderObject* removeChildNode(RenderObject*);

    RenderTableSection* sectionAbove(const RenderTableSection*, bool skipEmptySections = false) const;
    RenderTableSection* sectionBelow(const RenderTableSection*, bool skipEmptySections = false) const;

    RenderTableCell* cellAbove(const RenderTableCell*) const;
    RenderTableCell* cellBelow(const RenderTableCell*) const;
    RenderTableCell* cellBefore(const RenderTableCell*) const;
    RenderTableCell* cellAfter(const RenderTableCell*) const;
 
    CollapsedBorderValue* currentBorderStyle() { return m_currentBorder; }
    
    bool hasSections() const { return head || foot || firstBody; }

    virtual IntRect getOverflowClipRect(int tx, int ty);

    void recalcSectionsIfNeeded();

private:
    void recalcSections();

    friend class AutoTableLayout;
    friend class FixedTableLayout;

    RenderBlock* tCaption;
    RenderTableSection* head;
    RenderTableSection* foot;
    RenderTableSection* firstBody;

    TableLayout* tableLayout;

    CollapsedBorderValue* m_currentBorder;
    
    unsigned frame : 4; // Frame
    unsigned rules : 4; // Rules

    bool has_col_elems : 1;
    unsigned padding : 22;
    bool needSectionRecalc : 1;
    
    short hspacing;
    short vspacing;
    int m_borderRight;
    int m_borderLeft;
};

}

#endif
