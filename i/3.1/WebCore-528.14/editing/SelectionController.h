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

#ifndef SelectionController_h
#define SelectionController_h

#include "IntRect.h"
#include "Selection.h"
#include "Range.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class Frame;
class GraphicsContext;
class RenderObject;
class VisiblePosition;

class SelectionController : Noncopyable {
public:
    enum EAlteration { MOVE, EXTEND };
    enum EDirection { FORWARD, BACKWARD, RIGHT, LEFT };

    SelectionController(Frame* = 0, bool isDragCaretController = false);

    Element* rootEditableElement() const { return m_sel.rootEditableElement(); }
    bool isContentEditable() const { return m_sel.isContentEditable(); }
    bool isContentRichlyEditable() const { return m_sel.isContentRichlyEditable(); }
    Node* shadowTreeRootNode() const { return m_sel.shadowTreeRootNode(); }
     
    void moveTo(const Range*, EAffinity, bool userTriggered = false);
    void moveTo(const VisiblePosition&, bool userTriggered = false);
    void moveTo(const VisiblePosition&, const VisiblePosition&, bool userTriggered = false);
    void moveTo(const Position&, EAffinity, bool userTriggered = false);
    void moveTo(const Position&, const Position&, EAffinity, bool userTriggered = false);

    const Selection& selection() const { return m_sel; }
    void setSelection(const Selection&, bool closeTyping = true, bool clearTypingStyle = true, bool userTriggered = false);
    bool setSelectedRange(Range*, EAffinity, bool closeTyping);
    void selectAll();
    void clear();
    
    // Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
    void selectFrameElementInParentIfFullySelected();

    bool contains(const IntPoint&);

    Selection::EState state() const { return m_sel.state(); }

    EAffinity affinity() const { return m_sel.affinity(); }

    bool modify(EAlteration, EDirection, TextGranularity, bool userTriggered = false);
    bool modify(EAlteration, int verticalDistance, bool userTriggered = false);
    bool expandUsingGranularity(TextGranularity);

    void setBase(const VisiblePosition&, bool userTriggered = false);
    void setBase(const Position&, EAffinity, bool userTriggered = false);
    void setExtent(const VisiblePosition&, bool userTriggered = false);
    void setExtent(const Position&, EAffinity, bool userTriggered = false);

    Position base() const { return m_sel.base(); }
    Position extent() const { return m_sel.extent(); }
    Position start() const { return m_sel.start(); }
    Position end() const { return m_sel.end(); }

    // Return the renderer that is responsible for painting the caret (in the selection start node)
    RenderObject* caretRenderer() const;

    // Caret rect local to the caret's renderer
    IntRect localCaretRect() const;
    // Bounds of (possibly transformed) caret in absolute coords
    IntRect absoluteCaretBounds();
    void setNeedsLayout(bool flag = true);

    void setLastChangeWasHorizontalExtension(bool b) { m_lastChangeWasHorizontalExtension = b; }
    void willBeModified(EAlteration, EDirection);
    
    bool isNone() const { return m_sel.isNone(); }
    bool isCaret() const { return m_sel.isCaret(); }
    bool isRange() const { return m_sel.isRange(); }
    bool isCaretOrRange() const { return m_sel.isCaretOrRange(); }
    bool isInPasswordField() const;
    
    PassRefPtr<Range> toRange() const { return m_sel.toRange(); }

    void debugRenderer(RenderObject*, bool selected) const;
    
    void nodeWillBeRemoved(Node*);

    bool recomputeCaretRect(); // returns true if caret rect moved
    void invalidateCaretRect();
    void paintCaret(GraphicsContext*, int tx, int ty, const IntRect& clipRect);

    // Used to suspend caret blinking while the mouse is down.
    void setCaretBlinkingSuspended(bool suspended) { m_isCaretBlinkingSuspended = suspended; }
    bool isCaretBlinkingSuspended() const { return m_isCaretBlinkingSuspended; }

    // Focus
    void setFocused(bool);
    bool isFocusedAndActive() const;
    void pageActivationChanged();

#ifndef NDEBUG
    void formatForDebugger(char* buffer, unsigned length) const;
    void showTreeForThis() const;
#endif
    
public:
    void expandSelectionToElementContainingCaretSelection();
    PassRefPtr<Range> elementRangeContainingCaretSelection() const;
    void expandSelectionToWordContainingCaretSelection();
    PassRefPtr<Range> wordRangeContainingCaretSelection();
    void expandSelectionToStartOfWordContainingCaretSelection();
    UChar characterInRelationToCaretSelection(int amount) const;
    UChar characterBeforeCaretSelection() const;
    UChar characterAfterCaretSelection() const;
    int wordOffsetInRange(const Range *range) const;
    bool spaceFollowsWordInRange(const Range * range) const;
    bool selectionAtDocumentStart() const;
    bool selectionAtSentenceStart() const;
    bool selectionAtWordStart() const;
    PassRefPtr<Range> rangeByMovingCurrentSelection(int amount) const;
    PassRefPtr<Range> rangeByExtendingCurrentSelection(int amount) const;
    void selectRangeOnElement(unsigned int location, unsigned int length, Node* node);
    bool selectionIsCaretInDisplayBlockElementAtOffset(int offset) const;
    void suppressCloseTyping() { ++m_closeTypingSuppressions; }
    void restoreCloseTyping() { --m_closeTypingSuppressions; }
private:
    static Selection wordSelectionContainingCaretSelection(const Selection&);
    bool _selectionAtSentenceStart(const Selection& sel) const;
    PassRefPtr<Range> _rangeByAlteringCurrentSelection(EAlteration alteration, int amount) const;

private:
    enum EPositionType { START, END, BASE, EXTENT };

    VisiblePosition modifyExtendingRightForward(TextGranularity);
    VisiblePosition modifyMovingRight(TextGranularity);
    VisiblePosition modifyMovingForward(TextGranularity);
    VisiblePosition modifyExtendingLeftBackward(TextGranularity);
    VisiblePosition modifyMovingLeft(TextGranularity);
    VisiblePosition modifyMovingBackward(TextGranularity);

    void layout();
    IntRect caretRepaintRect() const;

    int xPosForVerticalArrowNavigation(EPositionType);
    
#if PLATFORM(MAC)
    void notifyAccessibilityForSelectionChange();
#else
    void notifyAccessibilityForSelectionChange() {};
#endif

    void focusedOrActiveStateChanged();
    bool caretRendersInsideNode(Node*) const;
    
    IntRect absoluteBoundsForLocalRect(const IntRect&) const;

    Frame* m_frame;
    int m_xPosForVerticalArrowNavigation;

    Selection m_sel;

    IntRect m_caretRect;        // caret rect in coords local to the renderer responsible for painting the caret
    IntRect m_absCaretBounds;   // absolute bounding rect for the caret
    IntRect m_absoluteCaretRepaintBounds;
    
    bool m_needsLayout : 1;       // true if the caret and expectedVisible rectangles need to be calculated
    bool m_absCaretBoundsDirty: 1;
    bool m_lastChangeWasHorizontalExtension : 1;
    bool m_isDragCaretController : 1;
    bool m_isCaretBlinkingSuspended : 1;
    bool m_focused : 1;

    int m_closeTypingSuppressions;
};

inline bool operator==(const SelectionController& a, const SelectionController& b)
{
    return a.start() == b.start() && a.end() == b.end() && a.affinity() == b.affinity();
}

inline bool operator!=(const SelectionController& a, const SelectionController& b)
{
    return !(a == b);
}

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::SelectionController&);
void showTree(const WebCore::SelectionController*);
#endif

#endif // SelectionController_h
