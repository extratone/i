/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *           (C) 2008 Rob Buis <buis@kde.org>
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

#if ENABLE(SVG)
#include "RenderSVGInlineText.h"

#include "FloatConversion.h"
#include "FloatQuad.h"
#include "RenderBlock.h"
#include "RenderSVGRoot.h"
#include "SVGInlineTextBox.h"
#include "SVGRootInlineBox.h"
#include "VisiblePosition.h"

namespace WebCore {

static inline bool isChildOfHiddenContainer(RenderObject* start)
{
    while (start) {
        if (start->isSVGHiddenContainer())
            return true;

        start = start->parent();
    }

    return false;
}

RenderSVGInlineText::RenderSVGInlineText(Node* n, PassRefPtr<StringImpl> str) 
    : RenderText(n, str)
{
}


void RenderSVGInlineText::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    // Skip RenderText's work.
    RenderObject::styleDidChange(diff, oldStyle);
    
    // SVG text is apparently always transformed.
    if (RefPtr<StringImpl> textToTransform = originalText())
        setText(textToTransform.release(), true);
}

void RenderSVGInlineText::absoluteRects(Vector<IntRect>& rects, int, int, bool)
{
    rects.append(computeRepaintRectForRange(0, 0, textLength()));
}

void RenderSVGInlineText::absoluteQuads(Vector<FloatQuad>& quads, bool)
{
    quads.append(FloatRect(computeRepaintRectForRange(0, 0, textLength())));
}

IntRect RenderSVGInlineText::selectionRectForRepaint(RenderBox* repaintContainer, bool /*clipToVisibleContent*/)
{
    ASSERT(!needsLayout());

    if (selectionState() == SelectionNone)
        return IntRect();

    // Early exit if we're ie. a <text> within a <defs> section.
    if (isChildOfHiddenContainer(this))
        return IntRect();

    // Now calculate startPos and endPos for painting selection.
    // We include a selection while endPos > 0
    int startPos, endPos;
    if (selectionState() == SelectionInside) {
        // We are fully selected.
        startPos = 0;
        endPos = textLength();
    } else {
        selectionStartEnd(startPos, endPos);
        if (selectionState() == SelectionStart)
            endPos = textLength();
        else if (selectionState() == SelectionEnd)
            startPos = 0;
    }

    if (startPos == endPos)
        return IntRect();

    return computeRepaintRectForRange(repaintContainer, startPos, endPos);
}

IntRect RenderSVGInlineText::computeRepaintRectForRange(RenderBox* /*repaintContainer*/, int startPos, int endPos)
{
    RenderBlock* cb = containingBlock();
    if (!cb || !cb->container())
        return IntRect();

    RenderSVGRoot* root = findSVGRootObject(parent());
    if (!root)
        return IntRect();

    IntRect rect;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        rect.unite(box->selectionRect(0, 0, startPos, endPos));

    // Mimic RenderBox::computeAbsoluteRepaintRect() functionality. But only the subset needed for SVG and respecting SVG transformations.
    FloatPoint absPos = cb->container()->localToAbsolute();

    // Remove HTML parent translation offsets here! These need to be retrieved from the RenderSVGRoot object.
    // But do take the containingBlocks's container position into account, ie. SVG text in scrollable <div>.
    TransformationMatrix htmlParentCtm = root->RenderContainer::absoluteTransform();

    FloatRect fixedRect(narrowPrecisionToFloat(rect.x() + absPos.x() - htmlParentCtm.e()),
                        narrowPrecisionToFloat(rect.y() + absPos.y() - htmlParentCtm.f()), rect.width(), rect.height());
    // FIXME: broken with CSS transforms, and non-zero repaintContainer
    return enclosingIntRect(absoluteTransform().mapRect(fixedRect));
}

InlineTextBox* RenderSVGInlineText::createInlineTextBox()
{
    return new (renderArena()) SVGInlineTextBox(this);
}

IntRect RenderSVGInlineText::localCaretRect(InlineBox*, int, int*)
{
    // SVG doesn't have any editable content where a caret rect would be needed.
    // FIXME: That's not sufficient. The localCaretRect function is also used for selection.
    return IntRect();
}

VisiblePosition RenderSVGInlineText::positionForCoordinates(int x, int y)
{
    SVGInlineTextBox* textBox = static_cast<SVGInlineTextBox*>(firstTextBox());

    if (!textBox || textLength() == 0)
        return VisiblePosition(element(), 0, DOWNSTREAM);

    SVGRootInlineBox* rootBox = textBox->svgRootInlineBox();
    RenderBlock* object = rootBox ? rootBox->block() : 0;

    if (!object)
        return VisiblePosition(element(), 0, DOWNSTREAM);

    int offset = 0;

    for (SVGInlineTextBox* box = textBox; box; box = static_cast<SVGInlineTextBox*>(box->nextTextBox())) {
        if (box->svgCharacterHitsPosition(x + object->x(), y + object->y(), offset)) {
            // If we're not at the end/start of the box, stop looking for other selected boxes.
            if (box->direction() == LTR) {
                if (offset <= (int) box->end() + 1)
                    break;
            } else {
                if (offset > (int) box->start())
                    break;
            }
        }
    }

    return VisiblePosition(element(), offset, DOWNSTREAM);
}

void RenderSVGInlineText::destroy()
{
    if (!documentBeingDestroyed()) {
        setNeedsLayoutAndPrefWidthsRecalc();
        repaint();
    }
    RenderText::destroy();
}

}

#endif // ENABLE(SVG)
