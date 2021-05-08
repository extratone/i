/*
 * This file is part of the HTML rendering engine for KDE.
 *
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
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
 */

#include "config.h"
#include "FixedTableLayout.h"

#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableSection.h"

/*
  The text below is from the CSS 2.1 specs.

  Fixed table layout

  With this (fast) algorithm, the horizontal layout of the table does
  not depend on the contents of the cells; it only depends on the
  table's width, the width of the columns, and borders or cell
  spacing.

  The table's width may be specified explicitly with the 'width'
  property. A value of 'auto' (for both 'display: table' and 'display:
  inline-table') means use the automatic table layout algorithm.

  In the fixed table layout algorithm, the width of each column is
  determined as follows:

    1. A column element with a value other than 'auto' for the 'width'
    property sets the width for that column.

    2. Otherwise, a cell in the first row with a value other than
    'auto' for the 'width' property sets the width for that column. If
    the cell spans more than one column, the width is divided over the
    columns.

    3. Any remaining columns equally divide the remaining horizontal
    table space (minus borders or cell spacing).

  The width of the table is then the greater of the value of the
  'width' property for the table element and the sum of the column
  widths (plus cell spacing or borders). If the table is wider than
  the columns, the extra space should be distributed over the columns.


  In this manner, the user agent can begin to lay out the table once
  the entire first row has been received. Cells in subsequent rows do
  not affect column widths. Any cell that has content that overflows
  uses the 'overflow' property to determine whether to clip the
  overflow content.
*/

using namespace std;

