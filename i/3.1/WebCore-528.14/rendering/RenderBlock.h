/*
 * This file is part of the render object implementation for KHTML.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 */

#ifndef RenderBlock_h
#define RenderBlock_h

#include "DeprecatedPtrList.h"
#include "GapRects.h"
#include "RenderFlow.h"
#include "RootInlineBox.h"
#include <wtf/ListHashSet.h>

namespace WebCore {

class InlineIterator;
class BidiRun;
class Position;
class RootInlineBox;

template <class Iterator, class Run> class BidiResolver;
typedef BidiResolver<InlineIterator, BidiRun> InlineBidiResolver;

enum CaretType { CursorCaret, DragCaret };

class RenderBlock : public RenderFlow {
public:
    RenderBlock(Node*);
    virtual ~RenderBlock();

    virtual const char* renderName() const;

    // These two functions are overridden for inline-block.
    virtual int lineHeight(bool firstLine, bool isRootLineBox = false) const;
    virtual int baselinePosition(bool firstLine, bool isRootLineBox = false) const;

    virtual bool isRenderBlock() const { return true; }
    virtual bool isBlockFlow() const { return (!isInline() || isReplaced()) && !isTable(); }
    virtual bool isInlineBlockOrInlineTable() const { return isInline() && isReplaced(); }

    virtual bool childrenInline() const { return m_childrenInline; }
    virtual void setChildrenInline(bool b) { m_childrenInline = b; }
    void makeChildrenNonInline(RenderObject* insertionPoint = 0);
    void deleteLineBoxTree();

    // The height (and width) of a block when you include overflow spillage out of the bottom
    // of the block (e.g., a <div style="height:25px"> that has a 100px tall image inside
    // it would have an overflow height of borderTop() + paddingTop() + 100px.
    virtual int overflowHeight(bool includeInterior = true) const;
    virtual int overflowWidth(bool includeInterior = true) const;
    virtual int overflowLeft(bool includeInterior = true) const;
    virtual int overflowTop(bool includeInterior = true) const;
    virtual IntRect overflowRect(bool includeInterior = true) const;
    virtual void setOverflowHeight(int h) { m_overflowHeight = h; }
    virtual void setOverflowWidth(int w) { m_overflowWidth = w; }

    void addVisualOverflow(const IntRect&);

    virtual bool isSelfCollapsingBlock() const;
    virtual bool isTopMarginQuirk() const { return m_topMarginQuirk; }
    virtual bool isBottomMarginQuirk() const { return m_bottomMarginQuirk; }

    virtual int maxTopMargin(bool positive) const { return positive ? maxTopPosMargin() : maxTopNegMargin(); }
    virtual int maxBottomMargin(bool positive) const { return positive ? maxBottomPosMargin() : maxBottomNegMargin(); }

    int maxTopPosMargin() const { return m_maxMargin ? m_maxMargin->m_topPos : MaxMargin::topPosDefault(this); }
    int maxTopNegMargin() const { return m_maxMargin ? m_maxMargin->m_topNeg : MaxMargin::topNegDefault(this); }
    int maxBottomPosMargin() const { return m_maxMargin ? m_maxMargin->m_bottomPos : MaxMargin::bottomPosDefault(this); }
    int maxBottomNegMargin() const { return m_maxMargin ? m_maxMargin->m_bottomNeg : MaxMargin::bottomNegDefault(this); }
    void setMaxTopMargins(int pos, int neg);
    void setMaxBottomMargins(int pos, int neg);
    
    void initMaxMarginValues()
    {
        if (m_maxMargin) {
            m_maxMargin->m_topPos = MaxMargin::topPosDefault(this);
            m_maxMargin->m_topNeg = MaxMargin::topNegDefault(this);
            m_maxMargin->m_bottomPos = MaxMargin::bottomPosDefault(this);
            m_maxMargin->m_bottomNeg = MaxMargin::bottomNegDefault(this);
        }
    }

    virtual void addChildToFlow(RenderObject* newChild, RenderObject* beforeChild);
    virtual void removeChild(RenderObject*);

    virtual void repaintOverhangingFloats(bool paintAllDescendants);

    virtual void layout();
    virtual void layoutBlock(bool relayoutChildren);
    void layoutBlockChildren(bool relayoutChildren, int& maxFloatBottom);
    void layoutInlineChildren(bool relayoutChildren, int& repaintTop, int& repaintBottom);

