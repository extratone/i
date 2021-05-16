/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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

#include "CString.h"
#include "DeleteSelectionCommand.h"
#include "Document.h"
#include "Editor.h"
#include "EditorClient.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "Element.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FocusController.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Page.h"
#include "Range.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <stdio.h>

#define EDIT_DEBUG 0

namespace WebCore {

using namespace HTMLNames;

const int NoXPosForVerticalArrowNavigation = INT_MIN;

SelectionController::SelectionController(Frame* frame, bool isDragCaretController)
    : m_frame(frame)
    , m_xPosForVerticalArrowNavigation(NoXPosForVerticalArrowNavigation)
    , m_needsLayout(true)
    , m_absCaretBoundsDirty(true)
    , m_lastChangeWasHorizontalExtension(false)
    , m_isDragCaretController(isDragCaretController)
    , m_isCaretBlinkingSuspended(false)
    , m_focused(false)
    , m_closeTypingSuppressions(0)
{
}

void SelectionController::moveTo(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(Selection(pos.deepEquivalent(), pos.deepEquivalent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::moveTo(const VisiblePosition &base, const VisiblePosition &extent, bool userTriggered)
{
    setSelection(Selection(base.deepEquivalent(), extent.deepEquivalent(), base.affinity()), true, true, userTriggered);
}

void SelectionController::moveTo(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(pos, affinity), true, true, userTriggered);
}

void SelectionController::moveTo(const Range *r, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(startPosition(r), endPosition(r), affinity), true, true, userTriggered);
}

void SelectionController::moveTo(const Position &base, const Position &extent, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(base, extent, affinity), true, true, userTriggered);
}

void SelectionController::setSelection(const Selection& s, bool closeTyping, bool clearTypingStyle, bool userTriggered)
{
    if (m_isDragCaretController) {
        invalidateCaretRect();
        m_sel = s;
        m_needsLayout = true;
        invalidateCaretRect();
        return;
    }
    if (!m_frame) {
        m_sel = s;
        return;
    }

    Node* baseNode = s.base().node();
    Document* document = 0;
    if (baseNode)
        document = baseNode->document();
    
    // <http://bugs.webkit.org/show_bug.cgi?id=23464>: Infinite recursion at SelectionController::setSelection
    // if document->frame() == m_frame we can get into an infinite loop
    if (document && document->frame() != m_frame && document != m_frame->document()) {
        document->frame()->selection()->setSelection(s, closeTyping, clearTypingStyle, userTriggered);
        return;
    }
    
    if (closeTyping && m_closeTypingSuppressions == 0)
        TypingCommand::closeTyping(m_frame->editor()->lastEditCommand());

    if (clearTypingStyle)
        m_frame->clearTypingStyle();
        
    if (m_sel == s)
        return;
    
    Selection oldSelection = m_sel;

    m_sel = s;
    
    m_needsLayout = true;
    
    if (!s.isNone())
        m_frame->setFocusedNodeIfNeeded();
    
    m_frame->selectionLayoutChanged();
    // Always clear the x position used for vertical arrow navigation.
    // It will be restored by the vertical arrow navigation code if necessary.
    m_xPosForVerticalArrowNavigation = NoXPosForVerticalArrowNavigation;
    selectFrameElementInParentIfFullySelected();
    m_frame->notifyRendererOfSelectionChange(userTriggered);
    m_frame->respondToChangedSelection(oldSelection, closeTyping);
    if (userTriggered)
        m_frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded, true);

    notifyAccessibilityForSelectionChange();
}

static bool removingNodeRemovesPosition(Node* node, const Position& position)
{
    if (!position.node())
        return false;
        
    if (position.node() == node)
        return true;
    
    if (!node->isElementNode())
        return false;
    
    Element* element = static_cast<Element*>(node);
    return element->contains(position.node()) || element->contains(position.node()->shadowAncestorNode());
}

void SelectionController::nodeWillBeRemoved(Node *node)
{
    if (isNone())
        return;
        
    // There can't be a selection inside a fragment, so if a fragment's node is being removed,
    // the selection in the document that created the fragment needs no adjustment.
    if (node && highestAncestor(node)->nodeType() == Node::DOCUMENT_FRAGMENT_NODE)
        return;
    
    bool baseRemoved = removingNodeRemovesPosition(node, m_sel.base());
    bool extentRemoved = removingNodeRemovesPosition(node, m_sel.extent());
    bool startRemoved = removingNodeRemovesPosition(node, m_sel.start());
    bool endRemoved = removingNodeRemovesPosition(node, m_sel.end());
    
    bool clearRenderTreeSelection = false;
    bool clearDOMTreeSelection = false;

    if (startRemoved || endRemoved) {
        // FIXME: When endpoints are removed, we should just alter the selection, instead of blowing it away.
        clearRenderTreeSelection = true;
        clearDOMTreeSelection = true;
    } else if (baseRemoved || extentRemoved) {
        // The base and/or extent are about to be removed, but the start and end aren't.
        // Change the base and extent to the start and end, but don't re-validate the
        // selection, since doing so could move the start and end into the node
        // that is about to be removed.
        if (m_sel.isBaseFirst())
            m_sel.setWithoutValidation(m_sel.start(), m_sel.end());
        else
            m_sel.setWithoutValidation(m_sel.end(), m_sel.start());
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
        RefPtr<Document> document = m_sel.start().node()->document();
        document->updateRendering();
        if (RenderView* view = static_cast<RenderView*>(document->renderer()))
            view->clearSelection();
    }

    if (clearDOMTreeSelection)
        setSelection(Selection(), false, false);
}