namespace WebCore {

FixedTableLayout::FixedTableLayout(RenderTable* table)
    : TableLayout(table)
{
}

int FixedTableLayout::calcWidthArray(int)
{
    int usedWidth = 0;

    // iterate over all <col> elements
    RenderObject* child = m_table->firstChild();
    int cCol = 0;
    int nEffCols = m_table->numEffCols();
    m_width.resize(nEffCols);
    m_width.fill(Length(Auto));

    Length grpWidth;
    while (child) {
        if (child->isTableCol()) {
            RenderTableCol *col = static_cast<RenderTableCol *>(child);
            int span = col->span();
            if (col->firstChild())
                grpWidth = col->style()->width();
            else {
                Length w = col->style()->width();
                if (w.isAuto())
                    w = grpWidth;
                int effWidth = 0;
                if (w.isFixed() && w.value() > 0)
                    effWidth = w.value();
                
                int usedSpan = 0;
                int i = 0;
                while (usedSpan < span) {
                    if(cCol + i >= nEffCols) {
                        m_table->appendColumn(span - usedSpan);
                        nEffCols++;
                        m_width.resize(nEffCols);
                        m_width[nEffCols-1] = Length();
                    }
                    int eSpan = m_table->spanOfEffCol(cCol+i);
                    if ((w.isFixed() || w.isPercent()) && w.isPositive()) {
                        m_width[cCol + i].setRawValue(w.type(), w.rawValue() * eSpan);
                        usedWidth += effWidth * eSpan;
                    }
                    usedSpan += eSpan;
                    i++;
                }
                cCol += i;
            }
        } else
            break;

        RenderObject *next = child->firstChild();
        if (!next)
            next = child->nextSibling();
        if (!next && child->parent()->isTableCol()) {
            next = child->parent()->nextSibling();
            grpWidth = Length();
        }
        child = next;
    }

    // Iterate over the first row in case some are unspecified.
    RenderTableSection* section = m_table->header();
    if (!section)
        section = m_table->firstBody();
    if (!section)
        section = m_table->footer();
    if (section && !section->numRows())
        section = m_table->sectionBelow(section, true);
    if (section) {
        cCol = 0;
        RenderObject* firstRow = section->firstChild();
        child = firstRow->firstChild();
        while (child) {
            if (child->isTableCell()) {
                RenderTableCell* cell = static_cast<RenderTableCell*>(child);
                if (cell->prefWidthsDirty())
                    cell->calcPrefWidths();

                Length w = cell->styleOrColWidth();
                int span = cell->colSpan();
                int effWidth = 0;
                if (w.isFixed() && w.isPositive())
                    effWidth = w.value();
                
                int usedSpan = 0;
                int i = 0;
                while (usedSpan < span) {
                    ASSERT(cCol + i < nEffCols);
                    int eSpan = m_table->spanOfEffCol(cCol + i);
                    // Only set if no col element has already set it.
                    if (m_width[cCol + i].isAuto() && w.type() != Auto) {
                        m_width[cCol + i].setRawValue(w.type(), w.rawValue() * eSpan / span);
                        usedWidth += effWidth * eSpan / span;
                    }
                    usedSpan += eSpan;
                    i++;
                }
                cCol += i;
            }
            child = child->nextSibling();
        }
    }

    return usedWidth;
}

void FixedTableLayout::calcPrefWidths(int& minWidth, int& maxWidth)
{
    // FIXME: This entire calculation is incorrect for both minwidth and maxwidth.
    
    // we might want to wait until we have all of the first row before
    // layouting for the first time.

    // only need to calculate the minimum width as the sum of the
    // cols/cells with a fixed width.
    //
    // The maximum width is max(minWidth, tableWidth).
    int bs = m_table->bordersPaddingAndSpacing();
    
    int tableWidth = m_table->style()->width().isFixed() ? m_table->style()->width().value() - bs : 0;
    int mw = calcWidthArray(tableWidth) + bs;

    minWidth = max(mw, tableWidth);
    maxWidth = minWidth;
}

void FixedTableLayout::layout()
{
    int tableWidth = m_table->width() - m_table->bordersPaddingAndSpacing();
    int nEffCols = m_table->numEffCols();
    Vector<int> calcWidth(nEffCols, 0);

    int numAuto = 0;
    int autoSpan = 0;
    int totalFixedWidth = 0;
    int totalPercentWidth = 0;
    int totalRawPercent = 0;

    // Compute requirements and try to satisfy fixed and percent widths.
    // Percentages are of the table's width, so for example
    // for a table width of 100px with columns (40px, 10%), the 10% compute
    // to 10px here, and will scale up to 20px in the final (80px, 20px).
    for (int i = 0; i < nEffCols; i++) {
        if (m_width[i].isFixed()) {
            calcWidth[i] = m_width[i].value();
            totalFixedWidth += calcWidth[i];
        } else if (m_width[i].isPercent()) {
            calcWidth[i] = m_width[i].calcValue(tableWidth);
            totalPercentWidth += calcWidth[i];
            totalRawPercent += m_width[i].rawValue();
        } else if (m_width[i].isAuto()) {
            numAuto++;
            autoSpan += m_table->spanOfEffCol(i);
        }
    }

    int hspacing = m_table->hBorderSpacing();
    int totalWidth = totalFixedWidth + totalPercentWidth;
    if (!numAuto || totalWidth > tableWidth) {
        // If there are no auto columns, or if the total is too wide, take
        // what we have and scale it to fit as necessary.
        if (totalWidth != tableWidth) {
            // Fixed widths only scale up
            if (totalFixedWidth && totalWidth < tableWidth) {
                totalFixedWidth = 0;
                for (int i = 0; i < nEffCols; i++) {
                    if (m_width[i].isFixed()) {
                        calcWidth[i] = calcWidth[i] * tableWidth / totalWidth;
                        totalFixedWidth += calcWidth[i];
                    }
                }
            }
            if (totalRawPercent) {
                totalPercentWidth = 0;
                for (int i = 0; i < nEffCols; i++) {
                    if (m_width[i].isPercent()) {
                        calcWidth[i] = m_width[i].rawValue() * (tableWidth - totalFixedWidth) / totalRawPercent;
                        totalPercentWidth += calcWidth[i];
                    }
                }
            }
            totalWidth = totalFixedWidth + totalPercentWidth;
        }
    } else {
        // Divide the remaining width among the auto columns.
        int remainingWidth = tableWidth - totalFixedWidth - totalPercentWidth - hspacing * (autoSpan - numAuto);
        int lastAuto = 0;
        for (int i = 0; i < nEffCols; i++) {
            if (m_width[i].isAuto()) {
                int span = m_table->spanOfEffCol(i);
                int w = remainingWidth * span / autoSpan;
                calcWidth[i] = w + hspacing * (span - 1);
                remainingWidth -= w;
                if (!remainingWidth)
                    break;
                lastAuto = i;
                numAuto--;
                autoSpan -= span;
            }
        }
        // Last one gets the remainder.
        if (remainingWidth)
            calcWidth[lastAuto] += remainingWidth;
        totalWidth = tableWidth;
    }

    if (totalWidth < tableWidth) {
        // Spread extra space over columns.
        int remainingWidth = tableWidth - totalWidth;
        int total = nEffCols;
        while (total) {
            int w = remainingWidth / total;
            remainingWidth -= w;
            calcWidth[--total] += w;
        }
        if (nEffCols > 0)
            calcWidth[nEffCols - 1] += remainingWidth;
    }
    
    int pos = 0;
    for (int i = 0; i < nEffCols; i++) {
        m_table->columnPositions()[i] = pos;
        pos += calcWidth[i] + hspacing;
    }
    int colPositionsSize = m_table->columnPositions().size();
    if (colPositionsSize > 0)
        m_table->columnPositions()[colPositionsSize - 1] = pos;
}

} // namespace WebCore