    void layoutPositionedObjects(bool relayoutChildren);
    void insertPositionedObject(RenderBox*);
    void removePositionedObject(RenderBox*);
    void removePositionedObjects(RenderBlock*);

    void addPercentHeightDescendant(RenderBox*);
    static void removePercentHeightDescendant(RenderBox*);

    virtual void positionListMarker() { }

    virtual void borderFitAdjust(int& x, int& w) const; // Shrink the box in which the border paints if border-fit is set.

    // Called to lay out the legend for a fieldset.
    virtual RenderObject* layoutLegend(bool /*relayoutChildren*/) { return 0; }

    // the implementation of the following functions is in bidi.cpp
    struct FloatWithRect {
        FloatWithRect(RenderBox* f)
            : object(f)
            , rect(IntRect(f->x() - f->marginLeft(), f->y() - f->marginTop(), f->width() + f->marginLeft() + f->marginRight(), f->height() + f->marginTop() + f->marginBottom()))
        {
        }

        RenderBox* object;
        IntRect rect;
    };

    void bidiReorderLine(InlineBidiResolver&, const InlineIterator& end);
    RootInlineBox* determineStartPosition(bool& fullLayout, InlineBidiResolver&, Vector<FloatWithRect>& floats, unsigned& numCleanFloats);
    RootInlineBox* determineEndPosition(RootInlineBox* startBox, InlineIterator& cleanLineStart,
                                        BidiStatus& cleanLineBidiStatus,
                                        int& yPos);
    bool matchedEndLine(const InlineBidiResolver&, const InlineIterator& endLineStart, const BidiStatus& endLineStatus,
                        RootInlineBox*& endLine, int& endYPos, int& repaintBottom, int& repaintTop);
    bool generatesLineBoxesForInlineChild(RenderObject*);
    void skipTrailingWhitespace(InlineIterator&);
    int skipLeadingWhitespace(InlineBidiResolver&);
    void fitBelowFloats(int widthToFit, int& availableWidth);
    InlineIterator findNextLineBreak(InlineBidiResolver&, EClear* clear = 0);
    RootInlineBox* constructLine(unsigned runCount, BidiRun* firstRun, BidiRun* lastRun, bool lastLine, RenderObject* endObject);
    InlineFlowBox* createLineBoxes(RenderObject*);
    void computeHorizontalPositionsForLine(RootInlineBox*, BidiRun* firstRun, BidiRun* trailingSpaceRun, bool reachedEnd);
    void computeVerticalPositionsForLine(RootInlineBox*, BidiRun*);
    void checkLinesForOverflow();
    void deleteEllipsisLineBoxes();
    void checkLinesForTextOverflow();
    // end bidi.cpp functions

    virtual void paint(PaintInfo&, int tx, int ty);
    virtual void paintObject(PaintInfo&, int tx, int ty);
    void paintFloats(PaintInfo&, int tx, int ty, bool preservePhase = false);
    void paintContents(PaintInfo&, int tx, int ty);
    void paintColumns(PaintInfo&, int tx, int ty, bool paintFloats = false);
    void paintChildren(PaintInfo&, int tx, int ty);
    void paintEllipsisBoxes(PaintInfo&, int tx, int ty);
    void paintSelection(PaintInfo&, int tx, int ty);
    void paintCaret(PaintInfo&, int tx, int ty, CaretType);

    void insertFloatingObject(RenderBox*);
    void removeFloatingObject(RenderBox*);

    // Called from lineWidth, to position the floats added in the last line.
    // Returns ture if and only if it has positioned any floats.
    bool positionNewFloats();
    void clearFloats();
    int getClearDelta(RenderBox* child);
    void markAllDescendantsWithFloatsForLayout(RenderBox* floatToRemove = 0, bool inLayout = true);
    void markPositionedObjectsForLayout();

    virtual bool containsFloats() { return m_floatingObjects && !m_floatingObjects->isEmpty(); }
    virtual bool containsFloat(RenderObject*);

    virtual bool avoidsFloats() const;

