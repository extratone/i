/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
  
#include "config.h"
#include "SelectionController.h"

#include "Document.h"
#include "Element.h"
#include "EventNames.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "RenderView.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"

#define EDIT_DEBUG 0

namespace WebCore {

using namespace EventNames;

SelectionController::SelectionController()
    : m_needsLayout(true)
{
    setSelection(Selection());
}

SelectionController::SelectionController(const Selection &sel)
    : m_needsLayout(true)
    , m_modifyBiasSet(false)
{
    setSelection(sel);
}

SelectionController::SelectionController(const Position &pos, EAffinity affinity)
    : m_needsLayout(true)
    , m_modifyBiasSet(false)
{
    setSelection(Selection(pos, pos, affinity));
}

SelectionController::SelectionController(const Range *r, EAffinity affinity)
    : m_needsLayout(true)
    , m_modifyBiasSet(false)
{
    setSelection(Selection(startPosition(r), endPosition(r), affinity));
}

SelectionController::SelectionController(const Position &base, const Position &extent, EAffinity affinity)
    : m_needsLayout(true)
    , m_modifyBiasSet(false)
{
    setSelection(Selection(base, extent, affinity));
}

SelectionController::SelectionController(const VisiblePosition &visiblePos)
    : m_needsLayout(true)
    , m_modifyBiasSet(false)
{
    setSelection(Selection(visiblePos.deepEquivalent(), visiblePos.deepEquivalent(), visiblePos.affinity()));
}

SelectionController::SelectionController(const VisiblePosition &base, const VisiblePosition &extent)
    : m_needsLayout(true)
    , m_modifyBiasSet(false)
{
    setSelection(Selection(base.deepEquivalent(), extent.deepEquivalent(), base.affinity()));
}

SelectionController::SelectionController(const SelectionController &o)
    : m_needsLayout(o.m_needsLayout)
    , m_modifyBiasSet(o.m_modifyBiasSet)
{
    setSelection(o.m_sel); 
    // Only copy the coordinates over if the other object
    // has had a layout, otherwise keep the current
    // coordinates. This prevents drawing artifacts from
    // remaining when the caret is painted and then moves,
    // and the old rectangle needs to be repainted.
    if (!m_needsLayout) {
        m_caretRect = o.m_caretRect;
        m_caretPositionOnLayout = o.m_caretPositionOnLayout;
    }
}

SelectionController &SelectionController::operator=(const SelectionController &o)
{
    setSelection(o.m_sel);

    m_needsLayout = o.m_needsLayout;
    m_modifyBiasSet = o.m_modifyBiasSet;
    
    // Only copy the coordinates over if the other object
    // has had a layout, otherwise keep the current
    // coordinates. This prevents drawing artifacts from
    // remaining when the caret is painted and then moves,
    // and the old rectangle needs to be repainted.
    if (!m_needsLayout) {
        m_caretRect = o.m_caretRect;
        m_caretPositionOnLayout = o.m_caretPositionOnLayout;
    }
    
    return *this;
}

void SelectionController::moveTo(const VisiblePosition &pos)
{
    setSelection(Selection(pos.deepEquivalent(), pos.deepEquivalent(), pos.affinity()));
    m_needsLayout = true;
}

void SelectionController::moveTo(const VisiblePosition &base, const VisiblePosition &extent)
{
    setSelection(Selection(base.deepEquivalent(), extent.deepEquivalent(), base.affinity()));
    m_needsLayout = true;
}

void SelectionController::moveTo(const SelectionController &o)
{
    setSelection(o.m_sel);
    m_needsLayout = true;
}

void SelectionController::moveTo(const Position &pos, EAffinity affinity)
{
    setSelection(Selection(pos, affinity));
    m_needsLayout = true;
}

void SelectionController::moveTo(const Range *r, EAffinity affinity)
{
    setSelection(Selection(startPosition(r), endPosition(r), affinity));
    m_needsLayout = true;
}

void SelectionController::moveTo(const Position &base, const Position &extent, EAffinity affinity)
{
    setSelection(Selection(base, extent, affinity));
    m_needsLayout = true;
}

void SelectionController::setSelection(const Selection& newSelection)
{
    m_sel = newSelection;
}

void SelectionController::nodeWillBeRemoved(Node *node)
{
    if (isNone())
        return;
    
    Node* base = m_sel.base().node();
    Node* extent = m_sel.extent().node();
    Node* start = m_sel.start().node();
    Node* end = m_sel.end().node();
    
    bool baseRemoved = node == base || (base && base->isAncestor(node));
    bool extentRemoved = node == extent || (extent && extent->isAncestor(node));
    bool startRemoved = node == start || (start && start->isAncestor(node));
    bool endRemoved = node == end || (end && end->isAncestor(node));
    
    bool clearRenderTreeSelection = false;
    bool clearDOMTreeSelection = false;

    if (startRemoved || endRemoved) {
        // FIXME: When endpoints are removed, we should just alter the selection, instead of blowing it away.
        clearRenderTreeSelection = true;
        clearDOMTreeSelection = true;
    } else if (baseRemoved || extentRemoved) {
        if (m_sel.isBaseFirst()) {
            m_sel.setBase(m_sel.start());
            m_sel.setExtent(m_sel.end());
        } else {
            m_sel.setBase(m_sel.start());
            m_sel.setExtent(m_sel.end());
        }
    // FIXME: This could be more efficient if we had an isNodeInRange function on Ranges.
    } else if (Range::compareBoundaryPoints(m_sel.start(), Position(node, 0)) == -1 &&
               Range::compareBoundaryPoints(m_sel.end(), Position(node, 0)) == 1) {
        // If we did nothing here, when this node's renderer was destroyed, the rect that it 
        // occupied would be invalidated, but, selection gaps that change as a result of 
        // the removal wouldn't be invalidated.
        // FIXME: Don't do so much unnecessary invalidation.
        clearRenderTreeSelection = true;
    }

    if (clearRenderTreeSelection) {
        // FIXME (6498): This doesn't notify the editing delegate of a selection change.
        RefPtr<Document> document = start->document();
        document->updateRendering();
        if (RenderView* view = static_cast<RenderView*>(document->renderer()))
            view->clearSelection();
    }

    if (clearDOMTreeSelection)
        setSelection(Selection());
}

void SelectionController::setModifyBias(EAlter alter, EDirection direction)
{
    switch (alter) {
        case MOVE:
            m_modifyBiasSet = false;
            break;
        case EXTEND:
            if (!m_modifyBiasSet) {
                m_modifyBiasSet = true;
                switch (direction) {
                    // FIXME: right for bidi?
                    case RIGHT:
                    case FORWARD:
                        m_sel.setBase(m_sel.start());
                        m_sel.setExtent(m_sel.end());
                        break;
                    case LEFT:
                    case BACKWARD:
                        m_sel.setBase(m_sel.end());
                        m_sel.setExtent(m_sel.start());
                        break;
                }
            }
            break;
    }
}

VisiblePosition SelectionController::modifyExtendingRightForward(TextGranularity granularity)
{
    VisiblePosition pos(m_sel.extent(), m_sel.affinity());
    switch (granularity) {
        case CharacterGranularity:
            if (isLastVisiblePositionBeforeTableElement(pos.deepEquivalent()))
                pos = VisiblePosition(positionAfterFollowingTableElement(pos.deepEquivalent()), VP_DEFAULT_AFFINITY);
            else
                pos = pos.next();
            break;
        case WordGranularity:
            if (isLastVisiblePositionBeforeTableElement(pos.deepEquivalent()))
                pos = VisiblePosition(positionAfterFollowingTableElement(pos.deepEquivalent()), VP_DEFAULT_AFFINITY);
            else
                pos = nextWordPosition(pos);
            break;
        case SentenceGranularity:
            pos = nextSentencePosition(pos);
            break;
        case LineGranularity:
            pos = nextLinePosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case ParagraphGranularity:
            pos = nextParagraphPosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case SentenceBoundary:
            pos = endOfSentence(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = endOfLine(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = endOfParagraph(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            if (pos.deepEquivalent().node()->isContentEditable())
                pos = endOfEditableContent(pos);
            else
                pos = endOfDocument(pos);
            break;
    }
    
    return pos;
}

VisiblePosition SelectionController::modifyMovingRightForward(TextGranularity granularity)
{
    VisiblePosition pos;
    // FIXME: Stay in editable content for the less common granularities.
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            else
                pos = VisiblePosition(m_sel.extent(), m_sel.affinity()).next(true);
            break;
        case WordGranularity:
            pos = nextWordPosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case SentenceGranularity:
            pos = nextSentencePosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case LineGranularity: {
            // down-arrowing from a range selection that ends at the start of a line needs
            // to leave the selection at that line start (no need to call nextLinePosition!)
            pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            if (!isRange() || !isStartOfLine(pos))
                pos = nextLinePosition(pos, xPosForVerticalArrowNavigation(END, isRange()));
            break;
        }
        case ParagraphGranularity:
            pos = nextParagraphPosition(VisiblePosition(m_sel.end(), m_sel.affinity()), xPosForVerticalArrowNavigation(END, isRange()));
            break;
        case SentenceBoundary:
            pos = endOfSentence(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = endOfLine(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = endOfParagraph(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            if (pos.deepEquivalent().node()->isContentEditable())
                pos = endOfEditableContent(pos);
            else
                pos = endOfDocument(pos);
            break;
            
    }
    return pos;
}

VisiblePosition SelectionController::modifyExtendingLeftBackward(TextGranularity granularity)
{
    VisiblePosition pos(m_sel.extent(), m_sel.affinity());
        
    // Extending a selection backward by word or character from just after a table selects
    // the table.  This "makes sense" from the user perspective, esp. when deleting.
    // It was done here instead of in VisiblePosition because we want VPs to iterate
    // over everything.
    switch (granularity) {
        case CharacterGranularity:
            if (isFirstVisiblePositionAfterTableElement(pos.deepEquivalent()))
                pos = VisiblePosition(positionBeforePrecedingTableElement(pos.deepEquivalent()), VP_DEFAULT_AFFINITY);
            else
                pos = pos.previous();
            break;
        case WordGranularity:
            if (isFirstVisiblePositionAfterTableElement(pos.deepEquivalent()))
                pos = VisiblePosition(positionBeforePrecedingTableElement(pos.deepEquivalent()), VP_DEFAULT_AFFINITY);
            else
                pos = previousWordPosition(pos);
            break;
        case SentenceGranularity:
            pos = previousSentencePosition(pos);
            break;
        case LineGranularity:
            pos = previousLinePosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case ParagraphGranularity:
            pos = previousParagraphPosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case SentenceBoundary:
            pos = startOfSentence(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = startOfLine(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = startOfParagraph(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            if (pos.deepEquivalent().node()->isContentEditable())
                pos = startOfEditableContent(pos);
            else 
                pos = startOfDocument(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingLeftBackward(TextGranularity granularity)
{
    VisiblePosition pos;
    // FIXME: Stay in editable content for the less common granularities.
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            else
                pos = VisiblePosition(m_sel.extent(), m_sel.affinity()).previous(true);
            break;
        case WordGranularity:
            pos = previousWordPosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case SentenceGranularity:
            pos = previousSentencePosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case LineGranularity:
            pos = previousLinePosition(VisiblePosition(m_sel.start(), m_sel.affinity()), xPosForVerticalArrowNavigation(START, isRange()));
            break;
        case ParagraphGranularity:
            pos = previousParagraphPosition(VisiblePosition(m_sel.start(), m_sel.affinity()), xPosForVerticalArrowNavigation(START, isRange()));
            break;
        case SentenceBoundary:
            pos = startOfSentence(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = startOfLine(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = startOfParagraph(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            if (pos.deepEquivalent().node()->isContentEditable())
                pos = startOfEditableContent(pos);
            else 
                pos = startOfDocument(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
    }
    return pos;
}

bool SelectionController::modify(const String &alterString, const String &directionString, const String &granularityString)
{
    String alterStringLower = alterString.lower();
    EAlter alter;
    if (alterStringLower == "extend")
        alter = EXTEND;
    else if (alterStringLower == "move")
        alter = MOVE;
    else 
        return false;
    
    String directionStringLower = directionString.lower();
    EDirection direction;
    if (directionStringLower == "forward")
        direction = FORWARD;
    else if (directionStringLower == "backward")
        direction = BACKWARD;
    else if (directionStringLower == "left")
        direction = LEFT;
    else if (directionStringLower == "right")
        direction = RIGHT;
    else
        return false;
        
    String granularityStringLower = granularityString.lower();
    TextGranularity granularity;
    if (granularityStringLower == "character")
        granularity = CharacterGranularity;
    else if (granularityStringLower == "word")
        granularity = WordGranularity;
    else if (granularityStringLower == "sentence")
        granularity = SentenceGranularity;
    else if (granularityStringLower == "line")
        granularity = LineGranularity;
    else if (granularityStringLower == "paragraph")
        granularity = ParagraphGranularity;
    else
        return false;
                
    return modify(alter, direction, granularity);
}

bool SelectionController::modify(EAlter alter, EDirection dir, TextGranularity granularity)
{
    if (frame())
        frame()->setSelectionGranularity(granularity);
    
    setModifyBias(alter, dir);

    VisiblePosition pos;
    switch (dir) {
        // EDIT FIXME: These need to handle bidi
        case RIGHT:
        case FORWARD:
            if (alter == EXTEND)
                pos = modifyExtendingRightForward(granularity);
            else
                pos = modifyMovingRightForward(granularity);
            break;
        case LEFT:
        case BACKWARD:
            if (alter == EXTEND)
                pos = modifyExtendingLeftBackward(granularity);
            else
                pos = modifyMovingLeftBackward(granularity);
            break;
    }

    if (pos.isNull())
        return false;

    switch (alter) {
        case MOVE:
            moveTo(pos);
            break;
        case EXTEND:
            setExtent(pos);
            break;
    }

    setNeedsLayout();

    return true;
}

// FIXME: Maybe baseline would be better?
static bool caretY(const VisiblePosition &c, int &y)
{
    Position p = c.deepEquivalent();
    Node *n = p.node();
    if (!n)
        return false;
    RenderObject *r = p.node()->renderer();
    if (!r)
        return false;
    IntRect rect = r->caretRect(p.offset());
    if (rect.isEmpty())
        return false;
    y = rect.y() + rect.height() / 2;
    return true;
}

bool SelectionController::modify(EAlter alter, int verticalDistance)
{
    if (verticalDistance == 0)
        return false;

    bool up = verticalDistance < 0;
    if (up)
        verticalDistance = -verticalDistance;

    setModifyBias(alter, up ? BACKWARD : FORWARD);

    VisiblePosition pos;
    int xPos = 0;
    switch (alter) {
        case MOVE:
            pos = VisiblePosition(up ? m_sel.start() : m_sel.end(), m_sel.affinity());
            xPos = xPosForVerticalArrowNavigation(up ? START : END, isRange());
            m_sel.setAffinity(up ? UPSTREAM : DOWNSTREAM);
            break;
        case EXTEND:
            pos = VisiblePosition(m_sel.extent(), m_sel.affinity());
            xPos = xPosForVerticalArrowNavigation(EXTENT);
            m_sel.setAffinity(DOWNSTREAM);
            break;
    }

    int startY;
    if (!caretY(pos, startY))
        return false;
    if (up)
        startY = -startY;
    int lastY = startY;

    VisiblePosition result;
    VisiblePosition next;
    for (VisiblePosition p = pos; ; p = next) {
        next = (up ? previousLinePosition : nextLinePosition)(p, xPos);
        if (next.isNull() || next == p)
            break;
        int nextY;
        if (!caretY(next, nextY))
            break;
        if (up)
            nextY = -nextY;
        if (nextY - startY > verticalDistance)
            break;
        if (nextY >= lastY) {
            lastY = nextY;
            result = next;
        }
    }

    if (result.isNull())
        return false;

    switch (alter) {
        case MOVE:
            moveTo(result);
            break;
        case EXTEND:
            setExtent(result);
            break;
    }

    return true;
}

bool SelectionController::expandUsingGranularity(TextGranularity granularity)
{
    if (isNone())
        return false;

    m_sel.expandUsingGranularity(granularity);
    m_needsLayout = true;
    return true;
}

int SelectionController::xPosForVerticalArrowNavigation(EPositionType type, bool recalc) const
{
    int x = 0;

    if (isNone())
        return x;

    Position pos;
    switch (type) {
        case START:
            pos = m_sel.start();
            break;
        case END:
            pos = m_sel.end();
            break;
        case BASE:
            pos = m_sel.base();
            break;
        case EXTENT:
            pos = m_sel.extent();
            break;
    }

    Frame *frame = pos.node()->document()->frame();
    if (!frame)
        return x;
        
    if (recalc || frame->xPosForVerticalArrowNavigation() == Frame::NoXPosForVerticalArrowNavigation) {
        pos = VisiblePosition(pos, m_sel.affinity()).deepEquivalent();
        x = pos.node()->renderer()->caretRect(pos.offset(), m_sel.affinity()).x();
        frame->setXPosForVerticalArrowNavigation(x);
    }
    else {
        x = frame->xPosForVerticalArrowNavigation();
    }
    return x;
}

void SelectionController::clear()
{
    setSelection(Selection());
    m_needsLayout = true;
}

void SelectionController::setBase(const VisiblePosition &pos)
{
    setSelection(Selection(pos.deepEquivalent(), m_sel.extent(), pos.affinity()));
    m_needsLayout = true;
}

void SelectionController::setExtent(const VisiblePosition &pos)
{
    setSelection(Selection(m_sel.base(), pos.deepEquivalent(), pos.affinity()));
    m_needsLayout = true;
}

void SelectionController::setBase(const Position &pos, EAffinity affinity)
{
    setSelection(Selection(pos, m_sel.extent(), affinity));
    m_needsLayout = true;
}

void SelectionController::setExtent(const Position &pos, EAffinity affinity)
{
    setSelection(Selection(m_sel.base(), pos, affinity));
    m_needsLayout = true;
}

void SelectionController::setNeedsLayout(bool flag)
{
    m_needsLayout = flag;
}

String SelectionController::type() const
{
    if (isNone())
        return "None";
    else if (isCaret())
        return "Caret";
    else
        return "Range";
}

String SelectionController::toString() const
{
    return String(plainText(m_sel.toRange().get()));
}

PassRefPtr<Range> SelectionController::getRangeAt(int index) const
{
    return index == 0 ? m_sel.toRange() : 0;
}

Frame *SelectionController::frame() const
{
    return !isNone() ? m_sel.start().node()->document()->frame() : 0;
}

void SelectionController::setBaseAndExtent(Node *baseNode, int baseOffset, Node *extentNode, int extentOffset)
{
    VisiblePosition visibleBase = VisiblePosition(baseNode, baseOffset, DOWNSTREAM);
    VisiblePosition visibleExtent = VisiblePosition(extentNode, extentOffset, DOWNSTREAM);
    
    moveTo(visibleBase, visibleExtent);
}

void SelectionController::setPosition(Node *node, int offset)
{
    moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void SelectionController::collapse(Node *node, int offset)
{
    moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void SelectionController::collapseToEnd()
{
    moveTo(VisiblePosition(m_sel.end(), DOWNSTREAM));
}

void SelectionController::collapseToStart()
{
    moveTo(VisiblePosition(m_sel.start(), DOWNSTREAM));
}

void SelectionController::empty()
{
    moveTo(SelectionController());
}

void SelectionController::extend(Node *node, int offset)
{
    moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void SelectionController::layout()
{
    if (isNone() || !m_sel.start().node()->inDocument() || !m_sel.end().node()->inDocument()) {
        m_caretRect = IntRect();
        m_caretPositionOnLayout = IntPoint();
        return;
    }

    m_sel.start().node()->document()->updateRendering();
    
    m_caretRect = IntRect();
    m_caretPositionOnLayout = IntPoint();
        
    if (isCaret()) {
        Position pos = m_sel.start();
        pos = VisiblePosition(m_sel.start(), m_sel.affinity()).deepEquivalent();
        if (pos.isNotNull()) {
            ASSERT(pos.node()->renderer());
            m_caretRect = pos.node()->renderer()->caretRect(pos.offset(), m_sel.affinity());
            
            int x, y;
            pos.node()->renderer()->absolutePositionForContent(x, y);
            m_caretPositionOnLayout = IntPoint(x, y);
        }
    }

    m_needsLayout = false;
}

IntRect SelectionController::caretRect() const
{
    if (m_needsLayout)
        const_cast<SelectionController *>(this)->layout();
    
    IntRect caret = m_caretRect;

    if (m_sel.start().node() && m_sel.start().node()->renderer()) {
        int x, y;
        m_sel.start().node()->renderer()->absolutePositionForContent(x, y);
        caret.move(IntPoint(x, y) - m_caretPositionOnLayout);
    }

    return caret;
}

IntRect SelectionController::caretRepaintRect() const
{
    // FIXME: Add one pixel of slop on each side to make sure we don't leave behind artifacts.
    IntRect r = caretRect();
    if (r.isEmpty())
        return IntRect();
    return IntRect(r.x() - 1, r.y() - 1, r.width() + 2, r.height() + 2);
}

void SelectionController::needsCaretRepaint()
{
    if (!isCaret())
        return;

    FrameView *v = m_sel.start().node()->document()->view();
    if (!v)
        return;

    if (m_needsLayout) {
        // repaint old position and calculate new position
        v->updateContents(caretRepaintRect(), false);
        layout();
        
        // EDIT FIXME: This is an unfortunate hack.
        // Basically, we can't trust this layout position since we 
        // can't guarantee that the check to see if we are in unrendered 
        // content will work at this point. We may have to wait for
        // a layout and re-render of the document to happen. So, resetting this
        // flag will cause another caret layout to happen the first time
        // that we try to paint the caret after this call. That one will work since
        // it happens after the document has accounted for any editing
        // changes which may have been done.
        // And, we need to leave this layout here so the caret moves right 
        // away after clicking.
        m_needsLayout = true;
    }
    v->updateContents(caretRepaintRect(), false);
}

void SelectionController::paintCaret(GraphicsContext *p, const IntRect &rect)
{
    if (! m_sel.isCaret())
        return;

    if (m_needsLayout)
        layout();
        
    IntRect caret = intersection(caretRect(), rect);
    if (!caret.isEmpty())
    { 
        p->fillRect(caret, Color(0, 84, 166));
    }
}

void SelectionController::debugRenderer(RenderObject *r, bool selected) const
{
    if (r->node()->isElementNode()) {
        Element *element = static_cast<Element *>(r->node());
        fprintf(stderr, "%s%s\n", selected ? "==> " : "    ", element->localName().deprecatedString().latin1());
    }
    else if (r->isText()) {
        RenderText *textRenderer = static_cast<RenderText *>(r);
        if (textRenderer->stringLength() == 0 || !textRenderer->firstTextBox()) {
            fprintf(stderr, "%s#text (empty)\n", selected ? "==> " : "    ");
            return;
        }
        
        static const int max = 36;
        DeprecatedString text = String(textRenderer->string()).deprecatedString();
        int textLength = text.length();
        if (selected) {
            int offset = 0;
            if (r->node() == m_sel.start().node())
                offset = m_sel.start().offset();
            else if (r->node() == m_sel.end().node())
                offset = m_sel.end().offset();
                
            int pos;
            InlineTextBox *box = textRenderer->findNextInlineTextBox(offset, pos);
            text = text.mid(box->m_start, box->m_len);
            
            DeprecatedString show;
            int mid = max / 2;
            int caret = 0;
            
            // text is shorter than max
            if (textLength < max) {
                show = text;
                caret = pos;
            }
            
            // too few characters to left
            else if (pos - mid < 0) {
                show = text.left(max - 3) + "...";
                caret = pos;
            }
            
            // enough characters on each side
            else if (pos - mid >= 0 && pos + mid <= textLength) {
                show = "..." + text.mid(pos - mid + 3, max - 6) + "...";
                caret = mid;
            }
            
            // too few characters on right
            else {
                show = "..." + text.right(max - 3);
                caret = pos - (textLength - show.length());
            }
            
            show.replace('\n', ' ');
            show.replace('\r', ' ');
            fprintf(stderr, "==> #text : \"%s\" at offset %d\n", show.latin1(), pos);
            fprintf(stderr, "           ");
            for (int i = 0; i < caret; i++)
                fprintf(stderr, " ");
            fprintf(stderr, "^\n");
        }
        else {
            if ((int)text.length() > max)
                text = text.left(max - 3) + "...";
            else
                text = text.left(max);
            fprintf(stderr, "    #text : \"%s\"\n", text.latin1());
        }
    }
}

#ifndef NDEBUG

void SelectionController::formatForDebugger(char* buffer, unsigned length) const
{
    m_sel.formatForDebugger(buffer, length);
}

void SelectionController::showTreeForThis() const
{
    m_sel.showTreeForThis();
}

#endif

}

#ifndef NDEBUG

void showTree(const WebCore::SelectionController& sel)
{
    sel.showTreeForThis();
}

void showTree(const WebCore::SelectionController* sel)
{
    if (sel)
        sel->showTreeForThis();
}

#endif