void SelectionController::willBeModified(EAlteration alter, EDirection direction)
{
    switch (alter) {
        case MOVE:
            m_lastChangeWasHorizontalExtension = false;
            break;
        case EXTEND:
            if (!m_lastChangeWasHorizontalExtension) {
                m_lastChangeWasHorizontalExtension = true;
                Position start = m_sel.start();
                Position end = m_sel.end();
                switch (direction) {
                    // FIXME: right for bidi?
                    case RIGHT:
                    case FORWARD:
                        m_sel.setBase(start);
                        m_sel.setExtent(end);
                        break;
                    case LEFT:
                    case BACKWARD:
                        m_sel.setBase(end);
                        m_sel.setExtent(start);
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
            pos = pos.next(true);
            break;
        case WordGranularity:
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
            if (isEditablePosition(pos.deepEquivalent()))
                pos = endOfEditableContent(pos);
            else
                pos = endOfDocument(pos);
            break;
    }
    
    return pos;
}

VisiblePosition SelectionController::modifyMovingRight(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            else
                pos = VisiblePosition(m_sel.extent(), m_sel.affinity()).right(true);
            break;
        case WordGranularity:
        case SentenceGranularity:
        case LineGranularity:
        case ParagraphGranularity:
        case SentenceBoundary:
        case LineBoundary:
        case ParagraphBoundary:
        case DocumentBoundary:
            // FIXME: Implement all of the above.
            pos = modifyMovingForward(granularity);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingForward(TextGranularity granularity)
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
                pos = nextLinePosition(pos, xPosForVerticalArrowNavigation(START));
            break;
        }
        case ParagraphGranularity:
            pos = nextParagraphPosition(VisiblePosition(m_sel.end(), m_sel.affinity()), xPosForVerticalArrowNavigation(START));
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
            if (isEditablePosition(pos.deepEquivalent()))
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
            pos = pos.previous(true);
            break;
        case WordGranularity:
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
            if (isEditablePosition(pos.deepEquivalent()))
                pos = startOfEditableContent(pos);
            else 
                pos = startOfDocument(pos);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingLeft(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            else
                pos = VisiblePosition(m_sel.extent(), m_sel.affinity()).left(true);
            break;
        case WordGranularity:
        case SentenceGranularity:
        case LineGranularity:
        case ParagraphGranularity:
        case SentenceBoundary:
        case LineBoundary:
        case ParagraphBoundary:
        case DocumentBoundary:
            // FIXME: Implement all of the above.
            pos = modifyMovingBackward(granularity);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingBackward(TextGranularity granularity)
{
    VisiblePosition pos;
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
            pos = previousLinePosition(VisiblePosition(m_sel.start(), m_sel.affinity()), xPosForVerticalArrowNavigation(START));
            break;
        case ParagraphGranularity:
            pos = previousParagraphPosition(VisiblePosition(m_sel.start(), m_sel.affinity()), xPosForVerticalArrowNavigation(START));
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
            if (isEditablePosition(pos.deepEquivalent()))
                pos = startOfEditableContent(pos);
            else 
                pos = startOfDocument(pos);
            break;
    }
    return pos;
}

bool SelectionController::modify(EAlteration alter, EDirection dir, TextGranularity granularity, bool userTriggered)
{
    if (userTriggered) {
        SelectionController trialSelectionController;
        trialSelectionController.setLastChangeWasHorizontalExtension(m_lastChangeWasHorizontalExtension);
        trialSelectionController.setSelection(m_sel);
        trialSelectionController.modify(alter, dir, granularity, false);

        bool change = m_frame->shouldChangeSelection(trialSelectionController.selection());
        if (!change)
            return false;
    }

    if (m_frame)
        m_frame->setSelectionGranularity(granularity);
    
    willBeModified(alter, dir);

    VisiblePosition pos;
    switch (dir) {
        case RIGHT:
            if (alter == MOVE)
                pos = modifyMovingRight(granularity);
            else
                pos = modifyExtendingRightForward(granularity);
            break;
        case FORWARD:
            if (alter == EXTEND)
                pos = modifyExtendingRightForward(granularity);
            else
                pos = modifyMovingForward(granularity);
            break;
        case LEFT:
            if (alter == MOVE)
                pos = modifyMovingLeft(granularity);
            else
                pos = modifyExtendingLeftBackward(granularity);
            break;
        case BACKWARD:
            if (alter == EXTEND)
                pos = modifyExtendingLeftBackward(granularity);
            else
                pos = modifyMovingBackward(granularity);
            break;
    }

    if (pos.isNull())
        return false;
    
    // Some of the above operations set an xPosForVerticalArrowNavigation.
    // Setting a selection will clear it, so save it to possibly restore later.
    // Note: the START position type is arbitrary because it is unused, it would be
    // the requested position type if there were no xPosForVerticalArrowNavigation set.
    int x = xPosForVerticalArrowNavigation(START);

    switch (alter) {
        case MOVE:
            moveTo(pos, userTriggered);
            break;
        case EXTEND:
            setExtent(pos, userTriggered);
            break;
    }
    
    if (granularity == LineGranularity || granularity == ParagraphGranularity)
        m_xPosForVerticalArrowNavigation = x;

    if (userTriggered) {
        // User modified selection change also sets the granularity back to character.
        // NOTE: The one exception is that we need to keep word granularity to
        // preserve smart delete behavior when extending by word (e.g. double-click),
        // then shift-option-right arrow, then delete needs to smart delete, per TextEdit.
        if (!(alter == EXTEND && granularity == WordGranularity && m_frame->selectionGranularity() == WordGranularity))
            m_frame->setSelectionGranularity(CharacterGranularity);
    }

    setNeedsLayout();

    return true;
}

// FIXME: Maybe baseline would be better?
static bool absoluteCaretY(const VisiblePosition &c, int &y)
{
    IntRect rect = c.absoluteCaretBounds();
    if (rect.isEmpty())
        return false;
    y = rect.y() + rect.height() / 2;
    return true;
}

bool SelectionController::modify(EAlteration alter, int verticalDistance, bool userTriggered)
{
    if (verticalDistance == 0)
        return false;

    if (userTriggered) {
        SelectionController trialSelectionController;
        trialSelectionController.setSelection(m_sel);
        trialSelectionController.modify(alter, verticalDistance, false);

        bool change = m_frame->shouldChangeSelection(trialSelectionController.selection());
        if (!change)
            return false;
    }

    bool up = verticalDistance < 0;
    if (up)
        verticalDistance = -verticalDistance;

    willBeModified(alter, up ? BACKWARD : FORWARD);

    VisiblePosition pos;
    int xPos = 0;
    switch (alter) {
        case MOVE:
            pos = VisiblePosition(up ? m_sel.start() : m_sel.end(), m_sel.affinity());
            xPos = xPosForVerticalArrowNavigation(up ? START : END);
            m_sel.setAffinity(up ? UPSTREAM : DOWNSTREAM);
            break;
        case EXTEND:
            pos = VisiblePosition(m_sel.extent(), m_sel.affinity());
            xPos = xPosForVerticalArrowNavigation(EXTENT);
            m_sel.setAffinity(DOWNSTREAM);
            break;
    }

    int startY;
    if (!absoluteCaretY(pos, startY))
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
        if (!absoluteCaretY(next, nextY))
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
            moveTo(result, userTriggered);
            break;
        case EXTEND:
            setExtent(result, userTriggered);
            break;
    }

    if (userTriggered)
        m_frame->setSelectionGranularity(CharacterGranularity);

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

int SelectionController::xPosForVerticalArrowNavigation(EPositionType type)
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
        
    if (m_xPosForVerticalArrowNavigation == NoXPosForVerticalArrowNavigation) {
        VisiblePosition visiblePosition(pos, m_sel.affinity());
        // VisiblePosition creation can fail here if a node containing the selection becomes visibility:hidden
        // after the selection is created and before this function is called.
        x = visiblePosition.isNotNull() ? visiblePosition.xOffsetForVerticalNavigation() : 0;
        m_xPosForVerticalArrowNavigation = x;
    }
    else
        x = m_xPosForVerticalArrowNavigation;
        
    return x;
}

void SelectionController::clear()
{
    setSelection(Selection());
}

void SelectionController::setBase(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(Selection(pos.deepEquivalent(), m_sel.extent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::setExtent(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(Selection(m_sel.base(), pos.deepEquivalent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::setBase(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(pos, m_sel.extent(), affinity), true, true, userTriggered);
}

void SelectionController::setExtent(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(m_sel.base(), pos, affinity), true, true, userTriggered);
}

void SelectionController::setNeedsLayout(bool flag)
{
    m_needsLayout = flag;
}

void SelectionController::layout()
{
    if (isNone() || !m_sel.start().node()->inDocument() || !m_sel.end().node()->inDocument()) {
        m_caretRect = IntRect();
        return;
    }

    m_sel.start().node()->document()->updateRendering();
    
    m_caretRect = IntRect();
        
    if (isCaret()) {
        VisiblePosition pos(m_sel.start(), m_sel.affinity());
        if (pos.isNotNull()) {
            ASSERT(pos.deepEquivalent().node()->renderer());
            
            // First compute a rect local to the renderer at the selection start
            RenderObject* renderer;
            IntRect localRect = pos.localCaretRect(renderer);

            // Get the renderer that will be responsible for painting the caret (which
            // is either the renderer we just found, or one of its containers)
            RenderObject* caretPainter = caretRenderer();

            // Compute an offset between the renderer and the caretPainter
            IntSize offsetFromPainter;
            bool unrooted = false;
            while (renderer != caretPainter) {
                RenderObject* containerObject = renderer->container();
                if (!containerObject) {
                    unrooted = true;
                    break;
                }
                offsetFromPainter += renderer->offsetFromContainer(containerObject);
                renderer = containerObject;
            }
            
            if (!unrooted) {
                // Move the caret rect to the coords of the painter
                localRect.move(offsetFromPainter);
                m_caretRect = localRect;
            }
            
            m_absCaretBoundsDirty = true;
        }
    }

    m_needsLayout = false;
}

RenderObject* SelectionController::caretRenderer() const
{
    Node* node = m_sel.start().node();
    if (!node)
        return 0;

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return 0;

    // if caretNode is a block and caret is inside it then caret should be painted by that block
    bool paintedByBlock = renderer->isBlockFlow() && caretRendersInsideNode(node);
    return paintedByBlock ? renderer : renderer->containingBlock();
}

IntRect SelectionController::localCaretRect() const
{
    if (m_needsLayout)
        const_cast<SelectionController*>(this)->layout();
    
    return m_caretRect;
}

IntRect SelectionController::absoluteBoundsForLocalRect(const IntRect& rect) const
{
    RenderObject* caretPainter = caretRenderer();
    if (!caretPainter)
        return IntRect();
        
    return caretPainter->localToAbsoluteQuad(FloatRect(rect)).enclosingBoundingBox();
}

IntRect SelectionController::absoluteCaretBounds()
{
    recomputeCaretRect();
    return m_absCaretBounds;
}

static IntRect repaintRectForCaret(IntRect caret)
{
    if (caret.isEmpty())
        return IntRect();
    // Ensure that the dirty rect intersects the block that paints the caret even in the case where
    // the caret itself is just outside the block. See <https://bugs.webkit.org/show_bug.cgi?id=19086>.
    caret.inflateX(1);
    return caret;
}

IntRect SelectionController::caretRepaintRect() const
{
    return absoluteBoundsForLocalRect(repaintRectForCaret(localCaretRect()));
}

bool SelectionController::recomputeCaretRect()
{
    if (!m_frame || !m_frame->document())
        return false;
        
    FrameView* v = m_frame->document()->view();
    if (!v)
        return false;

    if (!m_needsLayout)
        return false;

    IntRect oldRect = m_caretRect;
    IntRect newRect = localCaretRect();
    if (oldRect == newRect && !m_absCaretBoundsDirty)
        return false;

    IntRect oldAbsCaretBounds = m_absCaretBounds;
    // FIXME: Rename m_caretRect to m_localCaretRect.
    m_absCaretBounds = absoluteBoundsForLocalRect(m_caretRect);
    
    if (oldAbsCaretBounds == m_absCaretBounds)
        return false;
        
    IntRect oldAbsoluteCaretRepaintBounds = m_absoluteCaretRepaintBounds;
    // We believe that we need to inflate the local rect before transforming it to obtain the repaint bounds.
    m_absoluteCaretRepaintBounds = caretRepaintRect();
    
    if (RenderView* view = static_cast<RenderView*>(m_frame->document()->renderer())) {
        // FIXME: make caret repainting container-aware.
        view->repaintRectangleInViewAndCompositedLayers(oldAbsoluteCaretRepaintBounds, false);
        view->repaintRectangleInViewAndCompositedLayers(m_absoluteCaretRepaintBounds, false);
    }

    return true;
}

void SelectionController::invalidateCaretRect()
{
    if (!isCaret())
        return;

    Document* d = m_sel.start().node()->document();

    // recomputeCaretRect will always return false for the drag caret,
    // because its m_frame is always 0.
    bool caretRectChanged = recomputeCaretRect();

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

    if (!caretRectChanged) {
        if (RenderView* view = static_cast<RenderView*>(d->renderer()))
            view->repaintRectangleInViewAndCompositedLayers(caretRepaintRect(), false);
    }
}

void SelectionController::paintCaret(GraphicsContext* p, int tx, int ty, const IntRect& clipRect)
{
    return;
    if (! m_sel.isCaret())
        return;

    if (m_needsLayout)
        layout();

    IntRect drawingRect = localCaretRect();
    drawingRect.move(tx, ty);
    IntRect caret = intersection(drawingRect, clipRect);
    if (!caret.isEmpty()) {
        Color caretColor = Color::black;
        Element* element = rootEditableElement();
        if (element && element->renderer())
            caretColor = element->renderer()->style()->color();

        p->fillRect(caret, caretColor);
    }
}

void SelectionController::debugRenderer(RenderObject *r, bool selected) const
{
    if (r->node()->isElementNode()) {
        Element *element = static_cast<Element *>(r->node());
        fprintf(stderr, "%s%s\n", selected ? "==> " : "    ", element->localName().string().utf8().data());
    }
    else if (r->isText()) {
        RenderText* textRenderer = toRenderText(r);
        if (textRenderer->textLength() == 0 || !textRenderer->firstTextBox()) {
            fprintf(stderr, "%s#text (empty)\n", selected ? "==> " : "    ");
            return;
        }
        
        static const int max = 36;
        String text = textRenderer->text();
        int textLength = text.length();
        if (selected) {
            int offset = 0;
            if (r->node() == m_sel.start().node())
                offset = m_sel.start().offset();
            else if (r->node() == m_sel.end().node())
                offset = m_sel.end().offset();
                
            int pos;
            InlineTextBox *box = textRenderer->findNextInlineTextBox(offset, pos);
            text = text.substring(box->start(), box->len());
            
            String show;
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
                show = "..." + text.substring(pos - mid + 3, max - 6) + "...";
                caret = mid;
            }
            
            // too few characters on right
            else {
                show = "..." + text.right(max - 3);
                caret = pos - (textLength - show.length());
            }
            
            show.replace('\n', ' ');
            show.replace('\r', ' ');
            fprintf(stderr, "==> #text : \"%s\" at offset %d\n", show.utf8().data(), pos);
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
            fprintf(stderr, "    #text : \"%s\"\n", text.utf8().data());
        }
    }
}

bool SelectionController::contains(const IntPoint& point)
{
    Document* document = m_frame->document();
    
    // Treat a collapsed selection like no selection.
    if (!isRange())
        return false;
    if (!document->renderer()) 
        return false;
    
    HitTestRequest request(true, true);
    HitTestResult result(point);
    document->renderView()->layer()->hitTest(request, result);
    Node* innerNode = result.innerNode();
    if (!innerNode || !innerNode->renderer())
        return false;
    
    VisiblePosition visiblePos(innerNode->renderer()->positionForPoint(result.localPoint()));
    if (visiblePos.isNull())
        return false;
        
    if (m_sel.visibleStart().isNull() || m_sel.visibleEnd().isNull())
        return false;
        
    Position start(m_sel.visibleStart().deepEquivalent());
    Position end(m_sel.visibleEnd().deepEquivalent());
    Position p(visiblePos.deepEquivalent());

    return comparePositions(start, p) <= 0 && comparePositions(p, end) <= 0;
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
// Can't do this implicitly as part of every setSelection call because in some contexts it might not be good
// for the focus to move to another frame. So instead we call it from places where we are selecting with the
// mouse or the keyboard after setting the selection.
void SelectionController::selectFrameElementInParentIfFullySelected()
{
    // Find the parent frame; if there is none, then we have nothing to do.
    Frame* parent = m_frame->tree()->parent();
    if (!parent)
        return;
    Page* page = m_frame->page();
    if (!page)
        return;

    // Check if the selection contains the entire frame contents; if not, then there is nothing to do.
    if (!isRange())
        return;
    if (!isStartOfDocument(selection().visibleStart()))
        return;
    if (!isEndOfDocument(selection().visibleEnd()))
        return;

    // Get to the <iframe> or <frame> (or even <object>) element in the parent frame.
    Document* doc = m_frame->document();
    if (!doc)
        return;
    Element* ownerElement = doc->ownerElement();
    if (!ownerElement)
        return;
    Node* ownerElementParent = ownerElement->parentNode();
    if (!ownerElementParent)
        return;
        
    // This method's purpose is it to make it easier to select iframes (in order to delete them).  Don't do anything if the iframe isn't deletable.
    if (!ownerElementParent->isContentEditable())
        return;

    // Create compute positions before and after the element.
    unsigned ownerElementNodeIndex = ownerElement->nodeIndex();
    VisiblePosition beforeOwnerElement(VisiblePosition(ownerElementParent, ownerElementNodeIndex, SEL_DEFAULT_AFFINITY));
    VisiblePosition afterOwnerElement(VisiblePosition(ownerElementParent, ownerElementNodeIndex + 1, VP_UPSTREAM_IF_POSSIBLE));

    // Focus on the parent frame, and then select from before this element to after.
    Selection newSelection(beforeOwnerElement, afterOwnerElement);
    if (parent->shouldChangeSelection(newSelection)) {
        page->focusController()->setFocusedFrame(parent);
        parent->selection()->setSelection(newSelection);
    }
}

void SelectionController::selectAll()
{
    Document* document = m_frame->document();
    if (!document)
        return;
    
    if (document->focusedNode() && document->focusedNode()->canSelectAll()) {
        document->focusedNode()->selectAll();
        return;
    }
    
    Node* root = 0;
    if (isContentEditable())
        root = highestEditableRoot(m_sel.start());
    else {
        root = shadowTreeRootNode();
        if (!root)
            root = document->documentElement();
    }
    if (!root)
        return;
    Selection newSelection(Selection::selectionFromContentsOfNode(root));
    if (m_frame->shouldChangeSelection(newSelection))
        setSelection(newSelection);
    selectFrameElementInParentIfFullySelected();
    m_frame->notifyRendererOfSelectionChange(true);
}

bool SelectionController::setSelectedRange(Range* range, EAffinity affinity, bool closeTyping)
{
    if (!range)
        return false;

    ExceptionCode ec = 0;
    Node* startContainer = range->startContainer(ec);
    if (ec)
        return false;

    Node* endContainer = range->endContainer(ec);
    if (ec)
        return false;
    
    ASSERT(startContainer);
    ASSERT(endContainer);
    ASSERT(startContainer->document() == endContainer->document());
    
    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    // Non-collapsed ranges are not allowed to start at the end of a line that is wrapped,
    // they start at the beginning of the next line instead
    bool collapsed = range->collapsed(ec);
    if (ec)
        return false;
    
    int startOffset = range->startOffset(ec);
    if (ec)
        return false;

    int endOffset = range->endOffset(ec);
    if (ec)
        return false;
    
    // FIXME: Can we provide extentAffinity?
    VisiblePosition visibleStart(startContainer, startOffset, collapsed ? affinity : DOWNSTREAM);
    VisiblePosition visibleEnd(endContainer, endOffset, SEL_DEFAULT_AFFINITY);
    if (startContainer && visibleStart.isNull())
        return false;
    if (endContainer && visibleEnd.isNull())
        return false;
    setSelection(Selection(visibleStart, visibleEnd), closeTyping);
    return true;
}

bool SelectionController::isInPasswordField() const
{
    Node* startNode = start().node();
    if (!startNode)
        return false;

    startNode = startNode->shadowAncestorNode();
    if (!startNode)
        return false;

    if (!startNode->hasTagName(inputTag))
        return false;
    
    return static_cast<HTMLInputElement*>(startNode)->inputType() == HTMLInputElement::PASSWORD;
}

bool SelectionController::caretRendersInsideNode(Node* node) const
{
    if (!node)
        return false;
    return !isTableElement(node) && !editingIgnoresContent(node);
}

void SelectionController::focusedOrActiveStateChanged()
{
    bool activeAndFocused = isFocusedAndActive();

    // Simpler on iPhone.
    // Caret blinking (blinks | does not blink)
    if (activeAndFocused)
        m_frame->setSelectionFromNone();
    else
        m_frame->clearSelection();
    m_frame->setCaretVisible(activeAndFocused);

    // Secure keyboard entry is set by the active frame.
    if (m_frame->document()->useSecureKeyboardEntryWhenActive())
        m_frame->setUseSecureKeyboardEntry(activeAndFocused);
}

void SelectionController::pageActivationChanged()
{
    focusedOrActiveStateChanged();
}

void SelectionController::setFocused(bool flag)
{
    if (m_focused == flag)
        return;
    m_focused = flag;

    focusedOrActiveStateChanged();

    if (Document* doc = m_frame->document())
        doc->dispatchWindowEvent(flag ? eventNames().focusEvent : eventNames().blurEvent, false, false);
}

bool SelectionController::isFocusedAndActive() const
{
    return m_focused && m_frame->page() && m_frame->page()->focusController()->isActive();
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
    

static inline bool operator<(const Position& a, const Position& b)
{
    if (a.isNull() || b.isNull())
        return false;
    if (a.container == b.container) {
        return a.posOffset < b.posOffset;
    } 
    else {
        unsigned short result = b.container->compareDocumentPosition(a.container.get());
        return (result == DOCUMENT_POSITION_PRECEDING);
    }
}

static inline bool operator>(const Position& a, const Position& b) 
{
    if (a.isNull() || b.isNull())
        return false;
    return (a != b && b < a);
}

static inline bool operator<(const VisiblePosition& a, const VisiblePosition& b)
{
    return a.deepEquivalent() < b.deepEquivalent();
}

static inline bool operator>(const VisiblePosition& a, const VisiblePosition& b) 
{
    return a.deepEquivalent() > b.deepEquivalent();
}

void SelectionController::expandSelectionToElementContainingCaretSelection()
{
    RefPtr<Range> range = elementRangeContainingCaretSelection();
    if (!range)
        return;
    Selection selection(range.get(), DOWNSTREAM);
    setSelection(selection);
}

PassRefPtr<Range> SelectionController::elementRangeContainingCaretSelection() const
{
    if (m_sel.isNone())
        return NULL;
    
    Selection selection = m_sel;
    if (selection.isNone())
        return NULL;
    
    VisiblePosition visiblePos(selection.start(), VP_DEFAULT_AFFINITY);
    if (visiblePos.isNull())
        return NULL;
    
    Node *node = visiblePos.deepEquivalent().node();
    Element *element = node->enclosingBlockFlowElement();
    if (!element)
        return NULL;
    
    Position startPos(element, 0);
    Position endPos(element, element->childNodeCount());
    
    VisiblePosition startVisiblePos(startPos, VP_DEFAULT_AFFINITY);
    VisiblePosition endVisiblePos(endPos, VP_DEFAULT_AFFINITY);
    if (startVisiblePos.isNull() || endVisiblePos.isNull())
        return NULL;
    
    selection.setBase(startVisiblePos);
    selection.setExtent(endVisiblePos);
    
    return selection.toRange();
}
    
void SelectionController::expandSelectionToWordContainingCaretSelection()
{
    Selection selection(wordSelectionContainingCaretSelection(m_sel));        
    if (selection.isCaretOrRange())
        setSelection(selection);
}

PassRefPtr<Range> SelectionController::wordRangeContainingCaretSelection()
{    
    return wordSelectionContainingCaretSelection(m_sel).toRange();
}

void SelectionController::expandSelectionToStartOfWordContainingCaretSelection()
{
    if (m_sel.isNone() || isStartOfDocument(m_sel.start()))
        return;

    VisiblePosition s1(m_sel.start());
    VisiblePosition e1(m_sel.end());

    Selection expanded(wordSelectionContainingCaretSelection(m_sel));
    VisiblePosition s2(expanded.start());

    // Don't allow the start to become greater after the expansion.
    if (s2.isNull() || s2 > s1)
        s2 = s1;

    moveTo(s2, e1);
}
    
UChar SelectionController::characterInRelationToCaretSelection(int amount) const
{
    if (m_sel.isNone())
        return 0;
    
    Selection selection = m_sel;
    ASSERT(selection.isCaretOrRange());
    
    VisiblePosition visiblePosition(selection.start(), VP_DEFAULT_AFFINITY);
    
    if (amount < 0) {
        int count = abs(amount);
        for (int i = 0; i < count; i++)
            visiblePosition = visiblePosition.previous();    
        return visiblePosition.characterBefore();
    }
    else {
        for (int i = 0; i < amount; i++)
            visiblePosition = visiblePosition.next();    
        return visiblePosition.characterAfter();
    }
}

UChar SelectionController::characterBeforeCaretSelection() const
{
    if (m_sel.isNone())
        return 0;

    Selection selection = m_sel;
    ASSERT(selection.isCaretOrRange());
    
    VisiblePosition visiblePosition(selection.start(), VP_DEFAULT_AFFINITY);
    return visiblePosition.characterBefore();
}

UChar SelectionController::characterAfterCaretSelection() const
{
    if (m_sel.isNone())
        return 0;

    Selection selection = m_sel;
    ASSERT(selection.isCaretOrRange());
    
    VisiblePosition visiblePosition(selection.end(), VP_DEFAULT_AFFINITY);
    return visiblePosition.characterAfter();
}

int SelectionController::wordOffsetInRange(const Range *range) const
{
    if (!range)
        return -1;
    
    Selection selection = m_sel;
    if (!selection.isCaret())
        return -1;

    // FIXME: This will only work in cases where the selection remains in
    // the same node after it is expanded. Improve to handle more complicated
    // cases.
    ExceptionCode ec = 0;
    int result = selection.start().offset() - range->startOffset(ec);
    ASSERT(!ec);
    if (result < 0)
        result = 0;
    return result;
}

bool SelectionController::spaceFollowsWordInRange(const Range *range) const
{
    if (!range)
        return false;
    ExceptionCode ec = 0;
    Node *node = range->endContainer(ec);
    ASSERT(!ec);
    int endOffset = range->endOffset(ec);
    ASSERT(!ec);
    VisiblePosition pos(node, endOffset, VP_DEFAULT_AFFINITY);
    return isSpaceOrNewline(pos.characterAfter());
}

bool SelectionController::selectionAtDocumentStart() const
{
    Selection selection = m_sel;
    if (selection.isNone())
        return false;

    Position startPos(selection.start());
    VisiblePosition pos(startPos.node(), startPos.offset(), VP_DEFAULT_AFFINITY);
    if (pos.isNull())
        return false;

    return isStartOfDocument(pos);
}

bool SelectionController::selectionAtSentenceStart() const
{
    Selection selection = m_sel;
    if (selection.isNone())
        return false;

    return _selectionAtSentenceStart(selection);
}

bool SelectionController::selectionAtWordStart() const
{
    Selection selection = m_sel;
    if (selection.isNone())
        return false;

    Position startPos(selection.start());
    VisiblePosition pos(startPos.node(), startPos.offset(), VP_DEFAULT_AFFINITY);
    if (pos.isNull())
        return false;

    if (isStartOfParagraph(pos))
        return true;
        
    bool result = true;
    unsigned previousCount = 0;
    for (pos = pos.previous(); !pos.isNull(); pos = pos.previous()) {
        previousCount++;
        if (isStartOfParagraph(pos)) {
            if (previousCount == 1)
                result = false;
            break;
        }
        UChar c(pos.characterAfter());
        if (c) {
            result = isSpaceOrNewline(c) || c == 0xA0 || (u_ispunct(c) && c != ',' && c != '-' && c != '\'');
            break;
        }
    }
    
    return result;
}

PassRefPtr<Range> SelectionController::rangeByMovingCurrentSelection(int amount) const
{
    return _rangeByAlteringCurrentSelection(MOVE, amount);
}

PassRefPtr<Range> SelectionController::rangeByExtendingCurrentSelection(int amount) const
{
    return _rangeByAlteringCurrentSelection(EXTEND, amount);
}

void SelectionController::selectRangeOnElement(unsigned int location, unsigned int length, Node* node)
{
    RefPtr<Range> resultRange = m_frame->document()->createRange();
    ExceptionCode ec = 0;
    resultRange->setStart(node, location, ec);
    ASSERT(!ec);
    resultRange->setEnd(node, location + length, ec);
    ASSERT(!ec);
    Selection selection = Selection(resultRange.get(), SEL_DEFAULT_AFFINITY);
    setSelection(selection, true);
}

Selection SelectionController::wordSelectionContainingCaretSelection(const Selection& selection)
{
    if (selection.isNone())
        return Selection();
    
    ASSERT(selection.isCaretOrRange());
    SelectionController controller;
    controller.setSelection(selection);
    
    Position startPosBeforeExpansion(selection.start());
    Position endPosBeforeExpansion(selection.end());
    VisiblePosition startVisiblePosBeforeExpansion(startPosBeforeExpansion, VP_DEFAULT_AFFINITY);
    VisiblePosition endVisiblePosBeforeExpansion(endPosBeforeExpansion, VP_DEFAULT_AFFINITY);
    if (endVisiblePosBeforeExpansion.isNull())
        return Selection();
    
    if (isEndOfParagraph(endVisiblePosBeforeExpansion)) {
        UChar c(endVisiblePosBeforeExpansion.characterBefore());
        if (isSpaceOrNewline(c) || c == 0xA0) {
            //fprintf(stderr, "end of paragraph with space\n");
            return Selection();
        }
    }
    
    // If at end of paragraph, move backwards one character.
    // This has the effect of selecting the word on the line (which is
    // what we want, rather than selecting past the end of the line).
    if (isEndOfParagraph(endVisiblePosBeforeExpansion) &&
        !isStartOfParagraph(endVisiblePosBeforeExpansion)) {
        controller.modify(SelectionController::MOVE, SelectionController::BACKWARD, CharacterGranularity);
    }
    
    controller.expandUsingGranularity(WordGranularity);
    
    Position startPos(controller.selection().start());
    Position endPos(controller.selection().end());
    
    // Expansion cannot be allowed to change selection so that it is no longer
    // touches (or contains) the original, unexpanded selection.
    // Enforce this on the way into these additional calculations to give them
    // the best chance to yield a suitable answer.
    if (startPos > startPosBeforeExpansion) {
        startPos = startPosBeforeExpansion;
    }
    if (endPos < endPosBeforeExpansion) {
        endPos = endPosBeforeExpansion;
    }
    
    VisiblePosition startVisiblePos(startPos, VP_DEFAULT_AFFINITY);
    VisiblePosition endVisiblePos(endPos, VP_DEFAULT_AFFINITY);
    
    if (startVisiblePos.isNull() || endVisiblePos.isNull()) {
        //fprintf(stderr, "start or end is nil [1]\n");
        return Selection();
    }
    
    if (isEndOfLine(endVisiblePosBeforeExpansion)) {
        VisiblePosition previous(endVisiblePos.previous());
        if (previous == endVisiblePos) {
            // empty document
            //fprintf(stderr, "empty document\n");
            return Selection();
        }
        UChar c(previous.characterAfter());
        if (isSpaceOrNewline(c) || c == 0xA0) {
            // space at end of line
            //fprintf(stderr, "space at end of line [1]\n");
            return Selection();
        }
    }
    
    // Expansion has selected past end of line.
    // Try repositioning backwards.
    if (isEndOfLine(startVisiblePos) && isStartOfLine(endVisiblePos)) {
        VisiblePosition previous(startVisiblePos.previous());
        if (isEndOfLine(previous)) {
            // on empty line
            //fprintf(stderr, "on empty line\n");
            return Selection();
        }
        UChar c(previous.characterAfter());
        if (isSpaceOrNewline(c) || c == 0xA0) {
            // space at end of line
            //fprintf(stderr, "space at end of line [2]\n");
            return Selection();
        }
        controller.moveTo(startVisiblePos);
        controller.modify(SelectionController::EXTEND, SelectionController::BACKWARD, WordGranularity);
        startPos = controller.selection().start();
        endPos = controller.selection().end();
        startVisiblePos = VisiblePosition(startPos, VP_DEFAULT_AFFINITY);
        endVisiblePos = VisiblePosition(endPos, VP_DEFAULT_AFFINITY);
        if (startVisiblePos.isNull() || endVisiblePos.isNull()) {
            //fprintf(stderr, "start or end is nil [2]\n");
            return Selection();
        }
    }
    
    // Now loop backwards until we find a non-space.
    while (endVisiblePos != startVisiblePos) {
        VisiblePosition previous(endVisiblePos.previous());
        UChar c(previous.characterAfter());
        if (!isSpaceOrNewline(c) && c != 0xA0)
            break;
        endVisiblePos = previous;
    }

    // Expansion cannot be allowed to change selection so that it is no longer
    // touches (or contains) the original, unexpanded selection.
    // Enforce this on the way out of the function to preserve the invariant.
    if (startVisiblePos > startVisiblePosBeforeExpansion) {
        startVisiblePos = startVisiblePosBeforeExpansion;
    }
    if (endVisiblePos < endVisiblePosBeforeExpansion) {
        endVisiblePos = endVisiblePosBeforeExpansion;
    }

    return Selection(startVisiblePos, endVisiblePos);    
}

bool SelectionController::_selectionAtSentenceStart(const Selection& sel) const
{
    Position startPos(sel.start());
    VisiblePosition pos(startPos.node(), startPos.offset(), VP_DEFAULT_AFFINITY);
    if (pos.isNull())
        return false;

    if (isStartOfParagraph(pos))
        return true;
        
    bool result = true;
    bool sawSpace = false;
    unsigned previousCount = 0;
    for (pos = pos.previous(); !pos.isNull(); pos = pos.previous()) {
        previousCount++;
        if (isStartOfParagraph(pos)) {
            if (previousCount == 1 || (previousCount == 2 && sawSpace))
                result = false;
            break;
        }
        UChar c(pos.characterAfter());
        if (c) {
            if (isSpaceOrNewline(c) || c == 0xA0) {
                sawSpace = true;
            }
            else {
                result = (c == '.' || c == '!' || c == '?');
                break;
            }
        }
    }
    
    return result;
}

PassRefPtr<Range> SelectionController::_rangeByAlteringCurrentSelection(EAlteration alteration, int amount) const
{
    if (m_sel.isNone())
        return NULL;
        
    if (amount == 0)
        return toRange();

    SelectionController selectionController;
    selectionController.setSelection(m_sel);
    SelectionController::EDirection direction = amount > 0 ? SelectionController::FORWARD : SelectionController::BACKWARD;
    for (int i = 0; i < abs(amount); i++)
        selectionController.modify(alteration, direction, CharacterGranularity);
    return selectionController.toRange();
}

bool SelectionController::selectionIsCaretInDisplayBlockElementAtOffset(int offset) const
{
    if (!m_sel.isCaret())
        return false;

    Position pos(m_sel.start());
    RenderObject *renderer = pos.container->renderer();
    if (!renderer)
        return false;

    return renderer->style()->display() == BLOCK && pos.posOffset == offset;
}

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