    virtual bool hasOverhangingFloats() { return !hasColumns() && floatBottom() > height(); }
    void addIntrudingFloats(RenderBlock* prev, int xoffset, int yoffset);
    int addOverhangingFloats(RenderBlock* child, int xoffset, int yoffset, bool makeChildPaintOtherFloats);

    int nextFloatBottomBelow(int) const;
    int floatBottom() const;
    inline int leftBottom();
    inline int rightBottom();
    IntRect floatRect() const;

    virtual int lineWidth(int) const;
    virtual int lowestPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;
    virtual int rightmostPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;
    virtual int leftmostPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;

    int rightOffset() const;
    int rightRelOffset(int y, int fixedOffset, bool applyTextIndent = true, int* heightRemaining = 0) const;
    int rightOffset(int y) const { return rightRelOffset(y, rightOffset(), true); }

    int leftOffset() const;
    int leftRelOffset(int y, int fixedOffset, bool applyTextIndent = true, int* heightRemaining = 0) const;
    int leftOffset(int y) const { return leftRelOffset(y, leftOffset(), true); }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    virtual bool hitTestColumns(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    virtual bool hitTestContents(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    virtual bool isPointInOverflowControl(HitTestResult&, int x, int y, int tx, int ty);

    virtual VisiblePosition positionForCoordinates(int x, int y);
    
    // Block flows subclass availableWidth to handle multi column layout (shrinking the width available to children when laying out.)
    virtual int availableWidth() const;
    
    virtual void calcPrefWidths();
    void calcInlinePrefWidths();
    void calcBlockPrefWidths();

    virtual int getBaselineOfFirstLineBox() const;
    virtual int getBaselineOfLastLineBox() const;

    RootInlineBox* firstRootBox() const { return static_cast<RootInlineBox*>(firstLineBox()); }
    RootInlineBox* lastRootBox() const { return static_cast<RootInlineBox*>(lastLineBox()); }

    bool containsNonZeroBidiLevel() const;

    // Obtains the nearest enclosing block (including this block) that contributes a first-line style to our inline
    // children.
    virtual RenderBlock* firstLineBlock() const;
    virtual void updateFirstLetter();

    bool inRootBlockContext() const;

    void setHasMarkupTruncation(bool b = true) { m_hasMarkupTruncation = b; }
    bool hasMarkupTruncation() const { return m_hasMarkupTruncation; }

    virtual bool hasSelectedChildren() const { return m_selectionState != SelectionNone; }
    virtual SelectionState selectionState() const { return static_cast<SelectionState>(m_selectionState); }
    virtual void setSelectionState(SelectionState s);

    virtual IntRect selectionRectForRepaint(RenderBox* repaintContainer, bool /*clipToVisibleContent*/)
    {
        return selectionGapRectsForRepaint(repaintContainer);
    }
    GapRects selectionGapRectsForRepaint(RenderBox* repaintContainer);
    virtual bool shouldPaintSelectionGaps() const;
    bool isSelectionRoot() const;
    GapRects fillSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty,
                               int& lastTop, int& lastLeft, int& lastRight, const PaintInfo* = 0);
    GapRects fillInlineSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty,
                                     int& lastTop, int& lastLeft, int& lastRight, const PaintInfo*);
    GapRects fillBlockSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty,
                                    int& lastTop, int& lastLeft, int& lastRight, const PaintInfo*);
    IntRect fillVerticalSelectionGap(int lastTop, int lastLeft, int lastRight, int bottomY, RenderBlock* rootBlock,
                                     int blockX, int blockY, const PaintInfo*);
    IntRect fillLeftSelectionGap(RenderObject* selObj, int xPos, int yPos, int height, RenderBlock* rootBlock, 
                                 int blockX, int blockY, int tx, int ty, const PaintInfo*);
    IntRect fillRightSelectionGap(RenderObject* selObj, int xPos, int yPos, int height, RenderBlock* rootBlock,
                                  int blockX, int blockY, int tx, int ty, const PaintInfo*);
    IntRect fillHorizontalSelectionGap(RenderObject* selObj, int xPos, int yPos, int width, int height, const PaintInfo*);

    void getHorizontalSelectionGapInfo(SelectionState, bool& leftGap, bool& rightGap);
    int leftSelectionOffset(RenderBlock* rootBlock, int y);
    int rightSelectionOffset(RenderBlock* rootBlock, int y);

    // Helper methods for computing line counts and heights for line counts.
    RootInlineBox* lineAtIndex(int);
    int lineCount();
    int heightForLineCount(int);
    void clearTruncation();

    int immediateLineCount();
    void adjustComputedFontSizes(float size, float visibleWidth);
    void resetComputedFontSize() { 
        m_widthForTextAutosizing = -1; 
        m_lineCountForTextAutosizing = NOT_SET;
    }

    int desiredColumnWidth() const;
    unsigned desiredColumnCount() const;
    Vector<IntRect>* columnRects() const;
    void setDesiredColumnCountAndWidth(int count, int width);
    
    void adjustRectForColumns(IntRect&) const;

    void addContinuationWithOutline(RenderFlow*);
    void paintContinuationOutlines(PaintInfo&, int tx, int ty);

private:
    void adjustPointToColumnContents(IntPoint&) const;
    void adjustForBorderFit(int x, int& left, int& right) const; // Helper function for borderFitAdjust

    void markLinesDirtyInVerticalRange(int top, int bottom);

protected:
    virtual void styleWillChange(StyleDifference, const RenderStyle* newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    void newLine(EClear);
    virtual bool hasLineIfEmpty() const;
    bool layoutOnlyPositionedObjects();

private:
    Position positionForBox(InlineBox*, bool start = true) const;
    Position positionForRenderer(RenderObject*, bool start = true) const;

    // Adjust tx and ty from painting offsets to the local coords of this renderer
    void offsetForContents(int& tx, int& ty) const;

    int columnGap() const;
    void calcColumnWidth();
    int layoutColumns(int endOfContent = -1);

    bool expandsToEncloseOverhangingFloats() const;

protected:
    struct FloatingObject {
        enum Type {
            FloatLeft,
            FloatRight
        };

        FloatingObject(Type type)
            : m_renderer(0)
            , m_top(0)
            , m_bottom(0)
            , m_left(0)
            , m_width(0)
            , m_type(type)
            , m_shouldPaint(true)
            , m_isDescendant(false)
        {
        }

        Type type() { return static_cast<Type>(m_type); }

        RenderBox* m_renderer;
        int m_top;
        int m_bottom;
        int m_left;
        int m_width;
        unsigned m_type : 1; // Type (left or right aligned)
        bool m_shouldPaint : 1;
        bool m_isDescendant : 1;
    };

    class MarginInfo {
        // Collapsing flags for whether we can collapse our margins with our children's margins.
        bool m_canCollapseWithChildren : 1;
        bool m_canCollapseTopWithChildren : 1;
        bool m_canCollapseBottomWithChildren : 1;

        // Whether or not we are a quirky container, i.e., do we collapse away top and bottom
        // margins in our container.  Table cells and the body are the common examples. We
        // also have a custom style property for Safari RSS to deal with TypePad blog articles.
        bool m_quirkContainer : 1;

        // This flag tracks whether we are still looking at child margins that can all collapse together at the beginning of a block.  
        // They may or may not collapse with the top margin of the block (|m_canCollapseTopWithChildren| tells us that), but they will
        // always be collapsing with one another.  This variable can remain set to true through multiple iterations 
        // as long as we keep encountering self-collapsing blocks.
        bool m_atTopOfBlock : 1;

        // This flag is set when we know we're examining bottom margins and we know we're at the bottom of the block.
        bool m_atBottomOfBlock : 1;

        // If our last normal flow child was a self-collapsing block that cleared a float,
        // we track it in this variable.
        bool m_selfCollapsingBlockClearedFloat : 1;

        // These variables are used to detect quirky margins that we need to collapse away (in table cells
        // and in the body element).
        bool m_topQuirk : 1;
        bool m_bottomQuirk : 1;
        bool m_determinedTopQuirk : 1;

        // These flags track the previous maximal positive and negative margins.
        int m_posMargin;
        int m_negMargin;

    public:
        MarginInfo(RenderBlock* b, int top, int bottom);

        void setAtTopOfBlock(bool b) { m_atTopOfBlock = b; }
        void setAtBottomOfBlock(bool b) { m_atBottomOfBlock = b; }
        void clearMargin() { m_posMargin = m_negMargin = 0; }
        void setSelfCollapsingBlockClearedFloat(bool b) { m_selfCollapsingBlockClearedFloat = b; }
        void setTopQuirk(bool b) { m_topQuirk = b; }
        void setBottomQuirk(bool b) { m_bottomQuirk = b; }
        void setDeterminedTopQuirk(bool b) { m_determinedTopQuirk = b; }
        void setPosMargin(int p) { m_posMargin = p; }
        void setNegMargin(int n) { m_negMargin = n; }
        void setPosMarginIfLarger(int p) { if (p > m_posMargin) m_posMargin = p; }
        void setNegMarginIfLarger(int n) { if (n > m_negMargin) m_negMargin = n; }

        void setMargin(int p, int n) { m_posMargin = p; m_negMargin = n; }

        bool atTopOfBlock() const { return m_atTopOfBlock; }
        bool canCollapseWithTop() const { return m_atTopOfBlock && m_canCollapseTopWithChildren; }
        bool canCollapseWithBottom() const { return m_atBottomOfBlock && m_canCollapseBottomWithChildren; }
        bool canCollapseTopWithChildren() const { return m_canCollapseTopWithChildren; }
        bool canCollapseBottomWithChildren() const { return m_canCollapseBottomWithChildren; }
        bool selfCollapsingBlockClearedFloat() const { return m_selfCollapsingBlockClearedFloat; }
        bool quirkContainer() const { return m_quirkContainer; }
        bool determinedTopQuirk() const { return m_determinedTopQuirk; }
        bool topQuirk() const { return m_topQuirk; }
        bool bottomQuirk() const { return m_bottomQuirk; }
        int posMargin() const { return m_posMargin; }
        int negMargin() const { return m_negMargin; }
        int margin() const { return m_posMargin - m_negMargin; }
    };

    void adjustPositionedBlock(RenderBox* child, const MarginInfo&);
    void adjustFloatingBlock(const MarginInfo&);
    RenderBox* handleSpecialChild(RenderBox* child, const MarginInfo&, bool& handled);
    RenderBox* handleFloatingChild(RenderBox* child, const MarginInfo&, bool& handled);
    RenderBox* handlePositionedChild(RenderBox* child, const MarginInfo&, bool& handled);
    RenderBox* handleRunInChild(RenderBox* child, bool& handled);
    void collapseMargins(RenderBox* child, MarginInfo&, int yPosEstimate);
    void clearFloatsIfNeeded(RenderBox* child, MarginInfo&, int oldTopPosMargin, int oldTopNegMargin);
    int estimateVerticalPosition(RenderBox* child, const MarginInfo&);
    void determineHorizontalPosition(RenderBox* child);
    void handleBottomOfBlock(int top, int bottom, MarginInfo&);
    void setCollapsedBottomMargin(const MarginInfo&);
    // End helper functions and structs used by layoutBlockChildren.

private:
    typedef ListHashSet<RenderBox*>::const_iterator Iterator;
    DeprecatedPtrList<FloatingObject>* m_floatingObjects;
    ListHashSet<RenderBox*>* m_positionedObjects;
         
    // Allocated only when some of these fields have non-default values
    struct MaxMargin {
        MaxMargin(const RenderBlock* o) 
            : m_topPos(topPosDefault(o))
            , m_topNeg(topNegDefault(o))
            , m_bottomPos(bottomPosDefault(o))
            , m_bottomNeg(bottomNegDefault(o))
        { 
        }

        static int topPosDefault(const RenderBlock* o) { return o->marginTop() > 0 ? o->marginTop() : 0; }
        static int topNegDefault(const RenderBlock* o) { return o->marginTop() < 0 ? -o->marginTop() : 0; }
        static int bottomPosDefault(const RenderBlock* o) { return o->marginBottom() > 0 ? o->marginBottom() : 0; }
        static int bottomNegDefault(const RenderBlock* o) { return o->marginBottom() < 0 ? -o->marginBottom() : 0; }

        int m_topPos;
        int m_topNeg;
        int m_bottomPos;
        int m_bottomNeg;
     };

    MaxMargin* m_maxMargin;

protected:
    // How much content overflows out of our block vertically or horizontally.
    int m_overflowHeight;
    int m_overflowWidth;
    int m_overflowLeft;
    int m_overflowTop;

    int m_widthForTextAutosizing;
};

} // namespace WebCore

#endif // RenderBlock_h
