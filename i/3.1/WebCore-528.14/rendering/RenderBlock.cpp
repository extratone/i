/*
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

#include "config.h"
#include "RenderBlock.h"

#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "InlineTextBox.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderMarquee.h"
#include "RenderReplica.h"
#include "RenderTableCell.h"
#include "RenderTextFragment.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SelectionController.h"
#include <wtf/StdLibExtras.h>

using namespace std;
using namespace WTF;
using namespace Unicode;

namespace WebCore {

// Number of pixels to allow as a fudge factor when clicking above or below a line.
// clicking up to verticalLineClickFudgeFactor pixels above a line will correspond to the closest point on the line.   
const int verticalLineClickFudgeFactor= 3;

using namespace HTMLNames;

struct ColumnInfo {
    ColumnInfo()
        : m_desiredColumnWidth(0)
        , m_desiredColumnCount(1)
        { }
    int m_desiredColumnWidth;
    unsigned m_desiredColumnCount;
    Vector<IntRect> m_columnRects;
};

typedef WTF::HashMap<const RenderBox*, ColumnInfo*> ColumnInfoMap;
static ColumnInfoMap* gColumnInfoMap = 0;

typedef WTF::HashMap<const RenderBlock*, HashSet<RenderBox*>*> PercentHeightDescendantsMap;
static PercentHeightDescendantsMap* gPercentHeightDescendantsMap = 0;

typedef WTF::HashMap<const RenderBox*, HashSet<RenderBlock*>*> PercentHeightContainerMap;
static PercentHeightContainerMap* gPercentHeightContainerMap = 0;
    
typedef WTF::HashMap<RenderBlock*, ListHashSet<RenderFlow*>*> ContinuationOutlineTableMap;

// Our MarginInfo state used when laying out block children.
RenderBlock::MarginInfo::MarginInfo(RenderBlock* block, int top, int bottom)
{
    // Whether or not we can collapse our own margins with our children.  We don't do this
    // if we had any border/padding (obviously), if we're the root or HTML elements, or if
    // we're positioned, floating, a table cell.
    m_canCollapseWithChildren = !block->isRenderView() && !block->isRoot() && !block->isPositioned() &&
        !block->isFloating() && !block->isTableCell() && !block->hasOverflowClip() && !block->isInlineBlockOrInlineTable();

    m_canCollapseTopWithChildren = m_canCollapseWithChildren && (top == 0) && block->style()->marginTopCollapse() != MSEPARATE;

    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins.  To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it.  We also don't collapse if we have any bottom border/padding.
    m_canCollapseBottomWithChildren = m_canCollapseWithChildren && (bottom == 0) &&
        (block->style()->height().isAuto() && block->style()->height().value() == 0) && block->style()->marginBottomCollapse() != MSEPARATE;
    
    m_quirkContainer = block->isTableCell() || block->isBody() || block->style()->marginTopCollapse() == MDISCARD || 
        block->style()->marginBottomCollapse() == MDISCARD;

    m_atTopOfBlock = true;
    m_atBottomOfBlock = false;

    m_posMargin = m_canCollapseTopWithChildren ? block->maxTopMargin(true) : 0;
    m_negMargin = m_canCollapseTopWithChildren ? block->maxTopMargin(false) : 0;

    m_selfCollapsingBlockClearedFloat = false;
    
    m_topQuirk = m_bottomQuirk = m_determinedTopQuirk = false;
}

// -------------------------------------------------------------------------------------------------------

RenderBlock::RenderBlock(Node* node)
      : RenderFlow(node)
      , m_floatingObjects(0)
      , m_positionedObjects(0)
      , m_maxMargin(0)
      , m_overflowHeight(0)
      , m_overflowWidth(0)
      , m_overflowLeft(0)
      , m_overflowTop(0)
      , m_widthForTextAutosizing(-1)
{
}

RenderBlock::~RenderBlock()
{
    delete m_floatingObjects;
    delete m_positionedObjects;
    delete m_maxMargin;
    
    if (m_hasColumns)
        delete gColumnInfoMap->take(this);

    if (gPercentHeightDescendantsMap) {
        if (HashSet<RenderBox*>* descendantSet = gPercentHeightDescendantsMap->take(this)) {
            HashSet<RenderBox*>::iterator end = descendantSet->end();
            for (HashSet<RenderBox*>::iterator descendant = descendantSet->begin(); descendant != end; ++descendant) {
                HashSet<RenderBlock*>* containerSet = gPercentHeightContainerMap->get(*descendant);
                ASSERT(containerSet);
                if (!containerSet)
                    continue;
                ASSERT(containerSet->contains(this));
                containerSet->remove(this);
                if (containerSet->isEmpty()) {
                    gPercentHeightContainerMap->remove(*descendant);
                    delete containerSet;
                }
            }
            delete descendantSet;
        }
    }
}

void RenderBlock::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    setReplaced(newStyle->isDisplayReplacedType());
    
    if (style() && parent() && diff == StyleDifferenceLayout && style()->position() != newStyle->position()) {
        if (newStyle->position() == StaticPosition)
            // Clear our positioned objects list. Our absolutely positioned descendants will be
            // inserted into our containing block's positioned objects list during layout.
            removePositionedObjects(0);
        else if (style()->position() == StaticPosition) {
            // Remove our absolutely positioned descendants from their current containing block.
            // They will be inserted into our positioned objects list during layout.
            RenderObject* cb = parent();
            while (cb && (cb->style()->position() == StaticPosition || (cb->isInline() && !cb->isReplaced())) && !cb->isRenderView()) {
                if (cb->style()->position() == RelativePosition && cb->isInline() && !cb->isReplaced()) {
                    cb = cb->containingBlock();
                    break;
                }
                cb = cb->parent();
            }
            
            if (cb->isRenderBlock())
                static_cast<RenderBlock*>(cb)->removePositionedObjects(this);
        }
    }

    RenderFlow::styleWillChange(diff, newStyle);
}

void RenderBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderFlow::styleDidChange(diff, oldStyle);

    // FIXME: We could save this call when the change only affected non-inherited properties
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isAnonymousBlock()) {
            RefPtr<RenderStyle> newStyle = RenderStyle::create();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(BLOCK);
            child->setStyle(newStyle.release());
        }
    }

    m_lineHeight = -1;

    // Update pseudos for :before and :after now.
    if (!isAnonymous() && document()->usesBeforeAfterRules() && canHaveChildren()) {
        updateBeforeAfterContent(RenderStyle::BEFORE);
        updateBeforeAfterContent(RenderStyle::AFTER);
    }
    updateFirstLetter();
}

void RenderBlock::addChildToFlow(RenderObject* newChild, RenderObject* beforeChild)
{
    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild && isAfterContent(lastChild()))
        beforeChild = lastChild();

    bool madeBoxesNonInline = false;

    // If the requested beforeChild is not one of our children, then this is because
    // there is an anonymous container within this object that contains the beforeChild.
    if (beforeChild && beforeChild->parent() != this) {
        RenderObject* anonymousChild = beforeChild->parent();
        ASSERT(anonymousChild);

        while (anonymousChild->parent() != this)
            anonymousChild = anonymousChild->parent();

        ASSERT(anonymousChild->isAnonymous());

        if (anonymousChild->isAnonymousBlock()) {
            // Insert the child into the anonymous block box instead of here.
            if (newChild->isInline() || beforeChild->parent()->firstChild() != beforeChild)
                beforeChild->parent()->addChild(newChild, beforeChild);
            else
                addChildToFlow(newChild, beforeChild->parent());
            return;
        }

        ASSERT(anonymousChild->isTable());
        if (newChild->isTableCol() && newChild->style()->display() == TABLE_COLUMN_GROUP
                || newChild->isRenderBlock() && newChild->style()->display() == TABLE_CAPTION
                || newChild->isTableSection()
                || newChild->isTableRow()
                || newChild->isTableCell()) {
            // Insert into the anonymous table.
            anonymousChild->addChild(newChild, beforeChild);
            return;
        }

        // Go on to insert before the anonymous table.
        beforeChild = anonymousChild;
    }

    // A block has to either have all of its children inline, or all of its children as blocks.
    // So, if our children are currently inline and a block child has to be inserted, we move all our
    // inline children into anonymous block boxes.
    if (m_childrenInline && !newChild->isInline() && !newChild->isFloatingOrPositioned()) {
        // This is a block with inline content. Wrap the inline content in anonymous blocks.
        makeChildrenNonInline(beforeChild);
        madeBoxesNonInline = true;

        if (beforeChild && beforeChild->parent() != this) {
            beforeChild = beforeChild->parent();
            ASSERT(beforeChild->isAnonymousBlock());
            ASSERT(beforeChild->parent() == this);
        }
    } else if (!m_childrenInline && (newChild->isFloatingOrPositioned() || newChild->isInline())) {
        // If we're inserting an inline child but all of our children are blocks, then we have to make sure
        // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
        // a new one is created and inserted into our list of children in the appropriate position.
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : lastChild();

        if (afterChild && afterChild->isAnonymousBlock()) {
            afterChild->addChild(newChild);
            return;
        }

        if (newChild->isInline()) {
            // No suitable existing anonymous box - create a new one.
            RenderBlock* newBox = createAnonymousBlock();
            RenderContainer::addChild(newBox, beforeChild);
            newBox->addChild(newChild);
            return;
        }
    }

    RenderContainer::addChild(newChild, beforeChild);
    // ### care about aligned stuff

    if (madeBoxesNonInline && parent() && isAnonymousBlock())
        parent()->removeLeftoverAnonymousBlock(this);
    // this object may be dead here
}

static void getInlineRun(RenderObject* start, RenderObject* boundary,
                         RenderObject*& inlineRunStart,
                         RenderObject*& inlineRunEnd)
{
    // Beginning at |start| we find the largest contiguous run of inlines that
    // we can.  We denote the run with start and end points, |inlineRunStart|
    // and |inlineRunEnd|.  Note that these two values may be the same if
    // we encounter only one inline.
    //
    // We skip any non-inlines we encounter as long as we haven't found any
    // inlines yet.
    //
    // |boundary| indicates a non-inclusive boundary point.  Regardless of whether |boundary|
    // is inline or not, we will not include it in a run with inlines before it.  It's as though we encountered
    // a non-inline.
    
    // Start by skipping as many non-inlines as we can.
    RenderObject * curr = start;
    bool sawInline;
    do {
        while (curr && !(curr->isInline() || curr->isFloatingOrPositioned()))
            curr = curr->nextSibling();
        
        inlineRunStart = inlineRunEnd = curr;
        
        if (!curr)
            return; // No more inline children to be found.
        
        sawInline = curr->isInline();
        
        curr = curr->nextSibling();
        while (curr && (curr->isInline() || curr->isFloatingOrPositioned()) && (curr != boundary)) {
            inlineRunEnd = curr;
            if (curr->isInline())
                sawInline = true;
            curr = curr->nextSibling();
        }
    } while (!sawInline);
}

void RenderBlock::deleteLineBoxTree()
{
    InlineFlowBox* line = m_firstLineBox;
    InlineFlowBox* nextLine;
    while (line) {
        nextLine = line->nextFlowBox();
        line->deleteLine(renderArena());
        line = nextLine;
    }
    m_firstLineBox = m_lastLineBox = 0;
}

void RenderBlock::makeChildrenNonInline(RenderObject *insertionPoint)
{    
    // makeChildrenNonInline takes a block whose children are *all* inline and it
    // makes sure that inline children are coalesced under anonymous
    // blocks.  If |insertionPoint| is defined, then it represents the insertion point for
    // the new block child that is causing us to have to wrap all the inlines.  This
    // means that we cannot coalesce inlines before |insertionPoint| with inlines following
    // |insertionPoint|, because the new child is going to be inserted in between the inlines,
    // splitting them.
    ASSERT(isInlineBlockOrInlineTable() || !isInline());
    ASSERT(!insertionPoint || insertionPoint->parent() == this);

    m_childrenInline = false;

    RenderObject *child = firstChild();
    if (!child)
        return;

    deleteLineBoxTree();

    while (child) {
        RenderObject *inlineRunStart, *inlineRunEnd;
        getInlineRun(child, insertionPoint, inlineRunStart, inlineRunEnd);

        if (!inlineRunStart)
            break;

        child = inlineRunEnd->nextSibling();

        RenderBlock* box = createAnonymousBlock();
        insertChildNode(box, inlineRunStart);
        RenderObject* o = inlineRunStart;
        while(o != inlineRunEnd)
        {
            RenderObject* no = o;
            o = no->nextSibling();
            box->moveChildNode(no);
        }
        box->moveChildNode(inlineRunEnd);
    }

#ifndef NDEBUG
    for (RenderObject *c = firstChild(); c; c = c->nextSibling())
        ASSERT(!c->isInline());
#endif

    repaint();
}

void RenderBlock::removeChild(RenderObject *oldChild)
{
    // If this child is a block, and if our previous and next siblings are
    // both anonymous blocks with inline content, then we can go ahead and
    // fold the inline content back together.
    RenderObject* prev = oldChild->previousSibling();
    RenderObject* next = oldChild->nextSibling();
    bool canDeleteAnonymousBlocks = !documentBeingDestroyed() && !isInline() && !oldChild->isInline() && 
                                    !oldChild->virtualContinuation() && 
                                    (!prev || (prev->isAnonymousBlock() && prev->childrenInline())) &&
                                    (!next || (next->isAnonymousBlock() && next->childrenInline()));
    if (canDeleteAnonymousBlocks && prev && next) {
        // Take all the children out of the |next| block and put them in
        // the |prev| block.
        prev->setNeedsLayoutAndPrefWidthsRecalc();
        RenderObject* o = next->firstChild();
        while (o) {
            RenderObject* no = o;
            o = no->nextSibling();
            prev->moveChildNode(no);
        }
 
        RenderBlock* nextBlock = static_cast<RenderBlock*>(next);
        nextBlock->deleteLineBoxTree();
        
        // Nuke the now-empty block.
        next->destroy();
    }

    RenderFlow::removeChild(oldChild);

    RenderObject* child = prev ? prev : next;
    if (canDeleteAnonymousBlocks && child && !child->previousSibling() && !child->nextSibling() && !isFlexibleBox()) {
        // The removal has knocked us down to containing only a single anonymous
        // box.  We can go ahead and pull the content right back up into our
        // box.
        setNeedsLayoutAndPrefWidthsRecalc();
        RenderBlock* anonBlock = static_cast<RenderBlock*>(removeChildNode(child, false));
        m_childrenInline = true;
        RenderObject* o = anonBlock->firstChild();
        while (o) {
            RenderObject* no = o;
            o = no->nextSibling();
            moveChildNode(no);
        }

        // Delete the now-empty block's lines and nuke it.
        anonBlock->deleteLineBoxTree();
        anonBlock->destroy();
    }
}

int RenderBlock::overflowHeight(bool includeInterior) const
{
    if (!includeInterior && hasOverflowClip()) {
        int shadowHeight = 0;
        for (ShadowData* boxShadow = style()->boxShadow(); boxShadow; boxShadow = boxShadow->next)
            shadowHeight = max(boxShadow->y + boxShadow->blur, shadowHeight);
        int inflatedHeight = height() + shadowHeight;
        if (hasReflection())
            inflatedHeight = max(inflatedHeight, reflectionBox().bottom());
        return inflatedHeight;
    }
    return m_overflowHeight;
}

int RenderBlock::overflowWidth(bool includeInterior) const
{
    if (!includeInterior && hasOverflowClip()) {
        int shadowWidth = 0;
        for (ShadowData* boxShadow = style()->boxShadow(); boxShadow; boxShadow = boxShadow->next)
            shadowWidth = max(boxShadow->x + boxShadow->blur, shadowWidth);
        int inflatedWidth = width() + shadowWidth;
        if (hasReflection())
            inflatedWidth = max(inflatedWidth, reflectionBox().right());
        return inflatedWidth;
    }
    return m_overflowWidth;
}

int RenderBlock::overflowLeft(bool includeInterior) const
{
    if (!includeInterior && hasOverflowClip()) {
        int shadowLeft = 0;
        for (ShadowData* boxShadow = style()->boxShadow(); boxShadow; boxShadow = boxShadow->next)
            shadowLeft = min(boxShadow->x - boxShadow->blur, shadowLeft);
        int left = shadowLeft;
        if (hasReflection())
            left = min(left, reflectionBox().x());
        return left;
    }
    return m_overflowLeft;
}

int RenderBlock::overflowTop(bool includeInterior) const
{
    if (!includeInterior && hasOverflowClip()) {
        int shadowTop = 0;
        for (ShadowData* boxShadow = style()->boxShadow(); boxShadow; boxShadow = boxShadow->next)
            shadowTop = min(boxShadow->y - boxShadow->blur, shadowTop);
        int top = shadowTop;
        if (hasReflection())
            top = min(top, reflectionBox().y());
        return top;
    }
    return m_overflowTop;
}

IntRect RenderBlock::overflowRect(bool includeInterior) const
{
    if (!includeInterior && hasOverflowClip()) {
        IntRect box = borderBoxRect();
        int shadowLeft = 0;
        int shadowRight = 0;
        int shadowTop = 0;
        int shadowBottom = 0;

        for (ShadowData* boxShadow = style()->boxShadow(); boxShadow; boxShadow = boxShadow->next) {
            shadowLeft = min(boxShadow->x - boxShadow->blur, shadowLeft);
            shadowRight = max(boxShadow->x + boxShadow->blur, shadowRight);
            shadowTop = min(boxShadow->y - boxShadow->blur, shadowTop);
            shadowBottom = max(boxShadow->y + boxShadow->blur, shadowBottom);
        }

        box.move(shadowLeft, shadowTop);
        box.setWidth(box.width() - shadowLeft + shadowRight);
        box.setHeight(box.height() - shadowTop + shadowBottom);

        if (hasReflection()) {
            IntRect reflection(reflectionBox());
            int reflectTop = min(box.y(), reflection.y());
            int reflectBottom = max(box.bottom(), reflection.bottom());
            box.setHeight(reflectBottom - reflectTop);
            box.setY(reflectTop);
            
            int reflectLeft = min(box.x(), reflection.x());
            int reflectRight = max(box.right(), reflection.right());
            box.setWidth(reflectRight - reflectLeft);
            box.setX(reflectLeft);
        }
        return box;
    }

    if (!includeInterior && hasOverflowClip())
        return borderBoxRect();
    int l = overflowLeft(includeInterior);
    int t = overflowTop(includeInterior);
    return IntRect(l, t, overflowWidth(includeInterior) - l, max(overflowHeight(includeInterior), height()) - t);
}

bool RenderBlock::isSelfCollapsingBlock() const
{
    // We are not self-collapsing if we
    // (a) have a non-zero height according to layout (an optimization to avoid wasting time)
    // (b) are a table,
    // (c) have border/padding,
    // (d) have a min-height
    // (e) have specified that one of our margins can't collapse using a CSS extension
    if (height() > 0 ||
        isTable() || (borderBottom() + paddingBottom() + borderTop() + paddingTop()) != 0 ||
        style()->minHeight().isPositive() || 
        style()->marginTopCollapse() == MSEPARATE || style()->marginBottomCollapse() == MSEPARATE)
        return false;

    bool hasAutoHeight = style()->height().isAuto();
    if (style()->height().isPercent() && !style()->htmlHacks()) {
        hasAutoHeight = true;
        for (RenderBlock* cb = containingBlock(); !cb->isRenderView(); cb = cb->containingBlock()) {
            if (cb->style()->height().isFixed() || cb->isTableCell())
                hasAutoHeight = false;
        }
    }

    // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
    // on whether we have content that is all self-collapsing or not.
    if (hasAutoHeight || ((style()->height().isFixed() || style()->height().isPercent()) && style()->height().isZero())) {
        // If the block has inline children, see if we generated any line boxes.  If we have any
        // line boxes, then we can't be self-collapsing, since we have content.
        if (childrenInline())
            return !firstLineBox();
        
        // Whether or not we collapse is dependent on whether all our normal flow children
        // are also self-collapsing.
        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->isFloatingOrPositioned())
                continue;
            if (!child->isSelfCollapsingBlock())
                return false;
        }
        return true;
    }
    return false;
}

void RenderBlock::layout()
{
    // Update our first letter info now.
    updateFirstLetter();

    // Table cells call layoutBlock directly, so don't add any logic here.  Put code into
    // layoutBlock().
    layoutBlock(false);
    
    // It's safe to check for control clip here, since controls can never be table cells.
    if (hasControlClip()) {
        // Because of the lightweight clip, there can never be any overflow from children.
        m_overflowWidth = width();
        m_overflowHeight = height();
        m_overflowLeft = 0;
        m_overflowTop = 0;
    }
}

void RenderBlock::layoutBlock(bool relayoutChildren)
{
    ASSERT(needsLayout());

    if (isInline() && !isInlineBlockOrInlineTable()) // Inline <form>s inside various table elements can
        return;                                      // cause us to come in here.  Just bail.

    if (!relayoutChildren && layoutOnlyPositionedObjects())
        return;

    LayoutRepainter repainter(*this, m_everHadLayout && checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(view(), this, IntSize(x(), y()), m_hasColumns || hasTransform() || hasReflection());

    int oldWidth = width();
    int oldColumnWidth = desiredColumnWidth();

    calcWidth();
    calcColumnWidth();

    m_overflowWidth = width();
    m_overflowLeft = 0;

    if (oldWidth != width() || oldColumnWidth != desiredColumnWidth())
        relayoutChildren = true;

    clearFloats();

    int previousHeight = height();
    setHeight(0);

    m_overflowHeight = 0;

    // We use four values, maxTopPos, maxPosNeg, maxBottomPos, and maxBottomNeg, to track
    // our current maximal positive and negative margins.  These values are used when we
    // are collapsed with adjacent blocks, so for example, if you have block A and B
    // collapsing together, then you'd take the maximal positive margin from both A and B
    // and subtract it from the maximal negative margin from both A and B to get the
    // true collapsed margin.  This algorithm is recursive, so when we finish layout()
    // our block knows its current maximal positive/negative values.
    //
    // Start out by setting our margin values to our current margins.  Table cells have
    // no margins, so we don't fill in the values for table cells.
    bool isCell = isTableCell();
    if (!isCell) {
        initMaxMarginValues();

        m_topMarginQuirk = style()->marginTop().quirk();
        m_bottomMarginQuirk = style()->marginBottom().quirk();

        Node* node = element();
        if (node && node->hasTagName(formTag) && static_cast<HTMLFormElement*>(node)->isMalformed()) {
            // See if this form is malformed (i.e., unclosed). If so, don't give the form
            // a bottom margin.
            setMaxBottomMargins(0, 0);
        }
    }

    // For overflow:scroll blocks, ensure we have both scrollbars in place always.
    if (scrollsOverflow()) {
        if (style()->overflowX() == OSCROLL)
            m_layer->setHasHorizontalScrollbar(true);
        if (style()->overflowY() == OSCROLL)
            m_layer->setHasVerticalScrollbar(true);
    }

    int repaintTop = 0;
    int repaintBottom = 0;
    int maxFloatBottom = 0;
    if (childrenInline())
        layoutInlineChildren(relayoutChildren, repaintTop, repaintBottom);
    else
        layoutBlockChildren(relayoutChildren, maxFloatBottom);

    // Expand our intrinsic height to encompass floats.
    int toAdd = borderBottom() + paddingBottom() + horizontalScrollbarHeight();
    if (floatBottom() > (height() - toAdd) && expandsToEncloseOverhangingFloats())
        setHeight(floatBottom() + toAdd);
    
    // Now lay out our columns within this intrinsic height, since they can slightly affect the intrinsic height as
    // we adjust for clean column breaks.
    int singleColumnBottom = layoutColumns();

    // Calculate our new height.
    int oldHeight = height();
    calcHeight();
    if (oldHeight != height()) {
        if (oldHeight > height() && maxFloatBottom > height() && !childrenInline()) {
            // One of our children's floats may have become an overhanging float for us. We need to look for it.
            for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
                if (child->isBlockFlow() && !child->isFloatingOrPositioned()) {
                    RenderBlock* block = static_cast<RenderBlock*>(child);
                    if (block->floatBottom() + block->y() > height())
                        addOverhangingFloats(block, -block->x(), -block->y(), false);
                }
            }
        }
        // We have to rebalance columns to the new height.
        layoutColumns(singleColumnBottom);

        // If the block got expanded in size, then increase our overflowheight to match.
        if (m_overflowHeight > height())
            m_overflowHeight -= toAdd;
        if (m_overflowHeight < height())
            m_overflowHeight = height();
    }
    if (previousHeight != height())
        relayoutChildren = true;

    if ((isCell || isInline() || isFloatingOrPositioned() || isRoot()) && !hasOverflowClip() && !hasControlClip())
        addVisualOverflow(floatRect());

    layoutPositionedObjects(relayoutChildren || isRoot());

    positionListMarker();

    // Always ensure our overflow width/height are at least as large as our width/height.
    m_overflowWidth = max(m_overflowWidth, width());
    m_overflowHeight = max(m_overflowHeight, height());

    if (!hasOverflowClip()) {
        for (ShadowData* boxShadow = style()->boxShadow(); boxShadow; boxShadow = boxShadow->next) {
            m_overflowLeft = min(m_overflowLeft, boxShadow->x - boxShadow->blur);
            m_overflowWidth = max(m_overflowWidth, width() + boxShadow->x + boxShadow->blur);
            m_overflowTop = min(m_overflowTop, boxShadow->y - boxShadow->blur);
            m_overflowHeight = max(m_overflowHeight, height() + boxShadow->y + boxShadow->blur);
        }
        
        if (hasReflection()) {
            m_overflowTop = min(m_overflowTop, reflectionBox().y());
            m_overflowHeight = max(m_overflowHeight, reflectionBox().bottom());
        }
    }

    statePusher.pop();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip())
        m_layer->updateScrollInfoAfterLayout();

    // Repaint with our new bounds if they are different from our old bounds.
    bool didFullRepaint = repainter.repaintAfterLayout();
    if (!didFullRepaint && repaintTop != repaintBottom && (style()->visibility() == VISIBLE || enclosingLayer()->hasVisibleContent())) {
        IntRect repaintRect(m_overflowLeft, repaintTop, m_overflowWidth - m_overflowLeft, repaintBottom - repaintTop);

        // FIXME: Deal with multiple column repainting.  We have to split the repaint
        // rect up into multiple rects if it spans columns.

        repaintRect.inflate(maximalOutlineSize(PaintPhaseOutline));
        
        if (hasOverflowClip()) {
            // Adjust repaint rect for scroll offset
            int x = repaintRect.x();
            int y = repaintRect.y();
            layer()->subtractScrolledContentOffset(x, y);
            repaintRect.setX(x);
            repaintRect.setY(y);

            // Don't allow this rect to spill out of our overflow box.
            repaintRect.intersect(IntRect(0, 0, width(), height()));
        }

        // Make sure the rect is still non-empty after intersecting for overflow above
        if (!repaintRect.isEmpty()) {
            repaintRectangle(repaintRect); // We need to do a partial repaint of our content.
            if (hasReflection())
                repaintRectangle(reflectedRect(repaintRect));
        }
    }
    setNeedsLayout(false);
}

bool RenderBlock::expandsToEncloseOverhangingFloats() const
{
    return isInlineBlockOrInlineTable() || isFloatingOrPositioned() || hasOverflowClip() || (parent() && parent()->isFlexibleBox()) || m_hasColumns || isTableCell() || isFieldset();
}

void RenderBlock::adjustPositionedBlock(RenderBox* child, const MarginInfo& marginInfo)
{
    if (child->hasStaticX()) {
        if (style()->direction() == LTR)
            child->setStaticX(borderLeft() + paddingLeft());
        else
            child->setStaticX(borderRight() + paddingRight());
    }

    if (child->hasStaticY()) {
        int y = height();
        if (!marginInfo.canCollapseWithTop()) {
            child->calcVerticalMargins();
            int marginTop = child->marginTop();
            int collapsedTopPos = marginInfo.posMargin();
            int collapsedTopNeg = marginInfo.negMargin();
            if (marginTop > 0) {
                if (marginTop > collapsedTopPos)
                    collapsedTopPos = marginTop;
            } else {
                if (-marginTop > collapsedTopNeg)
                    collapsedTopNeg = -marginTop;
            }
            y += (collapsedTopPos - collapsedTopNeg) - marginTop;
        }
        child->setStaticY(y);
    }
}

void RenderBlock::adjustFloatingBlock(const MarginInfo& marginInfo)
{
    // The float should be positioned taking into account the bottom margin
    // of the previous flow.  We add that margin into the height, get the
    // float positioned properly, and then subtract the margin out of the
    // height again.  In the case of self-collapsing blocks, we always just
    // use the top margins, since the self-collapsing block collapsed its
    // own bottom margin into its top margin.
    //
    // Note also that the previous flow may collapse its margin into the top of
    // our block.  If this is the case, then we do not add the margin in to our
    // height when computing the position of the float.   This condition can be tested
    // for by simply calling canCollapseWithTop.  See
    // http://www.hixie.ch/tests/adhoc/css/box/block/margin-collapse/046.html for
    // an example of this scenario.
    int marginOffset = marginInfo.canCollapseWithTop() ? 0 : marginInfo.margin();
    setHeight(height() + marginOffset);
    positionNewFloats();
    setHeight(height() - marginOffset);
}

RenderBox* RenderBlock::handleSpecialChild(RenderBox* child, const MarginInfo& marginInfo, bool& handled)
{
    // Handle positioned children first.
    RenderBox* next = handlePositionedChild(child, marginInfo, handled);
    if (handled) return next;
    
    // Handle floating children next.
    next = handleFloatingChild(child, marginInfo, handled);
    if (handled) return next;

    // Finally, see if we have a run-in element.
    return handleRunInChild(child, handled);
}


RenderBox* RenderBlock::handlePositionedChild(RenderBox* child, const MarginInfo& marginInfo, bool& handled)
{
    if (child->isPositioned()) {
        handled = true;
        child->containingBlock()->insertPositionedObject(child);
        adjustPositionedBlock(child, marginInfo);
        return child->nextSiblingBox();
    }

    return 0;
}

RenderBox* RenderBlock::handleFloatingChild(RenderBox* child, const MarginInfo& marginInfo, bool& handled)
{
    if (child->isFloating()) {
        handled = true;
        insertFloatingObject(child);
        adjustFloatingBlock(marginInfo);
        return child->nextSiblingBox();
    }
    
    return 0;
}

RenderBox* RenderBlock::handleRunInChild(RenderBox* blockRunIn, bool& handled)
{
    // See if we have a run-in element with inline children.  If the
    // children aren't inline, then just treat the run-in as a normal
    // block.
    if (blockRunIn->isRunIn() && (blockRunIn->childrenInline() || blockRunIn->isReplaced())) {
        // Get the next non-positioned/non-floating RenderBlock.
        RenderObject* curr = blockRunIn->nextSibling();
        while (curr && curr->isFloatingOrPositioned())
            curr = curr->nextSibling();
        if (curr && (curr->isRenderBlock() && curr->childrenInline() && !curr->isRunIn())) {
            // The block acts like an inline, so just null out its
            // position.
            handled = true;
            
            // Remove the old child.
            RenderBox* next = blockRunIn->nextSiblingBox();
            removeChildNode(blockRunIn);

            // Create an inline.
            RenderInline* inlineRunIn = new (renderArena()) RenderInline(blockRunIn->node());
            inlineRunIn->setStyle(blockRunIn->style());

            // Move the nodes from the old child to the new child, but skip any :before/:after content.  It has already
            // been regenerated by the new inline.
            for (RenderObject* runInChild = blockRunIn->firstChild(); runInChild; runInChild = runInChild->nextSibling()) {
                if (runInChild->style()->styleType() != RenderStyle::BEFORE && runInChild->style()->styleType() != RenderStyle::AFTER) {
                    blockRunIn->removeChildNode(runInChild, false);
                    inlineRunIn->addChild(runInChild); // Use addChild instead of appendChildNode since it handles correct placement of the children relative to :after-generated content.
                }
            }

            // Now insert the new child under |curr|.
            curr->insertChildNode(inlineRunIn, curr->firstChild());
            
            // If the run-in had an element, we need to set the new renderer.
            if (blockRunIn->element())
                blockRunIn->element()->setRenderer(inlineRunIn);
            
            // Destroy the block run-in.
            blockRunIn->destroy();

            return next;
        }
    }
    return 0;
}

void RenderBlock::collapseMargins(RenderBox* child, MarginInfo& marginInfo, int yPosEstimate)
{
    // Get our max pos and neg top margins.
    int posTop = child->maxTopMargin(true);
    int negTop = child->maxTopMargin(false);

    // For self-collapsing blocks, collapse our bottom margins into our
    // top to get new posTop and negTop values.
    if (child->isSelfCollapsingBlock()) {
        posTop = max(posTop, child->maxBottomMargin(true));
        negTop = max(negTop, child->maxBottomMargin(false));
    }
    
    // See if the top margin is quirky. We only care if this child has
    // margins that will collapse with us.
    bool topQuirk = child->isTopMarginQuirk() || style()->marginTopCollapse() == MDISCARD;

    if (marginInfo.canCollapseWithTop()) {
        // This child is collapsing with the top of the
        // block.  If it has larger margin values, then we need to update
        // our own maximal values.
        if (!style()->htmlHacks() || !marginInfo.quirkContainer() || !topQuirk)
            setMaxTopMargins(max(posTop, maxTopPosMargin()), max(negTop, maxTopNegMargin()));

        // The minute any of the margins involved isn't a quirk, don't
        // collapse it away, even if the margin is smaller (www.webreference.com
        // has an example of this, a <dt> with 0.8em author-specified inside
        // a <dl> inside a <td>.
        if (!marginInfo.determinedTopQuirk() && !topQuirk && (posTop-negTop)) {
            m_topMarginQuirk = false;
            marginInfo.setDeterminedTopQuirk(true);
        }

        if (!marginInfo.determinedTopQuirk() && topQuirk && marginTop() == 0)
            // We have no top margin and our top child has a quirky margin.
            // We will pick up this quirky margin and pass it through.
            // This deals with the <td><div><p> case.
            // Don't do this for a block that split two inlines though.  You do
            // still apply margins in this case.
            m_topMarginQuirk = true;
    }

    if (marginInfo.quirkContainer() && marginInfo.atTopOfBlock() && (posTop - negTop))
        marginInfo.setTopQuirk(topQuirk);

    int ypos = height();
    if (child->isSelfCollapsingBlock()) {
        // This child has no height.  We need to compute our
        // position before we collapse the child's margins together,
        // so that we can get an accurate position for the zero-height block.
        int collapsedTopPos = max(marginInfo.posMargin(), child->maxTopMargin(true));
        int collapsedTopNeg = max(marginInfo.negMargin(), child->maxTopMargin(false));
        marginInfo.setMargin(collapsedTopPos, collapsedTopNeg);
        
        // Now collapse the child's margins together, which means examining our
        // bottom margin values as well. 
        marginInfo.setPosMarginIfLarger(child->maxBottomMargin(true));
        marginInfo.setNegMarginIfLarger(child->maxBottomMargin(false));

        if (!marginInfo.canCollapseWithTop())
            // We need to make sure that the position of the self-collapsing block
            // is correct, since it could have overflowing content
            // that needs to be positioned correctly (e.g., a block that
            // had a specified height of 0 but that actually had subcontent).
            ypos = height() + collapsedTopPos - collapsedTopNeg;
    }
    else {
        if (child->style()->marginTopCollapse() == MSEPARATE) {
            setHeight(height() + marginInfo.margin() + child->marginTop());
            ypos = height();
        }
        else if (!marginInfo.atTopOfBlock() ||
            (!marginInfo.canCollapseTopWithChildren()
             && (!style()->htmlHacks() || !marginInfo.quirkContainer() || !marginInfo.topQuirk()))) {
            // We're collapsing with a previous sibling's margins and not
            // with the top of the block.
            setHeight(height() + max(marginInfo.posMargin(), posTop) - max(marginInfo.negMargin(), negTop));
            ypos = height();
        }

        marginInfo.setPosMargin(child->maxBottomMargin(true));
        marginInfo.setNegMargin(child->maxBottomMargin(false));

        if (marginInfo.margin())
            marginInfo.setBottomQuirk(child->isBottomMarginQuirk() || style()->marginBottomCollapse() == MDISCARD);

        marginInfo.setSelfCollapsingBlockClearedFloat(false);
    }

    view()->addLayoutDelta(IntSize(0, yPosEstimate - ypos));
    child->setLocation(child->x(), ypos);
    if (ypos != yPosEstimate) {
        if (child->shrinkToAvoidFloats())
            // The child's width depends on the line width.
            // When the child shifts to clear an item, its width can
            // change (because it has more available line width).
            // So go ahead and mark the item as dirty.
            child->setChildNeedsLayout(true, false);

        if (!child->avoidsFloats() && child->containsFloats())
            static_cast<RenderBlock*>(child)->markAllDescendantsWithFloatsForLayout();

        // Our guess was wrong. Make the child lay itself out again.
        child->layoutIfNeeded();
    }
}

void RenderBlock::clearFloatsIfNeeded(RenderBox* child, MarginInfo& marginInfo, int oldTopPosMargin, int oldTopNegMargin)
{
    int heightIncrease = getClearDelta(child);
    if (!heightIncrease)
        return;

    // The child needs to be lowered.  Move the child so that it just clears the float.
    view()->addLayoutDelta(IntSize(0, -heightIncrease));
    child->setLocation(child->x(), child->y() + heightIncrease);

    if (child->isSelfCollapsingBlock()) {
        // For self-collapsing blocks that clear, they can still collapse their
        // margins with following siblings.  Reset the current margins to represent
        // the self-collapsing block's margins only.
        marginInfo.setPosMargin(max(child->maxTopMargin(true), child->maxBottomMargin(true)));
        marginInfo.setNegMargin(max(child->maxTopMargin(false), child->maxBottomMargin(false)));
        
        // Adjust our height such that we are ready to be collapsed with subsequent siblings.
        setHeight(child->y() - max(0, marginInfo.margin()));
        
        // Set a flag that we cleared a float so that we know both to increase the height of the block
        // to compensate for the clear and to avoid collapsing our margins with the parent block's
        // bottom margin.
        marginInfo.setSelfCollapsingBlockClearedFloat(true);
    } else
        // Increase our height by the amount we had to clear.
        setHeight(height() + heightIncrease);
    
    if (marginInfo.canCollapseWithTop()) {
        // We can no longer collapse with the top of the block since a clear
        // occurred.  The empty blocks collapse into the cleared block.
        // FIXME: This isn't quite correct.  Need clarification for what to do
        // if the height the cleared block is offset by is smaller than the
        // margins involved.
        setMaxTopMargins(oldTopPosMargin, oldTopNegMargin);
        marginInfo.setAtTopOfBlock(false);
    }

    // If our value of clear caused us to be repositioned vertically to be
    // underneath a float, we might have to do another layout to take into account
    // the extra space we now have available.
    if (child->shrinkToAvoidFloats())
        // The child's width depends on the line width.
        // When the child shifts to clear an item, its width can
        // change (because it has more available line width).
        // So go ahead and mark the item as dirty.
        child->setChildNeedsLayout(true, false);
    if (!child->avoidsFloats() && child->containsFloats())
        static_cast<RenderBlock*>(child)->markAllDescendantsWithFloatsForLayout();
    child->layoutIfNeeded();
}

int RenderBlock::estimateVerticalPosition(RenderBox* child, const MarginInfo& marginInfo)
{
    // FIXME: We need to eliminate the estimation of vertical position, because when it's wrong we sometimes trigger a pathological
    // relayout if there are intruding floats.
    int yPosEstimate = height();
    if (!marginInfo.canCollapseWithTop()) {
        int childMarginTop = child->selfNeedsLayout() ? child->marginTop() : child->collapsedMarginTop();
        yPosEstimate += max(marginInfo.margin(), childMarginTop);
    }
    return yPosEstimate;
}

void RenderBlock::determineHorizontalPosition(RenderBox* child)
{
    if (style()->direction() == LTR) {
        int xPos = borderLeft() + paddingLeft();
        
        // Add in our left margin.
        int chPos = xPos + child->marginLeft();
        
        // Some objects (e.g., tables, horizontal rules, overflow:auto blocks) avoid floats.  They need
        // to shift over as necessary to dodge any floats that might get in the way.
        if (child->avoidsFloats()) {
            int leftOff = leftOffset(height());
            if (style()->textAlign() != WEBKIT_CENTER && child->style()->marginLeft().type() != Auto) {
                if (child->marginLeft() < 0)
                    leftOff += child->marginLeft();
                chPos = max(chPos, leftOff); // Let the float sit in the child's margin if it can fit.
            }
            else if (leftOff != xPos) {
                // The object is shifting right. The object might be centered, so we need to
                // recalculate our horizontal margins. Note that the containing block content
                // width computation will take into account the delta between |leftOff| and |xPos|
                // so that we can just pass the content width in directly to the |calcHorizontalMargins|
                // function.
                child->calcHorizontalMargins(child->style()->marginLeft(), child->style()->marginRight(), lineWidth(child->y()));
                chPos = leftOff + child->marginLeft();
            }
        }
        view()->addLayoutDelta(IntSize(child->x() - chPos, 0));
        child->setLocation(chPos, child->y());
    } else {
        int xPos = width() - borderRight() - paddingRight() - verticalScrollbarWidth();
        int chPos = xPos - (child->width() + child->marginRight());
        if (child->avoidsFloats()) {
            int rightOff = rightOffset(height());
            if (style()->textAlign() != WEBKIT_CENTER && child->style()->marginRight().type() != Auto) {
                if (child->marginRight() < 0)
                    rightOff -= child->marginRight();
                chPos = min(chPos, rightOff - child->width()); // Let the float sit in the child's margin if it can fit.
            } else if (rightOff != xPos) {
                // The object is shifting left. The object might be centered, so we need to
                // recalculate our horizontal margins. Note that the containing block content
                // width computation will take into account the delta between |rightOff| and |xPos|
                // so that we can just pass the content width in directly to the |calcHorizontalMargins|
                // function.
                toRenderBox(child)->calcHorizontalMargins(child->style()->marginLeft(), child->style()->marginRight(), lineWidth(child->y()));
                chPos = rightOff - child->marginRight() - child->width();
            }
        }
        view()->addLayoutDelta(IntSize(child->x() - chPos, 0));
        child->setLocation(chPos, child->y());
    }
}

void RenderBlock::setCollapsedBottomMargin(const MarginInfo& marginInfo)
{
    if (marginInfo.canCollapseWithBottom() && !marginInfo.canCollapseWithTop()) {
        // Update our max pos/neg bottom margins, since we collapsed our bottom margins
        // with our children.
        setMaxBottomMargins(max(maxBottomPosMargin(), marginInfo.posMargin()), max(maxBottomNegMargin(), marginInfo.negMargin()));

        if (!marginInfo.bottomQuirk())
            m_bottomMarginQuirk = false;

        if (marginInfo.bottomQuirk() && marginBottom() == 0)
            // We have no bottom margin and our last child has a quirky margin.
            // We will pick up this quirky margin and pass it through.
            // This deals with the <td><div><p> case.
            m_bottomMarginQuirk = true;
    }
}

void RenderBlock::handleBottomOfBlock(int top, int bottom, MarginInfo& marginInfo)
{
    // If our last flow was a self-collapsing block that cleared a float, then we don't
    // collapse it with the bottom of the block.
    if (!marginInfo.selfCollapsingBlockClearedFloat())
        marginInfo.setAtBottomOfBlock(true);
    else {
        // We have to special case the negative margin situation (where the collapsed
        // margin of the self-collapsing block is negative), since there's no need
        // to make an adjustment in that case.
        if (marginInfo.margin() < 0)
            marginInfo.clearMargin();
    }

    // If we can't collapse with children then go ahead and add in the bottom margin.
    if (!marginInfo.canCollapseWithBottom() && !marginInfo.canCollapseWithTop()
        && (!style()->htmlHacks() || !marginInfo.quirkContainer() || !marginInfo.bottomQuirk()))
        setHeight(height() + marginInfo.margin());
        
    // Now add in our bottom border/padding.
    setHeight(height() + bottom);

    // Negative margins can cause our height to shrink below our minimal height (border/padding).
    // If this happens, ensure that the computed height is increased to the minimal height.
    setHeight(max(height(), top + bottom));

    // Always make sure our overflow height is at least our height.
    m_overflowHeight = max(height(), m_overflowHeight);

    // Update our bottom collapsed margin info.
    setCollapsedBottomMargin(marginInfo);
}

void RenderBlock::layoutBlockChildren(bool relayoutChildren, int& maxFloatBottom)
{
    if (gPercentHeightDescendantsMap) {
        if (HashSet<RenderBox*>* descendants = gPercentHeightDescendantsMap->get(this)) {
            HashSet<RenderBox*>::iterator end = descendants->end();
            for (HashSet<RenderBox*>::iterator it = descendants->begin(); it != end; ++it) {
                RenderBox* box = *it;
                while (box != this) {
                    if (box->normalChildNeedsLayout())
                        break;
                    box->setChildNeedsLayout(true, false);
                    box = box->containingBlock();
                    ASSERT(box);
                    if (!box)
                        break;
                }
            }
        }
    }

    int top = borderTop() + paddingTop();
    int bottom = borderBottom() + paddingBottom() + horizontalScrollbarHeight();

    m_overflowHeight = top;
    setHeight(m_overflowHeight);

    // The margin struct caches all our current margin collapsing state.  The compact struct caches state when we encounter compacts,
    MarginInfo marginInfo(this, top, bottom);

    // Fieldsets need to find their legend and position it inside the border of the object.
    // The legend then gets skipped during normal layout.
    RenderObject* legend = layoutLegend(relayoutChildren);

    int previousFloatBottom = 0;
    maxFloatBottom = 0;

    RenderBox* child = firstChildBox();
    while (child) {
        if (legend == child) {
            child = child->nextSiblingBox();
            continue; // Skip the legend, since it has already been positioned up in the fieldset's border.
        }

        int oldTopPosMargin = maxTopPosMargin();
        int oldTopNegMargin = maxTopNegMargin();

        // Make sure we layout children if they need it.
        // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
        // an auto value.  Add a method to determine this, so that we can avoid the relayout.
        if (relayoutChildren || ((child->style()->height().isPercent() || child->style()->minHeight().isPercent() || child->style()->maxHeight().isPercent()) && !isRenderView()))
            child->setChildNeedsLayout(true, false);

        // If relayoutChildren is set and we have percentage padding, we also need to invalidate the child's pref widths.
        if (relayoutChildren && (child->style()->paddingLeft().isPercent() || child->style()->paddingRight().isPercent()))
            child->setPrefWidthsDirty(true, false);

        // Handle the four types of special elements first.  These include positioned content, floating content, compacts and
        // run-ins.  When we encounter these four types of objects, we don't actually lay them out as normal flow blocks.
        bool handled = false;
        RenderBox* next = handleSpecialChild(child, marginInfo, handled);
        if (handled) {
            child = next;
            continue;
        }

        // The child is a normal flow object.  Compute its vertical margins now.
        child->calcVerticalMargins();

        // Do not allow a collapse if the margin top collapse style is set to SEPARATE.
        if (child->style()->marginTopCollapse() == MSEPARATE) {
            marginInfo.setAtTopOfBlock(false);
            marginInfo.clearMargin();
        }

        // Try to guess our correct y position.  In most cases this guess will
        // be correct.  Only if we're wrong (when we compute the real y position)
        // will we have to potentially relayout.
        int yPosEstimate = estimateVerticalPosition(child, marginInfo);

        // Cache our old rect so that we can dirty the proper repaint rects if the child moves.
        IntRect oldRect(child->x(), child->y() , child->width(), child->height());
#ifndef NDEBUG
        IntSize oldLayoutDelta = view()->layoutDelta();
#endif
        // Go ahead and position the child as though it didn't collapse with the top.
        view()->addLayoutDelta(IntSize(0, child->y() - yPosEstimate));
        child->setLocation(child->x(), yPosEstimate);

        bool markDescendantsWithFloats = false;
        if (yPosEstimate != oldRect.y() && !child->avoidsFloats() && child->containsFloats())
            markDescendantsWithFloats = true;
        else if (!child->avoidsFloats() || child->shrinkToAvoidFloats()) {
            // If an element might be affected by the presence of floats, then always mark it for
            // layout.
            int fb = max(previousFloatBottom, floatBottom());
            if (fb > height() || fb > yPosEstimate)
                markDescendantsWithFloats = true;
        }

        if (markDescendantsWithFloats)
            static_cast<RenderBlock*>(child)->markAllDescendantsWithFloatsForLayout();

        if (child->isRenderBlock())
            previousFloatBottom = max(previousFloatBottom, oldRect.y() + static_cast<RenderBlock*>(child)->floatBottom());

        bool childHadLayout = child->m_everHadLayout;
        bool childNeededLayout = child->needsLayout();
        if (childNeededLayout)
            child->layout();

        // Now determine the correct ypos based off examination of collapsing margin
        // values.
        collapseMargins(child, marginInfo, yPosEstimate);

        // Now check for clear.
        clearFloatsIfNeeded(child, marginInfo, oldTopPosMargin, oldTopNegMargin);

        // We are no longer at the top of the block if we encounter a non-empty child.  
        // This has to be done after checking for clear, so that margins can be reset if a clear occurred.
        if (marginInfo.atTopOfBlock() && !child->isSelfCollapsingBlock())
            marginInfo.setAtTopOfBlock(false);

        // Now place the child in the correct horizontal position
        determineHorizontalPosition(child);

        // Update our height now that the child has been placed in the correct position.
        setHeight(height() + child->height());
        if (child->style()->marginBottomCollapse() == MSEPARATE) {
            setHeight(height() + child->marginBottom());
            marginInfo.clearMargin();
        }
        // If the child has overhanging floats that intrude into following siblings (or possibly out
        // of this block), then the parent gets notified of the floats now.
        maxFloatBottom = max(maxFloatBottom, addOverhangingFloats(static_cast<RenderBlock*>(child), -child->x(), -child->y(), !childNeededLayout));

        // Update our overflow in case the child spills out the block.
        m_overflowTop = min(m_overflowTop, child->y() + child->overflowTop(false));
        m_overflowHeight = max(m_overflowHeight, height() + child->overflowHeight(false) - child->height());
        m_overflowWidth = max(child->x() + child->overflowWidth(false), m_overflowWidth);
        m_overflowLeft = min(child->x() + child->overflowLeft(false), m_overflowLeft);

        IntSize childOffset(child->x() - oldRect.x(), child->y() - oldRect.y());
        if (childOffset.width() || childOffset.height()) {
            view()->addLayoutDelta(childOffset);

            // If the child moved, we have to repaint it as well as any floating/positioned
            // descendants.  An exception is if we need a layout.  In this case, we know we're going to
            // repaint ourselves (and the child) anyway.
            if (childHadLayout && !selfNeedsLayout() && child->checkForRepaintDuringLayout())
                child->repaintDuringLayoutIfMoved(oldRect);
        }

        if (!childHadLayout && child->checkForRepaintDuringLayout())
            child->repaint();

        ASSERT(oldLayoutDelta == view()->layoutDelta());
        child = child->nextSiblingBox();
    }

    // Now do the handling of the bottom of the block, adding in our bottom border/padding and
    // determining the correct collapsed bottom margin information.
    handleBottomOfBlock(top, bottom, marginInfo);
}

bool RenderBlock::layoutOnlyPositionedObjects()
{
    if (!posChildNeedsLayout() || normalChildNeedsLayout() || selfNeedsLayout())
        return false;

    LayoutStateMaintainer statePusher(view(), this, IntSize(x(), y()), m_hasColumns || hasTransform() || hasReflection());

    if (needsPositionedMovementLayout()) {
        tryLayoutDoingPositionedMovementOnly();
        if (needsLayout())
            return false;
    }

    // All we have to is lay out our positioned objects.
    layoutPositionedObjects(false);

    statePusher.pop();

    if (hasOverflowClip())
        m_layer->updateScrollInfoAfterLayout();

    setNeedsLayout(false);
    return true;
}

void RenderBlock::layoutPositionedObjects(bool relayoutChildren)
{
    if (m_positionedObjects) {
        RenderBox* r;
        Iterator end = m_positionedObjects->end();
        for (Iterator it = m_positionedObjects->begin(); it != end; ++it) {
            r = *it;
            // When a non-positioned block element moves, it may have positioned children that are implicitly positioned relative to the
            // non-positioned block.  Rather than trying to detect all of these movement cases, we just always lay out positioned
            // objects that are positioned implicitly like this.  Such objects are rare, and so in typical DHTML menu usage (where everything is
            // positioned explicitly) this should not incur a performance penalty.
            if (relayoutChildren || (r->hasStaticY() && r->parent() != this && r->parent()->isBlockFlow()))
                r->setChildNeedsLayout(true, false);
                
            // If relayoutChildren is set and we have percentage padding, we also need to invalidate the child's pref widths.
            if (relayoutChildren && (r->style()->paddingLeft().isPercent() || r->style()->paddingRight().isPercent()))
                r->setPrefWidthsDirty(true, false);
            
            // We don't have to do a full layout.  We just have to update our position. Try that first. If we have shrink-to-fit width
            // and we hit the available width constraint, the layoutIfNeeded() will catch it and do a full layout.
            if (r->needsPositionedMovementLayoutOnly())
                r->tryLayoutDoingPositionedMovementOnly();
            r->layoutIfNeeded();
        }
    }
}

void RenderBlock::markPositionedObjectsForLayout()
{
    if (m_positionedObjects) {
        RenderBox* r;
        Iterator end = m_positionedObjects->end();
        for (Iterator it = m_positionedObjects->begin(); it != end; ++it) {
            r = *it;
            r->setChildNeedsLayout(true);
        }
    }
}

void RenderBlock::repaintOverhangingFloats(bool paintAllDescendants)
{
    // Repaint any overhanging floats (if we know we're the one to paint them).
    if (hasOverhangingFloats()) {
        // We think that we must be in a bad state if m_floatingObjects is nil at this point, so 
        // we assert on Debug builds and nil-check Release builds.
        ASSERT(m_floatingObjects);
        if (!m_floatingObjects)
            return;
        
        FloatingObject* r;
        DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);

        // FIXME: Avoid disabling LayoutState. At the very least, don't disable it for floats originating
        // in this block. Better yet would be to push extra state for the containers of other floats.
        view()->disableLayoutState();
        for ( ; (r = it.current()); ++it) {
            // Only repaint the object if it is overhanging, is not in its own layer, and
            // is our responsibility to paint (m_shouldPaint is set). When paintAllDescendants is true, the latter
            // condition is replaced with being a descendant of us.
            if (r->m_bottom > height() && (paintAllDescendants && r->m_renderer->isDescendantOf(this) || r->m_shouldPaint) && !r->m_renderer->hasSelfPaintingLayer()) {                
                r->m_renderer->repaint();
                r->m_renderer->repaintOverhangingFloats();
            }
        }
        view()->enableLayoutState();
    }
}

void RenderBlock::paint(PaintInfo& paintInfo, int tx, int ty)
{
    tx += x();
    ty += y();
    
    PaintPhase phase = paintInfo.phase;

    // Check if we need to do anything at all.
    // FIXME: Could eliminate the isRoot() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (!isRoot()) {
        IntRect overflowBox = overflowRect(false);
        overflowBox.inflate(maximalOutlineSize(paintInfo.phase));
        overflowBox.move(tx, ty);
        if (!overflowBox.intersects(paintInfo.rect))
            return;
    }

    bool pushedClip = pushContentsClip(paintInfo, tx, ty);
    paintObject(paintInfo, tx, ty);
    if (pushedClip)
        popContentsClip(paintInfo, phase, tx, ty);

    // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
    // z-index.  We paint after we painted the background/border, so that the scrollbars will
    // sit above the background/border.
    if (hasOverflowClip() && style()->visibility() == VISIBLE && (phase == PaintPhaseBlockBackground || phase == PaintPhaseChildBlockBackground))
        layer()->paintOverflowControls(paintInfo.context, tx, ty, paintInfo.rect);
}

void RenderBlock::paintColumns(PaintInfo& paintInfo, int tx, int ty, bool paintingFloats)
{
    // We need to do multiple passes, breaking up our child painting into strips.
    GraphicsContext* context = paintInfo.context;
    int currXOffset = 0;
    int currYOffset = 0;
    int ruleAdd = borderLeft() + paddingLeft();
    int ruleX = 0;
    int colGap = columnGap();
    const Color& ruleColor = style()->columnRuleColor();
    bool ruleTransparent = style()->columnRuleIsTransparent();
    EBorderStyle ruleStyle = style()->columnRuleStyle();
    int ruleWidth = style()->columnRuleWidth();
    bool renderRule = !paintingFloats && ruleStyle > BHIDDEN && !ruleTransparent && ruleWidth <= colGap;
    Vector<IntRect>* colRects = columnRects();
    unsigned colCount = colRects->size();
    for (unsigned i = 0; i < colCount; i++) {
        // For each rect, we clip to the rect, and then we adjust our coords.
        IntRect colRect = colRects->at(i);
        colRect.move(tx, ty);
        context->save();
        
        // Each strip pushes a clip, since column boxes are specified as being
        // like overflow:hidden.
        context->clip(colRect);
        
        // Adjust tx and ty to change where we paint.
        PaintInfo info(paintInfo);
        info.rect.intersect(colRect);
        
        // Adjust our x and y when painting.
        int finalX = tx + currXOffset;
        int finalY = ty + currYOffset;
        if (paintingFloats)
            paintFloats(info, finalX, finalY, paintInfo.phase == PaintPhaseSelection || paintInfo.phase == PaintPhaseTextClip);
        else
            paintContents(info, finalX, finalY);

        // Move to the next position.
        if (style()->direction() == LTR) {
            ruleX += colRect.width() + colGap / 2;
            currXOffset += colRect.width() + colGap;
        } else {
            ruleX -= (colRect.width() + colGap / 2);
            currXOffset -= (colRect.width() + colGap);
        }

        currYOffset -= colRect.height();
        
        context->restore();
        
        // Now paint the column rule.
        if (renderRule && paintInfo.phase == PaintPhaseForeground && i < colCount - 1) {
            int ruleStart = ruleX - ruleWidth / 2 + ruleAdd;
            int ruleEnd = ruleStart + ruleWidth;
            drawBorder(paintInfo.context, tx + ruleStart, ty + borderTop() + paddingTop(), tx + ruleEnd, ty + borderTop() + paddingTop() + contentHeight(),
                       style()->direction() == LTR ? BSLeft : BSRight, ruleColor, style()->color(), ruleStyle, 0, 0);
        }
        
        ruleX = currXOffset;
    }
}

void RenderBlock::paintContents(PaintInfo& paintInfo, int tx, int ty)
{
    // Avoid painting descendants of the root element when stylesheets haven't loaded.  This eliminates FOUC.
    // It's ok not to draw, because later on, when all the stylesheets do load, updateStyleSelector on the Document
    // will do a full repaint().
    if (document()->didLayoutWithPendingStylesheets() && !isRenderView())
        return;

    if (childrenInline())
        paintLines(paintInfo, tx, ty);
    else
        paintChildren(paintInfo, tx, ty);
}

void RenderBlock::paintChildren(PaintInfo& paintInfo, int tx, int ty)
{
    PaintPhase newPhase = (paintInfo.phase == PaintPhaseChildOutlines) ? PaintPhaseOutline : paintInfo.phase;
    newPhase = (newPhase == PaintPhaseChildBlockBackgrounds) ? PaintPhaseChildBlockBackground : newPhase;
    
    // We don't paint our own background, but we do let the kids paint their backgrounds.
    PaintInfo info(paintInfo);
    info.phase = newPhase;
    info.paintingRoot = paintingRootForChildren(paintInfo);
    bool isPrinting = document()->printing();

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {        
        // Check for page-break-before: always, and if it's set, break and bail.
        if (isPrinting && !childrenInline() && child->style()->pageBreakBefore() == PBALWAYS &&
            inRootBlockContext() && (ty + child->y()) > paintInfo.rect.y() && 
            (ty + child->y()) < paintInfo.rect.bottom()) {
            view()->setBestTruncatedAt(ty + child->y(), this, true);
            return;
        }

        if (!child->hasSelfPaintingLayer() && !child->isFloating())
            child->paint(info, tx, ty);

        // Check for page-break-after: always, and if it's set, break and bail.
        if (isPrinting && !childrenInline() && child->style()->pageBreakAfter() == PBALWAYS && 
            inRootBlockContext() && (ty + child->y() + child->height()) > paintInfo.rect.y() && 
            (ty + child->y() + child->height()) < paintInfo.rect.bottom()) {
            view()->setBestTruncatedAt(ty + child->y() + child->height() + max(0, child->collapsedMarginBottom()), this, true);
            return;
        }
    }
}

void RenderBlock::paintCaret(PaintInfo& paintInfo, int tx, int ty, CaretType type)
{
    SelectionController* selection = type == CursorCaret ? document()->frame()->selection() : document()->frame()->dragCaretController();

    // Ask the SelectionController if the caret should be painted by this block
    RenderObject* caretPainter = selection->caretRenderer();
    if (caretPainter == this && selection->isContentEditable()) {
        // Convert the painting offset into the local coordinate system of this renderer,
        // to match the localCaretRect computed by the SelectionController
        offsetForContents(tx, ty);

        if (type == CursorCaret)
            document()->frame()->paintCaret(paintInfo.context, tx, ty, paintInfo.rect);
        else
            document()->frame()->paintDragCaret(paintInfo.context, tx, ty, paintInfo.rect);
    }
}

void RenderBlock::paintObject(PaintInfo& paintInfo, int tx, int ty)
{
    PaintPhase paintPhase = paintInfo.phase;

    // 1. paint background, borders etc
    if ((paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) &&
        hasBoxDecorations() && style()->visibility() == VISIBLE) {
        paintBoxDecorations(paintInfo, tx, ty);
    }

    if (paintPhase == PaintPhaseMask && style()->visibility() == VISIBLE) {
        paintMask(paintInfo, tx, ty);
        return;
    }

    // We're done.  We don't bother painting any children.
    if (paintPhase == PaintPhaseBlockBackground)
        return;

    // Adjust our painting position if we're inside a scrolled layer (e.g., an overflow:auto div).s
    int scrolledX = tx;
    int scrolledY = ty;
    if (hasOverflowClip())
        m_layer->subtractScrolledContentOffset(scrolledX, scrolledY);

    // 2. paint contents
    if (paintPhase != PaintPhaseSelfOutline) {
        if (m_hasColumns)
            paintColumns(paintInfo, scrolledX, scrolledY);
        else
            paintContents(paintInfo, scrolledX, scrolledY);
    }

    // 3. paint selection
    // FIXME: Make this work with multi column layouts.  For now don't fill gaps.
    bool isPrinting = document()->printing();
    if (!isPrinting && !m_hasColumns)
        paintSelection(paintInfo, scrolledX, scrolledY); // Fill in gaps in selection on lines and between blocks.

    // 4. paint floats.
    if (paintPhase == PaintPhaseFloat || paintPhase == PaintPhaseSelection || paintPhase == PaintPhaseTextClip) {
        if (m_hasColumns)
            paintColumns(paintInfo, scrolledX, scrolledY, true);
        else
            paintFloats(paintInfo, scrolledX, scrolledY, paintPhase == PaintPhaseSelection || paintPhase == PaintPhaseTextClip);
    }

    // 5. paint outline.
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseSelfOutline) && hasOutline() && style()->visibility() == VISIBLE)
        RenderBox::paintOutline(paintInfo.context, tx, ty, width(), height(), style());

    // 6. paint continuation outlines.
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseChildOutlines)) {
        if (continuation() && continuation()->hasOutline() && continuation()->style()->visibility() == VISIBLE) {
            RenderFlow* inlineFlow = static_cast<RenderFlow*>(continuation()->element()->renderer());
            if (!inlineFlow->hasLayer())
                containingBlock()->addContinuationWithOutline(inlineFlow);
            else if (!inlineFlow->firstLineBox())
                inlineFlow->paintOutline(paintInfo.context, tx - x() + inlineFlow->containingBlock()->x(),
                                         ty - y() + inlineFlow->containingBlock()->y());
/*
        if (inlineContinuation() && inlineContinuation()->hasOutline() && inlineContinuation()->style()->visibility() == VISIBLE) {
            RenderInline* inlineRenderer = toRenderInline(inlineContinuation()->element()->renderer());
            if (!inlineRenderer->hasSelfPaintingLayer())
                containingBlock()->addContinuationWithOutline(inlineRenderer);
            else if (!inlineRenderer->firstLineBox())
                inlineRenderer->paintOutline(paintInfo.context, tx - x() + inlineRenderer->containingBlock()->x(),
                                             ty - y() + inlineRenderer->containingBlock()->y());
*/
        }
        paintContinuationOutlines(paintInfo, tx, ty);
    }

    // 7. paint caret.
    // If the caret's node's render object's containing block is this block, and the paint action is PaintPhaseForeground,
    // then paint the caret.
    if (paintPhase == PaintPhaseForeground) {        
        paintCaret(paintInfo, scrolledX, scrolledY, CursorCaret);
        paintCaret(paintInfo, scrolledX, scrolledY, DragCaret);
    }
}

void RenderBlock::paintFloats(PaintInfo& paintInfo, int tx, int ty, bool preservePhase)
{
    if (!m_floatingObjects)
        return;

    FloatingObject* r;
    DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for (; (r = it.current()); ++it) {
        // Only paint the object if our m_shouldPaint flag is set.
        if (r->m_shouldPaint && !r->m_renderer->hasSelfPaintingLayer()) {
            PaintInfo currentPaintInfo(paintInfo);
            currentPaintInfo.phase = preservePhase ? paintInfo.phase : PaintPhaseBlockBackground;
            int currentTX = tx + r->m_left - r->m_renderer->x() + r->m_renderer->marginLeft();
            int currentTY = ty + r->m_top - r->m_renderer->y() + r->m_renderer->marginTop();
            r->m_renderer->paint(currentPaintInfo, currentTX, currentTY);
            if (!preservePhase) {
                currentPaintInfo.phase = PaintPhaseChildBlockBackgrounds;
                r->m_renderer->paint(currentPaintInfo, currentTX, currentTY);
                currentPaintInfo.phase = PaintPhaseFloat;
                r->m_renderer->paint(currentPaintInfo, currentTX, currentTY);
                currentPaintInfo.phase = PaintPhaseForeground;
                r->m_renderer->paint(currentPaintInfo, currentTX, currentTY);
                currentPaintInfo.phase = PaintPhaseOutline;
                r->m_renderer->paint(currentPaintInfo, currentTX, currentTY);
            }
        }
    }
}

void RenderBlock::paintEllipsisBoxes(PaintInfo& paintInfo, int tx, int ty)
{
    if (!shouldPaintWithinRoot(paintInfo) || !firstLineBox())
        return;

    if (style()->visibility() == VISIBLE && paintInfo.phase == PaintPhaseForeground) {
        // We can check the first box and last box and avoid painting if we don't
        // intersect.
        int yPos = ty + firstLineBox()->yPos();
        int h = lastLineBox()->yPos() + lastLineBox()->height() - firstLineBox()->yPos();
        if (yPos >= paintInfo.rect.bottom() || yPos + h <= paintInfo.rect.y())
            return;

        // See if our boxes intersect with the dirty rect.  If so, then we paint
        // them.  Note that boxes can easily overlap, so we can't make any assumptions
        // based off positions of our first line box or our last line box.
        for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
            yPos = ty + curr->yPos();
            h = curr->height();
            if (curr->ellipsisBox() && yPos < paintInfo.rect.bottom() && yPos + h > paintInfo.rect.y())
                curr->paintEllipsisBox(paintInfo, tx, ty);
        }
    }
}

static ContinuationOutlineTableMap* continuationOutlineTable()
{
    DEFINE_STATIC_LOCAL(ContinuationOutlineTableMap, table, ());
    return &table;
}

void RenderBlock::addContinuationWithOutline(RenderFlow* flow)
{
    // We can't make this work if the inline is in a layer.  We'll just rely on the broken
    // way of painting.
    ASSERT(!flow->layer());
    
    ContinuationOutlineTableMap* table = continuationOutlineTable();
    ListHashSet<RenderFlow*>* continuations = table->get(this);
    if (!continuations) {
        continuations = new ListHashSet<RenderFlow*>;
        table->set(this, continuations);
    }
    
    continuations->add(flow);
}

void RenderBlock::paintContinuationOutlines(PaintInfo& info, int tx, int ty)
{
    ContinuationOutlineTableMap* table = continuationOutlineTable();
    if (table->isEmpty())
        return;
        
    ListHashSet<RenderFlow*>* continuations = table->get(this);
    if (!continuations)
        return;
        
    // Paint each continuation outline.
    ListHashSet<RenderFlow*>::iterator end = continuations->end();
    for (ListHashSet<RenderFlow*>::iterator it = continuations->begin(); it != end; ++it) {
        // Need to add in the coordinates of the intervening blocks.
        RenderFlow* flow = *it;
        RenderBlock* block = flow->containingBlock();
        for ( ; block && block != this; block = block->containingBlock()) {
            tx += block->x();
            ty += block->y();
        }
        ASSERT(block);   
        flow->paintOutline(info.context, tx, ty);
    }
    
    // Delete
    delete continuations;
    table->remove(this);
}

void RenderBlock::setSelectionState(SelectionState s)
{
    if (selectionState() == s)
        return;
    
    if (s == SelectionInside && selectionState() != SelectionNone)
        return;

    if ((s == SelectionStart && selectionState() == SelectionEnd) ||
        (s == SelectionEnd && selectionState() == SelectionStart))
        m_selectionState = SelectionBoth;
    else
        m_selectionState = s;
    
    RenderBlock* cb = containingBlock();
    if (cb && !cb->isRenderView())
        cb->setSelectionState(s);
}

bool RenderBlock::shouldPaintSelectionGaps() const
{
    return m_selectionState != SelectionNone && style()->visibility() == VISIBLE && isSelectionRoot();
}

bool RenderBlock::isSelectionRoot() const
{
    if (!element())
        return false;
        
    // FIXME: Eventually tables should have to learn how to fill gaps between cells, at least in simple non-spanning cases.
    if (isTable())
        return false;
        
    if (isBody() || isRoot() || hasOverflowClip() || isRelPositioned() ||
        isFloatingOrPositioned() || isTableCell() || isInlineBlockOrInlineTable() || hasTransform() ||
        hasReflection() || hasMask())
        return true;
    
    if (view() && view()->selectionStart()) {
        Node* startElement = view()->selectionStart()->element();
        if (startElement && startElement->rootEditableElement() == element())
            return true;
    }
    
    return false;
}

GapRects RenderBlock::selectionGapRectsForRepaint(RenderBox* /*repaintContainer*/)
{
    ASSERT(!needsLayout());

    if (!shouldPaintSelectionGaps())
        return GapRects();

    // FIXME: this is broken with transforms and a non-null repaintContainer
    FloatPoint absContentPoint = localToAbsolute(FloatPoint());
    if (hasOverflowClip())
        absContentPoint -= layer()->scrolledContentOffset();

    int lastTop = 0;
    int lastLeft = leftSelectionOffset(this, lastTop);
    int lastRight = rightSelectionOffset(this, lastTop);
    
    return fillSelectionGaps(this, absContentPoint.x(), absContentPoint.y(), absContentPoint.x(), absContentPoint.y(), lastTop, lastLeft, lastRight);
}

void RenderBlock::paintSelection(PaintInfo& paintInfo, int tx, int ty)
{
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(tx);
    UNUSED_PARAM(ty);
    // Never draw selection on iPhone, ever EVER!
    return;
}

static void clipOutPositionedObjects(const RenderObject::PaintInfo* paintInfo, int tx, int ty, ListHashSet<RenderBox*>* positionedObjects)
{
    if (!positionedObjects)
        return;
    
    ListHashSet<RenderBox*>::const_iterator end = positionedObjects->end();
    for (ListHashSet<RenderBox*>::const_iterator it = positionedObjects->begin(); it != end; ++it) {
        RenderBox* r = *it;
        paintInfo->context->clipOut(IntRect(tx + r->x(), ty + r->y(), r->width(), r->height()));
    }
}

GapRects RenderBlock::fillSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty,
                                        int& lastTop, int& lastLeft, int& lastRight, const PaintInfo* paintInfo)
{
    // IMPORTANT: Callers of this method that intend for painting to happen need to do a save/restore.
    // Clip out floating and positioned objects when painting selection gaps.
    if (paintInfo) {
        // Note that we don't clip out overflow for positioned objects.  We just stick to the border box.
        clipOutPositionedObjects(paintInfo, tx, ty, m_positionedObjects);
        if (isBody() || isRoot()) // The <body> must make sure to examine its containingBlock's positioned objects.
            for (RenderBlock* cb = containingBlock(); cb && !cb->isRenderView(); cb = cb->containingBlock())
                clipOutPositionedObjects(paintInfo, cb->x(), cb->y(), cb->m_positionedObjects);
        if (m_floatingObjects) {
            for (DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects); it.current(); ++it) {
                FloatingObject* r = it.current();
                paintInfo->context->clipOut(IntRect(tx + r->m_left + r->m_renderer->marginLeft(), 
                                                    ty + r->m_top + r->m_renderer->marginTop(),
                                                    r->m_renderer->width(), r->m_renderer->height()));
            }
        }
    }

    // FIXME: overflow: auto/scroll regions need more math here, since painting in the border box is different from painting in the padding box (one is scrolled, the other is
    // fixed).
    GapRects result;
    if (!isBlockFlow()) // FIXME: Make multi-column selection gap filling work someday.
        return result;

    if (m_hasColumns || hasTransform()) {
        // FIXME: We should learn how to gap fill multiple columns and transforms eventually.
        lastTop = (ty - blockY) + height();
        lastLeft = leftSelectionOffset(rootBlock, height());
        lastRight = rightSelectionOffset(rootBlock, height());
        return result;
    }

    if (childrenInline())
        result = fillInlineSelectionGaps(rootBlock, blockX, blockY, tx, ty, lastTop, lastLeft, lastRight, paintInfo);
    else
        result = fillBlockSelectionGaps(rootBlock, blockX, blockY, tx, ty, lastTop, lastLeft, lastRight, paintInfo);

    // Go ahead and fill the vertical gap all the way to the bottom of our block if the selection extends past our block.
    if (rootBlock == this && (m_selectionState != SelectionBoth && m_selectionState != SelectionEnd))
        result.uniteCenter(fillVerticalSelectionGap(lastTop, lastLeft, lastRight, ty + height(),
                                                    rootBlock, blockX, blockY, paintInfo));
    return result;
}

GapRects RenderBlock::fillInlineSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, 
                                              int& lastTop, int& lastLeft, int& lastRight, const PaintInfo* paintInfo)
{
    GapRects result;

    bool containsStart = selectionState() == SelectionStart || selectionState() == SelectionBoth;

    if (!firstLineBox()) {
        if (containsStart) {
            // Go ahead and update our lastY to be the bottom of the block.  <hr>s or empty blocks with height can trip this
            // case.
            lastTop = (ty - blockY) + height();
            lastLeft = leftSelectionOffset(rootBlock, height());
            lastRight = rightSelectionOffset(rootBlock, height());
        }
        return result;
    }

    RootInlineBox* lastSelectedLine = 0;
    RootInlineBox* curr;
    for (curr = firstRootBox(); curr && !curr->hasSelectedChildren(); curr = curr->nextRootBox()) { }

    // Now paint the gaps for the lines.
    for (; curr && curr->hasSelectedChildren(); curr = curr->nextRootBox()) {
        int selTop =  curr->selectionTop();
        int selHeight = curr->selectionHeight();

        if (!containsStart && !lastSelectedLine &&
            selectionState() != SelectionStart && selectionState() != SelectionBoth)
            result.uniteCenter(fillVerticalSelectionGap(lastTop, lastLeft, lastRight, ty + selTop,
                                                        rootBlock, blockX, blockY, paintInfo));

        if (!paintInfo || ty + selTop < paintInfo->rect.bottom() && ty + selTop + selHeight > paintInfo->rect.y())
            result.unite(curr->fillLineSelectionGap(selTop, selHeight, rootBlock, blockX, blockY, tx, ty, paintInfo));

        lastSelectedLine = curr;
    }

    if (containsStart && !lastSelectedLine)
        // Selection must start just after our last line.
        lastSelectedLine = lastRootBox();

    if (lastSelectedLine && selectionState() != SelectionEnd && selectionState() != SelectionBoth) {
        // Go ahead and update our lastY to be the bottom of the last selected line.
        lastTop = (ty - blockY) + lastSelectedLine->bottomOverflow();
        lastLeft = leftSelectionOffset(rootBlock, lastSelectedLine->bottomOverflow());
        lastRight = rightSelectionOffset(rootBlock, lastSelectedLine->bottomOverflow());
    }
    return result;
}

GapRects RenderBlock::fillBlockSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty,
                                             int& lastTop, int& lastLeft, int& lastRight, const PaintInfo* paintInfo)
{
    GapRects result;

    // Go ahead and jump right to the first block child that contains some selected objects.
    RenderBox* curr;
    for (curr = firstChildBox(); curr && curr->selectionState() == SelectionNone; curr = curr->nextSiblingBox()) { }

    for (bool sawSelectionEnd = false; curr && !sawSelectionEnd; curr = curr->nextSiblingBox()) {
        SelectionState childState = curr->selectionState();
        if (childState == SelectionBoth || childState == SelectionEnd)
            sawSelectionEnd = true;

        if (curr->isFloatingOrPositioned())
            continue; // We must be a normal flow object in order to even be considered.

        if (curr->isRelPositioned() && curr->hasLayer()) {
            // If the relposition offset is anything other than 0, then treat this just like an absolute positioned element.
            // Just disregard it completely.
            IntSize relOffset = curr->layer()->relativePositionOffset();
            if (relOffset.width() || relOffset.height())
                continue;
        }

        bool paintsOwnSelection = curr->shouldPaintSelectionGaps() || curr->isTable(); // FIXME: Eventually we won't special-case table like this.
        bool fillBlockGaps = paintsOwnSelection || (curr->canBeSelectionLeaf() && childState != SelectionNone);
        if (fillBlockGaps) {
            // We need to fill the vertical gap above this object.
            if (childState == SelectionEnd || childState == SelectionInside)
                // Fill the gap above the object.
                result.uniteCenter(fillVerticalSelectionGap(lastTop, lastLeft, lastRight, 
                                                            ty + curr->y(), rootBlock, blockX, blockY, paintInfo));

            // Only fill side gaps for objects that paint their own selection if we know for sure the selection is going to extend all the way *past*
            // our object.  We know this if the selection did not end inside our object.
            if (paintsOwnSelection && (childState == SelectionStart || sawSelectionEnd))
                childState = SelectionNone;

            // Fill side gaps on this object based off its state.
            bool leftGap, rightGap;
            getHorizontalSelectionGapInfo(childState, leftGap, rightGap);

            if (leftGap)
                result.uniteLeft(fillLeftSelectionGap(this, curr->x(), curr->y(), curr->height(), rootBlock, blockX, blockY, tx, ty, paintInfo));
            if (rightGap)
                result.uniteRight(fillRightSelectionGap(this, curr->x() + curr->width(), curr->y(), curr->height(), rootBlock, blockX, blockY, tx, ty, paintInfo));

            // Update lastTop to be just underneath the object.  lastLeft and lastRight extend as far as
            // they can without bumping into floating or positioned objects.  Ideally they will go right up
            // to the border of the root selection block.
            lastTop = (ty - blockY) + (curr->y() + curr->height());
            lastLeft = leftSelectionOffset(rootBlock, curr->y() + curr->height());
            lastRight = rightSelectionOffset(rootBlock, curr->y() + curr->height());
        } else if (childState != SelectionNone)
            // We must be a block that has some selected object inside it.  Go ahead and recur.
            result.unite(static_cast<RenderBlock*>(curr)->fillSelectionGaps(rootBlock, blockX, blockY, tx + curr->x(), ty + curr->y(), 
                                                                            lastTop, lastLeft, lastRight, paintInfo));
    }
    return result;
}

IntRect RenderBlock::fillHorizontalSelectionGap(RenderObject* selObj, int xPos, int yPos, int actualWidth, int height, const PaintInfo* paintInfo)
{
    if (actualWidth <= 0 || height <= 0)
        return IntRect();
    IntRect gapRect(xPos, yPos, actualWidth, height);
    if (paintInfo && selObj->style()->visibility() == VISIBLE)
        paintInfo->context->fillRect(gapRect, selObj->selectionBackgroundColor());
    return gapRect;
}

IntRect RenderBlock::fillVerticalSelectionGap(int lastTop, int lastLeft, int lastRight, int bottomY, RenderBlock* rootBlock,
                                              int blockX, int blockY, const PaintInfo* paintInfo)
{
    int top = blockY + lastTop;
    int height = bottomY - top;
    if (height <= 0)
        return IntRect();

    // Get the selection offsets for the bottom of the gap
    int left = blockX + max(lastLeft, leftSelectionOffset(rootBlock, bottomY));
    int right = blockX + min(lastRight, rightSelectionOffset(rootBlock, bottomY));
    int actualWidth = right - left;
    if (actualWidth <= 0)
        return IntRect();

    IntRect gapRect(left, top, actualWidth, height);
    if (paintInfo)
        paintInfo->context->fillRect(gapRect, selectionBackgroundColor());
    return gapRect;
}

IntRect RenderBlock::fillLeftSelectionGap(RenderObject* selObj, int xPos, int yPos, int height, RenderBlock* rootBlock,
                                          int blockX, int /*blockY*/, int tx, int ty, const PaintInfo* paintInfo)
{
    int top = yPos + ty;
    int left = blockX + max(leftSelectionOffset(rootBlock, yPos), leftSelectionOffset(rootBlock, yPos + height));
    int right = min(xPos + tx, blockX + min(rightSelectionOffset(rootBlock, yPos), rightSelectionOffset(rootBlock, yPos + height)));
    int actualWidth = right - left;
    if (actualWidth <= 0)
        return IntRect();

    IntRect gapRect(left, top, actualWidth, height);
    if (paintInfo)
        paintInfo->context->fillRect(gapRect, selObj->selectionBackgroundColor());
    return gapRect;
}

IntRect RenderBlock::fillRightSelectionGap(RenderObject* selObj, int xPos, int yPos, int height, RenderBlock* rootBlock,
                                           int blockX, int /*blockY*/, int tx, int ty, const PaintInfo* paintInfo)
{
    int left = max(xPos + tx, blockX + max(leftSelectionOffset(rootBlock, yPos), leftSelectionOffset(rootBlock, yPos + height)));
    int top = yPos + ty;
    int right = blockX + min(rightSelectionOffset(rootBlock, yPos), rightSelectionOffset(rootBlock, yPos + height));
    int actualWidth = right - left;
    if (actualWidth <= 0)
        return IntRect();

    IntRect gapRect(left, top, actualWidth, height);
    if (paintInfo)
        paintInfo->context->fillRect(gapRect, selObj->selectionBackgroundColor());
    return gapRect;
}

void RenderBlock::getHorizontalSelectionGapInfo(SelectionState state, bool& leftGap, bool& rightGap)
{
    bool ltr = style()->direction() == LTR;
    leftGap = (state == RenderObject::SelectionInside) ||
              (state == RenderObject::SelectionEnd && ltr) ||
              (state == RenderObject::SelectionStart && !ltr);
    rightGap = (state == RenderObject::SelectionInside) ||
               (state == RenderObject::SelectionStart && ltr) ||
               (state == RenderObject::SelectionEnd && !ltr);
}

int RenderBlock::leftSelectionOffset(RenderBlock* rootBlock, int yPos)
{
    int left = leftOffset(yPos);
    if (left == borderLeft() + paddingLeft()) {
        if (rootBlock != this)
            // The border can potentially be further extended by our containingBlock().
            return containingBlock()->leftSelectionOffset(rootBlock, yPos + y());
        return left;
    }
    else {
        RenderBlock* cb = this;
        while (cb != rootBlock) {
            left += cb->x();
            cb = cb->containingBlock();
        }
    }
    
    return left;
}

int RenderBlock::rightSelectionOffset(RenderBlock* rootBlock, int yPos)
{
    int right = rightOffset(yPos);
    if (right == (contentWidth() + (borderLeft() + paddingLeft()))) {
        if (rootBlock != this)
            // The border can potentially be further extended by our containingBlock().
            return containingBlock()->rightSelectionOffset(rootBlock, yPos + y());
        return right;
    }
    else {
        RenderBlock* cb = this;
        while (cb != rootBlock) {
            right += cb->x();
            cb = cb->containingBlock();
        }
    }
    return right;
}

void RenderBlock::insertPositionedObject(RenderBox* o)
{
    // Create the list of special objects if we don't aleady have one
    if (!m_positionedObjects)
        m_positionedObjects = new ListHashSet<RenderBox*>;

    m_positionedObjects->add(o);
}

void RenderBlock::removePositionedObject(RenderBox* o)
{
    if (m_positionedObjects)
        m_positionedObjects->remove(o);
}

void RenderBlock::removePositionedObjects(RenderBlock* o)
{
    if (!m_positionedObjects)
        return;
    
    RenderBox* r;
    
    Iterator end = m_positionedObjects->end();
    
    Vector<RenderBox*, 16> deadObjects;

    for (Iterator it = m_positionedObjects->begin(); it != end; ++it) {
        r = *it;
        if (!o || r->isDescendantOf(o)) {
            if (o)
                r->setChildNeedsLayout(true, false);
            
            // It is parent blocks job to add positioned child to positioned objects list of its containing block
            // Parent layout needs to be invalidated to ensure this happens.
            RenderObject* p = r->parent();
            while (p && !p->isRenderBlock())
                p = p->parent();
            if (p)
                p->setChildNeedsLayout(true);
            
            deadObjects.append(r);
        }
    }
    
    for (unsigned i = 0; i < deadObjects.size(); i++)
        m_positionedObjects->remove(deadObjects.at(i));
}

void RenderBlock::insertFloatingObject(RenderBox* o)
{
    ASSERT(o->isFloating());

    // Create the list of special objects if we don't aleady have one
    if (!m_floatingObjects) {
        m_floatingObjects = new DeprecatedPtrList<FloatingObject>;
        m_floatingObjects->setAutoDelete(true);
    } else {
        // Don't insert the object again if it's already in the list
        DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
        FloatingObject* f;
        while ( (f = it.current()) ) {
            if (f->m_renderer == o) return;
            ++it;
        }
    }

    // Create the special object entry & append it to the list

    o->layoutIfNeeded();

    FloatingObject* newObj = new FloatingObject(o->style()->floating() == FLEFT ? FloatingObject::FloatLeft : FloatingObject::FloatRight);

    newObj->m_top = -1;
    newObj->m_bottom = -1;
    newObj->m_width = o->width() + o->marginLeft() + o->marginRight();
    newObj->m_shouldPaint = !o->hasSelfPaintingLayer(); // If a layer exists, the float will paint itself.  Otherwise someone else will.
    newObj->m_isDescendant = true;
    newObj->m_renderer = o;

    m_floatingObjects->append(newObj);
}

void RenderBlock::removeFloatingObject(RenderBox* o)
{
    if (m_floatingObjects) {
        DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
        while (it.current()) {
            if (it.current()->m_renderer == o) {
                if (childrenInline())
                    markLinesDirtyInVerticalRange(0, it.current()->m_bottom);
                m_floatingObjects->removeRef(it.current());
            }
            ++it;
        }
    }
}

bool RenderBlock::positionNewFloats()
{
    if (!m_floatingObjects)
        return false;
    
    FloatingObject* f = m_floatingObjects->last();

    // If all floats have already been positioned, then we have no work to do.
    if (!f || f->m_top != -1)
        return false;

    // Move backwards through our floating object list until we find a float that has
    // already been positioned.  Then we'll be able to move forward, positioning all of
    // the new floats that need it.
    FloatingObject* lastFloat = m_floatingObjects->getPrev();
    while (lastFloat && lastFloat->m_top == -1) {
        f = m_floatingObjects->prev();
        lastFloat = m_floatingObjects->getPrev();
    }

    int y = height();
    
    // The float cannot start above the y position of the last positioned float.
    if (lastFloat)
        y = max(lastFloat->m_top, y);

    // Now walk through the set of unpositioned floats and place them.
    while (f) {
        // The containing block is responsible for positioning floats, so if we have floats in our
        // list that come from somewhere else, do not attempt to position them.
        if (f->m_renderer->containingBlock() != this) {
            f = m_floatingObjects->next();
            continue;
        }

        RenderBox* o = f->m_renderer;
        int _height = o->height() + o->marginTop() + o->marginBottom();

        int ro = rightOffset(); // Constant part of right offset.
        int lo = leftOffset(); // Constat part of left offset.
        int fwidth = f->m_width; // The width we look for.
        if (ro - lo < fwidth)
            fwidth = ro - lo; // Never look for more than what will be available.
        
        IntRect oldRect(o->x(), o->y() , o->width(), o->height());
        
        if (o->style()->clear() & CLEFT)
            y = max(leftBottom(), y);
        if (o->style()->clear() & CRIGHT)
            y = max(rightBottom(), y);

        if (o->style()->floating() == FLEFT) {
            int heightRemainingLeft = 1;
            int heightRemainingRight = 1;
            int fx = leftRelOffset(y,lo, false, &heightRemainingLeft);
            while (rightRelOffset(y,ro, false, &heightRemainingRight)-fx < fwidth) {
                y += min(heightRemainingLeft, heightRemainingRight);
                fx = leftRelOffset(y,lo, false, &heightRemainingLeft);
            }
            fx = max(0, fx);
            f->m_left = fx;
            o->setLocation(fx + o->marginLeft(), y + o->marginTop());
        } else {
            int heightRemainingLeft = 1;
            int heightRemainingRight = 1;
            int fx = rightRelOffset(y,ro, false, &heightRemainingRight);
            while (fx - leftRelOffset(y,lo, false, &heightRemainingLeft) < fwidth) {
                y += min(heightRemainingLeft, heightRemainingRight);
                fx = rightRelOffset(y, ro, false, &heightRemainingRight);
            }
            f->m_left = fx - f->m_width;
            o->setLocation(fx - o->marginRight() - o->width(), y + o->marginTop());
        }

        f->m_top = y;
        f->m_bottom = f->m_top + _height;

        // If the child moved, we have to repaint it.
        if (o->checkForRepaintDuringLayout())
            o->repaintDuringLayoutIfMoved(oldRect);

        f = m_floatingObjects->next();
    }
    return true;
}

void RenderBlock::newLine(EClear clear)
{
    positionNewFloats();
    // set y position
    int newY = 0;
    switch(clear)
    {
        case CLEFT:
            newY = leftBottom();
            break;
        case CRIGHT:
            newY = rightBottom();
            break;
        case CBOTH:
            newY = floatBottom();
        default:
            break;
    }
    if (height() < newY)
        setHeight(newY);
}

void RenderBlock::addPercentHeightDescendant(RenderBox* descendant)
{
    if (!gPercentHeightDescendantsMap) {
        gPercentHeightDescendantsMap = new PercentHeightDescendantsMap;
        gPercentHeightContainerMap = new PercentHeightContainerMap;
    }

    HashSet<RenderBox*>* descendantSet = gPercentHeightDescendantsMap->get(this);
    if (!descendantSet) {
        descendantSet = new HashSet<RenderBox*>;
        gPercentHeightDescendantsMap->set(this, descendantSet);
    }
    bool added = descendantSet->add(descendant).second;
    if (!added) {
        ASSERT(gPercentHeightContainerMap->get(descendant));
        ASSERT(gPercentHeightContainerMap->get(descendant)->contains(this));
        return;
    }

    HashSet<RenderBlock*>* containerSet = gPercentHeightContainerMap->get(descendant);
    if (!containerSet) {
        containerSet = new HashSet<RenderBlock*>;
        gPercentHeightContainerMap->set(descendant, containerSet);
    }
    ASSERT(!containerSet->contains(this));
    containerSet->add(this);
}

void RenderBlock::removePercentHeightDescendant(RenderBox* descendant)
{
    if (!gPercentHeightContainerMap)
        return;

    HashSet<RenderBlock*>* containerSet = gPercentHeightContainerMap->take(descendant);
    if (!containerSet)
        return;

    HashSet<RenderBlock*>::iterator end = containerSet->end();
    for (HashSet<RenderBlock*>::iterator it = containerSet->begin(); it != end; ++it) {
        RenderBlock* container = *it;
        HashSet<RenderBox*>* descendantSet = gPercentHeightDescendantsMap->get(container);
        ASSERT(descendantSet);
        if (!descendantSet)
            continue;
        ASSERT(descendantSet->contains(descendant));
        descendantSet->remove(descendant);
        if (descendantSet->isEmpty()) {
            gPercentHeightDescendantsMap->remove(container);
            delete descendantSet;
        }
    }

    delete containerSet;
}

int
RenderBlock::leftOffset() const
{
    return borderLeft()+paddingLeft();
}

int
RenderBlock::leftRelOffset(int y, int fixedOffset, bool applyTextIndent,
                           int *heightRemaining ) const
{
    int left = fixedOffset;
    if (m_floatingObjects) {
        if ( heightRemaining ) *heightRemaining = 1;
        FloatingObject* r;
        DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it )
        {
            if (r->m_top <= y && r->m_bottom > y &&
                r->type() == FloatingObject::FloatLeft &&
                r->m_left + r->m_width > left) {
                left = r->m_left + r->m_width;
                if ( heightRemaining ) *heightRemaining = r->m_bottom - y;
            }
        }
    }

    if (applyTextIndent && m_firstLine && style()->direction() == LTR) {
        int cw = 0;
        if (style()->textIndent().isPercent())
            cw = containingBlock()->availableWidth();
        left += style()->textIndent().calcMinValue(cw);
    }

    return left;
}

int
RenderBlock::rightOffset() const
{
    return borderLeft() + paddingLeft() + availableWidth();
}

int
RenderBlock::rightRelOffset(int y, int fixedOffset, bool applyTextIndent,
                            int *heightRemaining ) const
{
    int right = fixedOffset;

    if (m_floatingObjects) {
        if (heightRemaining) *heightRemaining = 1;
        FloatingObject* r;
        DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it )
        {
            if (r->m_top <= y && r->m_bottom > y &&
                r->type() == FloatingObject::FloatRight &&
                r->m_left < right) {
                right = r->m_left;
                if ( heightRemaining ) *heightRemaining = r->m_bottom - y;
            }
        }
    }
    
    if (applyTextIndent && m_firstLine && style()->direction() == RTL) {
        int cw = 0;
        if (style()->textIndent().isPercent())
            cw = containingBlock()->availableWidth();
        right -= style()->textIndent().calcMinValue(cw);
    }
    
    return right;
}

int
RenderBlock::lineWidth(int y) const
{
    int result = rightOffset(y) - leftOffset(y);
    return (result < 0) ? 0 : result;
}

int RenderBlock::nextFloatBottomBelow(int height) const
{
    if (!m_floatingObjects)
        return 0;

    int bottom = INT_MAX;
    FloatingObject* r;
    DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for ( ; (r = it.current()); ++it) {
        if (r->m_bottom > height)
            bottom = min(r->m_bottom, bottom);
    }

    return bottom == INT_MAX ? 0 : bottom;
}

int
RenderBlock::floatBottom() const
{
    if (!m_floatingObjects) return 0;
    int bottom=0;
    FloatingObject* r;
    DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for ( ; (r = it.current()); ++it )
        if (r->m_bottom>bottom)
            bottom=r->m_bottom;
    return bottom;
}

IntRect RenderBlock::floatRect() const
{
    IntRect result;
    if (!m_floatingObjects || hasOverflowClip())
        return result;
    FloatingObject* r;
    DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for (; (r = it.current()); ++it) {
        if (r->m_shouldPaint && !r->m_renderer->hasSelfPaintingLayer()) {
            IntRect childRect = r->m_renderer->overflowRect(false);
            childRect.move(r->m_left + r->m_renderer->marginLeft(), r->m_top + r->m_renderer->marginTop());
            result.unite(childRect);
        }
    }

    return result;
}

int RenderBlock::lowestPosition(bool includeOverflowInterior, bool includeSelf) const
{
    if (!includeOverflowInterior && (hasOverflowClip() || hasControlClip()))
        return includeSelf && width() > 0 ? overflowHeight(false) : 0;
    
    int bottom = includeSelf && width() > 0 ? height() : 0;
    if (!hasColumns()) {
        // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
        // For now, we have to descend into all the children, since we may have a huge abs div inside
        // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
        // the abs div.
        for (RenderObject* c = firstChild(); c; c = c->nextSibling()) {
            if (!c->isFloatingOrPositioned() && !c->isText() && !c->isRenderInline())
                bottom = max(bottom, toRenderBox(c)->y() + c->lowestPosition(false));
        }
    }

    if (includeSelf && isRelPositioned())
        bottom += relativePositionOffsetY();     
    if (!includeOverflowInterior && hasOverflowClip())
        return bottom;

    int relativeOffset = includeSelf && isRelPositioned() ? relativePositionOffsetY() : 0;

    if (includeSelf)
        bottom = max(bottom, m_overflowHeight + relativeOffset);
        
    if (m_positionedObjects) {
        RenderBox* r;
        Iterator end = m_positionedObjects->end();
        for (Iterator it = m_positionedObjects->begin(); it != end; ++it) {
            r = *it;
            // Fixed positioned objects do not scroll and thus should not constitute
            // part of the lowest position.
            if (r->style()->position() != FixedPosition) {
                // FIXME: Should work for overflow sections too.
                // If a positioned object lies completely to the left of the root it will be unreachable via scrolling.
                // Therefore we should not allow it to contribute to the lowest position.
                if (!isRenderView() || r->x() + r->width() > 0 || r->x() + r->rightmostPosition(false) > 0) {
                    int lp = r->y() + r->lowestPosition(false);
                    bottom = max(bottom, lp + relativeOffset);
                }
            }
        }
    }

    if (m_hasColumns) {
        Vector<IntRect>* colRects = columnRects();
        for (unsigned i = 0; i < colRects->size(); i++)
            bottom = max(bottom, colRects->at(i).bottom() + relativeOffset);
        return bottom;
    }

    if (m_floatingObjects) {
        FloatingObject* r;
        DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it ) {
            if (r->m_shouldPaint || r->m_renderer->hasSelfPaintingLayer()) {
                int lp = r->m_top + r->m_renderer->marginTop() + r->m_renderer->lowestPosition(false);
                bottom = max(bottom, lp + relativeOffset);
            }
        }
    }

    if (!includeSelf && lastLineBox()) {
        int lp = lastLineBox()->yPos() + lastLineBox()->height();
        bottom = max(bottom, lp);
    }
    
    return bottom;
}

int RenderBlock::rightmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    if (!includeOverflowInterior && (hasOverflowClip() || hasControlClip()))
        return includeSelf && height() > 0 ? overflowWidth(false) : 0;

    int right = includeSelf && height() > 0 ? width() : 0;

    if (!hasColumns()) {
        // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
        // For now, we have to descend into all the children, since we may have a huge abs div inside
        // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
        // the abs div.
        for (RenderObject* c = firstChild(); c; c = c->nextSibling()) {
            if (!c->isFloatingOrPositioned() && c->isBox() && !c->isRenderInline())
                right = max(right, toRenderBox(c)->x() + c->rightmostPosition(false));
        }
    }

    if (includeSelf && isRelPositioned())
        right += relativePositionOffsetX();

    if (!includeOverflowInterior && hasOverflowClip())
        return right;

    int relativeOffset = includeSelf && isRelPositioned() ? relativePositionOffsetX() : 0;

    if (includeSelf)
        right = max(right, m_overflowWidth + relativeOffset);

    if (m_positionedObjects) {
        RenderBox* r;
        Iterator end = m_positionedObjects->end();
        for (Iterator it = m_positionedObjects->begin() ; it != end; ++it) {
            r = *it;
            // Fixed positioned objects do not scroll and thus should not constitute
            // part of the rightmost position.
            if (r->style()->position() != FixedPosition) {
                // FIXME: Should work for overflow sections too.
                // If a positioned object lies completely above the root it will be unreachable via scrolling.
                // Therefore we should not allow it to contribute to the rightmost position.
                if (!isRenderView() || r->y() + r->height() > 0 || r->y() + r->lowestPosition(false) > 0) {
                    int rp = r->x() + r->rightmostPosition(false);
                    right = max(right, rp + relativeOffset);
                }
            }
        }
    }

    if (m_hasColumns) {
        // This only matters for LTR
        if (style()->direction() == LTR)
            right = max(columnRects()->last().right() + relativeOffset, right);
        return right;
    }

    if (m_floatingObjects) {
        FloatingObject* r;
        DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it ) {
            if (r->m_shouldPaint || r->m_renderer->hasSelfPaintingLayer()) {
                int rp = r->m_left + r->m_renderer->marginLeft() + r->m_renderer->rightmostPosition(false);
                right = max(right, rp + relativeOffset);
            }
        }
    }

    if (!includeSelf && firstLineBox()) {
        for (InlineRunBox* currBox = firstLineBox(); currBox; currBox = currBox->nextLineBox()) {
            int rp = currBox->xPos() + currBox->width();
            // If this node is a root editable element, then the rightmostPosition should account for a caret at the end.
            // FIXME: Need to find another way to do this, since scrollbars could show when we don't want them to.
            if (node()->isContentEditable() && node() == node()->rootEditableElement() && style()->direction() == LTR)
                rp += 1;
            right = max(right, rp);
        }
    }
    
    return right;
}

int RenderBlock::leftmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    if (!includeOverflowInterior && (hasOverflowClip() || hasControlClip()))
        return includeSelf && height() > 0 ? overflowLeft(false) : width();

    int left = includeSelf && height() > 0 ? 0 : width();
    if (!hasColumns()) {
        // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
        // For now, we have to descend into all the children, since we may have a huge abs div inside
        // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
        // the abs div.
        for (RenderObject* c = firstChild(); c; c = c->nextSibling()) {
            if (!c->isFloatingOrPositioned() && c->isBox() && !c->isRenderInline())
                left = min(left, toRenderBox(c)->x() + c->leftmostPosition(false));
        }
    }

    if (includeSelf && isRelPositioned())
        left += relativePositionOffsetX(); 

    if (!includeOverflowInterior && hasOverflowClip())
        return left;
    
    int relativeOffset = includeSelf && isRelPositioned() ? relativePositionOffsetX() : 0;

    if (includeSelf)
        left = min(left, m_overflowLeft + relativeOffset);

    if (m_positionedObjects) {
        RenderBox* r;
        Iterator end = m_positionedObjects->end();
        for (Iterator it = m_positionedObjects->begin(); it != end; ++it) {
            r = *it;
            // Fixed positioned objects do not scroll and thus should not constitute
            // part of the leftmost position.
            if (r->style()->position() != FixedPosition) {
                // FIXME: Should work for overflow sections too.
                // If a positioned object lies completely above the root it will be unreachable via scrolling.
                // Therefore we should not allow it to contribute to the leftmost position.
                if (!isRenderView() || r->y() + r->height() > 0 || r->y() + r->lowestPosition(false) > 0) {
                    int lp = r->x() + r->leftmostPosition(false);
                    left = min(left, lp + relativeOffset);
                }
            }
        }
    }

    if (m_hasColumns) {
        // This only matters for RTL
        if (style()->direction() == RTL)
            left = min(columnRects()->last().x() + relativeOffset, left);
        return left;
    }

    if (m_floatingObjects) {
        FloatingObject* r;
        DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it ) {
            if (r->m_shouldPaint || r->m_renderer->hasSelfPaintingLayer()) {
                int lp = r->m_left + r->m_renderer->marginLeft() + r->m_renderer->leftmostPosition(false);
                left = min(left, lp + relativeOffset);
            }
        }
    }

    if (!includeSelf && firstLineBox()) {
        for (InlineRunBox* currBox = firstLineBox(); currBox; currBox = currBox->nextLineBox())
            left = min(left, (int)currBox->xPos());
    }
    
    return left;
}

int
RenderBlock::leftBottom()
{
    if (!m_floatingObjects) return 0;
    int bottom=0;
    FloatingObject* r;
    DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for ( ; (r = it.current()); ++it )
        if (r->m_bottom > bottom && r->type() == FloatingObject::FloatLeft)
            bottom=r->m_bottom;

    return bottom;
}

int
RenderBlock::rightBottom()
{
    if (!m_floatingObjects) return 0;
    int bottom=0;
    FloatingObject* r;
    DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for ( ; (r = it.current()); ++it )
        if (r->m_bottom>bottom && r->type() == FloatingObject::FloatRight)
            bottom=r->m_bottom;

    return bottom;
}

void RenderBlock::markLinesDirtyInVerticalRange(int top, int bottom)
{
    if (top >= bottom)
        return;

    RootInlineBox* lowestDirtyLine = lastRootBox();
    RootInlineBox* afterLowest = lowestDirtyLine;
    while (lowestDirtyLine && lowestDirtyLine->blockHeight() >= bottom) {
        afterLowest = lowestDirtyLine;
        lowestDirtyLine = lowestDirtyLine->prevRootBox();
    }

    while (afterLowest && afterLowest->blockHeight() >= top) {
        afterLowest->markDirty();
        afterLowest = afterLowest->prevRootBox();
    }
}

void RenderBlock::clearFloats()
{
    // Inline blocks are covered by the isReplaced() check in the avoidFloats method.
    if (avoidsFloats() || isRoot() || isRenderView() || isFloatingOrPositioned() || isTableCell()) {
        if (m_floatingObjects)
            m_floatingObjects->clear();
        return;
    }

    typedef HashMap<RenderObject*, FloatingObject*> RendererToFloatInfoMap;
    RendererToFloatInfoMap floatMap;

    if (m_floatingObjects) {
        if (childrenInline()) {
            m_floatingObjects->first();
            while (FloatingObject* f = m_floatingObjects->take())
                floatMap.add(f->m_renderer, f);
        } else
            m_floatingObjects->clear();
    }

    // Attempt to locate a previous sibling with overhanging floats.  We skip any elements that are
    // out of flow (like floating/positioned elements), and we also skip over any objects that may have shifted
    // to avoid floats.
    bool parentHasFloats = false;
    RenderObject* prev = previousSibling();
    while (prev && (!prev->isBox() || !prev->isRenderBlock() || prev->avoidsFloats() || prev->isFloatingOrPositioned())) {
        if (prev->isFloating())
            parentHasFloats = true;
         prev = prev->previousSibling();
    }

    // First add in floats from the parent.
    int offset = y();
    if (parentHasFloats) {
        RenderBlock* parentBlock = static_cast<RenderBlock *>(parent());
        addIntrudingFloats(parentBlock, parentBlock->borderLeft() + parentBlock->paddingLeft(), offset);
    }
    
    int xoffset = 0;
    if (prev)
        offset -= toRenderBox(prev)->y();
    else if (parent()->isBox()) {
        prev = parent();
        xoffset += toRenderBox(prev)->borderLeft() + toRenderBox(prev)->paddingLeft();
    }

    // Add overhanging floats from the previous RenderBlock, but only if it has a float that intrudes into our space.
    if (!prev || !prev->isRenderBlock())
        return;
    
    RenderBlock* block = static_cast<RenderBlock *>(prev);
    if (block->m_floatingObjects && block->floatBottom() > offset)
        addIntrudingFloats(block, xoffset, offset);

    if (childrenInline()) {
        int changeTop = INT_MAX;
        int changeBottom = INT_MIN;
        if (m_floatingObjects) {
            for (FloatingObject* f = m_floatingObjects->first(); f; f = m_floatingObjects->next()) {
                FloatingObject* oldFloatingObject = floatMap.get(f->m_renderer);
                if (oldFloatingObject) {
                    if (f->m_width != oldFloatingObject->m_width || f->m_left != oldFloatingObject->m_left) {
                        changeTop = 0;
                        changeBottom = max(changeBottom, max(f->m_bottom, oldFloatingObject->m_bottom));
                    } else if (f->m_bottom != oldFloatingObject->m_bottom) {
                        changeTop = min(changeTop, min(f->m_bottom, oldFloatingObject->m_bottom));
                        changeBottom = max(changeBottom, max(f->m_bottom, oldFloatingObject->m_bottom));
                    }

                    floatMap.remove(f->m_renderer);
                    delete oldFloatingObject;
                } else {
                    changeTop = 0;
                    changeBottom = max(changeBottom, f->m_bottom);
                }
            }
        }

        RendererToFloatInfoMap::iterator end = floatMap.end();
        for (RendererToFloatInfoMap::iterator it = floatMap.begin(); it != end; ++it) {
            FloatingObject* floatingObject = (*it).second;
            if (!floatingObject->m_isDescendant) {
                changeTop = 0;
                changeBottom = max(changeBottom, floatingObject->m_bottom);
            }
        }
        deleteAllValues(floatMap);

        markLinesDirtyInVerticalRange(changeTop, changeBottom);
    }
}

int RenderBlock::addOverhangingFloats(RenderBlock* child, int xoff, int yoff, bool makeChildPaintOtherFloats)
{
    // Prevent floats from being added to the canvas by the root element, e.g., <html>.
    if (child->hasOverflowClip() || !child->containsFloats() || child->isRoot())
        return 0;

    int lowestFloatBottom = 0;

    // Floats that will remain the child's responsiblity to paint should factor into its
    // visual overflow.
    IntRect floatsOverflowRect;
    DeprecatedPtrListIterator<FloatingObject> it(*child->m_floatingObjects);
    for (FloatingObject* r; (r = it.current()); ++it) {
        int bottom = child->y() + r->m_bottom;
        lowestFloatBottom = max(lowestFloatBottom, bottom);

        if (bottom > height()) {
            // If the object is not in the list, we add it now.
            if (!containsFloat(r->m_renderer)) {
                FloatingObject *floatingObj = new FloatingObject(r->type());
                floatingObj->m_top = r->m_top - yoff;
                floatingObj->m_bottom = r->m_bottom - yoff;
                floatingObj->m_left = r->m_left - xoff;
                floatingObj->m_width = r->m_width;
                floatingObj->m_renderer = r->m_renderer;

                // The nearest enclosing layer always paints the float (so that zindex and stacking
                // behaves properly).  We always want to propagate the desire to paint the float as
                // far out as we can, to the outermost block that overlaps the float, stopping only
                // if we hit a layer boundary.
                if (r->m_renderer->enclosingLayer() == enclosingLayer())
                    r->m_shouldPaint = false;
                else
                    floatingObj->m_shouldPaint = false;
                
                // We create the floating object list lazily.
                if (!m_floatingObjects) {
                    m_floatingObjects = new DeprecatedPtrList<FloatingObject>;
                    m_floatingObjects->setAutoDelete(true);
                }
                m_floatingObjects->append(floatingObj);
            }
        } else if (makeChildPaintOtherFloats && !r->m_shouldPaint && !r->m_renderer->hasSelfPaintingLayer() &&
                   r->m_renderer->isDescendantOf(child) && r->m_renderer->enclosingLayer() == child->enclosingLayer())
            // The float is not overhanging from this block, so if it is a descendant of the child, the child should
            // paint it (the other case is that it is intruding into the child), unless it has its own layer or enclosing
            // layer.
            // If makeChildPaintOtherFloats is false, it means that the child must already know about all the floats
            // it should paint.
            r->m_shouldPaint = true;

        if (r->m_shouldPaint && !r->m_renderer->hasSelfPaintingLayer()) {
            IntRect floatOverflowRect = r->m_renderer->overflowRect(false);
            floatOverflowRect.move(r->m_left + r->m_renderer->marginLeft(), r->m_top + r->m_renderer->marginTop());
            floatsOverflowRect.unite(floatOverflowRect);
        }
    }
    child->addVisualOverflow(floatsOverflowRect);
    return lowestFloatBottom;
}

void RenderBlock::addIntrudingFloats(RenderBlock* prev, int xoff, int yoff)
{
    // If the parent or previous sibling doesn't have any floats to add, don't bother.
    if (!prev->m_floatingObjects)
        return;

    DeprecatedPtrListIterator<FloatingObject> it(*prev->m_floatingObjects);
    for (FloatingObject *r; (r = it.current()); ++it) {
        if (r->m_bottom > yoff) {
            // The object may already be in our list. Check for it up front to avoid
            // creating duplicate entries.
            FloatingObject* f = 0;
            if (m_floatingObjects) {
                DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
                while ((f = it.current())) {
                    if (f->m_renderer == r->m_renderer) break;
                    ++it;
                }
            }
            if (!f) {
                FloatingObject *floatingObj = new FloatingObject(r->type());
                floatingObj->m_top = r->m_top - yoff;
                floatingObj->m_bottom = r->m_bottom - yoff;
                floatingObj->m_left = r->m_left - xoff;
                // Applying the child's margin makes no sense in the case where the child was passed in.
                // since his own margin was added already through the subtraction of the |xoff| variable
                // above.  |xoff| will equal -flow->marginLeft() in this case, so it's already been taken
                // into account.  Only apply this code if |child| is false, since otherwise the left margin
                // will get applied twice.
                if (prev != parent())
                    floatingObj->m_left += prev->marginLeft();
                floatingObj->m_left -= marginLeft();
                floatingObj->m_shouldPaint = false;  // We are not in the direct inheritance chain for this float. We will never paint it.
                floatingObj->m_width = r->m_width;
                floatingObj->m_renderer = r->m_renderer;
                
                // We create the floating object list lazily.
                if (!m_floatingObjects) {
                    m_floatingObjects = new DeprecatedPtrList<FloatingObject>;
                    m_floatingObjects->setAutoDelete(true);
                }
                m_floatingObjects->append(floatingObj);
            }
        }
    }
}

bool RenderBlock::avoidsFloats() const
{
    // Floats can't intrude into our box if we have a non-auto column count or width.
    return RenderFlow::avoidsFloats() || !style()->hasAutoColumnCount() || !style()->hasAutoColumnWidth();
}

bool RenderBlock::containsFloat(RenderObject* o)
{
    if (m_floatingObjects) {
        DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
        while (it.current()) {
            if (it.current()->m_renderer == o)
                return true;
            ++it;
        }
    }
    return false;
}

void RenderBlock::markAllDescendantsWithFloatsForLayout(RenderBox* floatToRemove, bool inLayout)
{
    setChildNeedsLayout(true, !inLayout);

    if (floatToRemove)
        removeFloatingObject(floatToRemove);

    // Iterate over our children and mark them as needed.
    if (!childrenInline()) {
        for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
            if (child->isRenderBlock() && !child->isFloatingOrPositioned() &&
                ((floatToRemove ? child->containsFloat(floatToRemove) : child->containsFloats()) || child->shrinkToAvoidFloats()))
                static_cast<RenderBlock*>(child)->markAllDescendantsWithFloatsForLayout(floatToRemove, inLayout);
        }
    }
}

int RenderBlock::getClearDelta(RenderBox* child)
{
    // There is no need to compute clearance if we have no floats.
    if (!containsFloats())
        return 0;
    
    // At least one float is present.  We need to perform the clearance computation.
    bool clearSet = child->style()->clear() != CNONE;
    int bottom = 0;
    switch (child->style()->clear()) {
        case CNONE:
            break;
        case CLEFT:
            bottom = leftBottom();
            break;
        case CRIGHT:
            bottom = rightBottom();
            break;
        case CBOTH:
            bottom = floatBottom();
            break;
    }

    // We also clear floats if we are too big to sit on the same line as a float (and wish to avoid floats by default).
    // FIXME: Note that the remaining space checks aren't quite accurate, since you should be able to clear only some floats (the minimum # needed
    // to fit) and not all (we should be using nextFloatBottomBelow and looping).
    // Do not allow tables to wrap in quirks or even in almost strict mode 
    // (ebay on the PLT, finance.yahoo.com in the real world, versiontracker.com forces even almost strict mode not to work)
    int result = clearSet ? max(0, bottom - child->y()) : 0;
    if (!result && child->avoidsFloats() && child->style()->width().isFixed() && 
        child->minPrefWidth() > lineWidth(child->y()) && child->minPrefWidth() <= availableWidth() && 
        document()->inStrictMode())   
        result = max(0, floatBottom() - child->y());
    return result;
}

void RenderBlock::addVisualOverflow(const IntRect& r)
{
    if (r.isEmpty())
        return;
    m_overflowLeft = min(m_overflowLeft, r.x());
    m_overflowWidth = max(m_overflowWidth, r.right());
    m_overflowTop = min(m_overflowTop, r.y());
    m_overflowHeight = max(m_overflowHeight, r.bottom());
}

bool RenderBlock::isPointInOverflowControl(HitTestResult& result, int, int, int, int)
{
    if (!scrollsOverflow())
        return false;

    return layer()->hitTestOverflowControls(result);
}

bool RenderBlock::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    int tx = _tx + x();
    int ty = _ty + y();

    if (!isRenderView()) {
        // Check if we need to do anything at all.
        IntRect overflowBox = overflowRect(false);
        overflowBox.move(tx, ty);
        if (!overflowBox.contains(_x, _y))
            return false;
    }

    if ((hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) && isPointInOverflowControl(result, _x, _y, tx, ty)) {
        updateHitTestResult(result, IntPoint(_x - tx, _y - ty));
        return true;
    }

    // If we have clipping, then we can't have any spillout.
    bool useOverflowClip = hasOverflowClip() && !hasSelfPaintingLayer();
    bool useClip = (hasControlClip() || useOverflowClip);
    bool checkChildren = !useClip || (hasControlClip() ? controlClipRect(tx, ty).contains(_x, _y) : getOverflowClipRect(tx, ty).contains(_x, _y));
    if (checkChildren) {
        // Hit test descendants first.
        int scrolledX = tx;
        int scrolledY = ty;
        if (hasOverflowClip())
            m_layer->subtractScrolledContentOffset(scrolledX, scrolledY);

        // Hit test contents if we don't have columns.
        if (!m_hasColumns && hitTestContents(request, result, _x, _y, scrolledX, scrolledY, hitTestAction))
            return true;
            
        // Hit test our columns if we do have them.
        if (m_hasColumns && hitTestColumns(request, result, _x, _y, scrolledX, scrolledY, hitTestAction))
            return true;

        // Hit test floats.
        if (hitTestAction == HitTestFloat && m_floatingObjects) {
            if (isRenderView()) {
                scrolledX += static_cast<RenderView*>(this)->frameView()->scrollX();
                scrolledY += static_cast<RenderView*>(this)->frameView()->scrollY();
            }
            
            FloatingObject* o;
            DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
            for (it.toLast(); (o = it.current()); --it) {
                if (o->m_shouldPaint && !o->m_renderer->hasSelfPaintingLayer()) {
                    int xoffset = scrolledX + o->m_left + o->m_renderer->marginLeft() - o->m_renderer->x();
                    int yoffset =  scrolledY + o->m_top + o->m_renderer->marginTop() - o->m_renderer->y();
                    if (o->m_renderer->hitTest(request, result, IntPoint(_x, _y), xoffset, yoffset)) {
                        updateHitTestResult(result, IntPoint(_x - xoffset, _y - yoffset));
                        return true;
                    }
                }
            }
        }
    }

    // Now hit test our background
    if (hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) {
        IntRect boundsRect(tx, ty, width(), height());
        if (visibleToHitTesting() && boundsRect.contains(_x, _y)) {
            updateHitTestResult(result, IntPoint(_x - tx, _y - ty));
            return true;
        }
    }

    return false;
}

bool RenderBlock::hitTestColumns(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    // We need to do multiple passes, breaking up our hit testing into strips.
    // We can always go left to right, since column contents are clipped (meaning that there
    // can't be any overlap).
    int currXOffset = 0;
    int currYOffset = 0;
    int colGap = columnGap();
    Vector<IntRect>* colRects = columnRects();
    for (unsigned i = 0; i < colRects->size(); i++) {
        IntRect colRect = colRects->at(i);
        colRect.move(tx, ty);
        
        if (colRect.contains(x, y)) {
            // The point is inside this column.
            // Adjust tx and ty to change where we hit test.
        
            int finalX = tx + currXOffset;
            int finalY = ty + currYOffset;
            return hitTestContents(request, result, x, y, finalX, finalY, hitTestAction);
        }
        
        // Move to the next position.
        if (style()->direction() == LTR)
            currXOffset += colRect.width() + colGap;
        else
            currXOffset -= (colRect.width() + colGap);

        currYOffset -= colRect.height();
    }

    return false;
}

bool RenderBlock::hitTestContents(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    if (childrenInline() && !isTable()) {
        // We have to hit-test our line boxes.
        if (hitTestLines(request, result, x, y, tx, ty, hitTestAction)) {
            updateHitTestResult(result, IntPoint(x - tx, y - ty));
            return true;
        }
    } else {
        // Hit test our children.
        HitTestAction childHitTest = hitTestAction;
        if (hitTestAction == HitTestChildBlockBackgrounds)
            childHitTest = HitTestChildBlockBackground;
        for (RenderBox* child = lastChildBox(); child; child = child->previousSiblingBox()) {
            if (!child->hasSelfPaintingLayer() && !child->isFloating() && child->nodeAtPoint(request, result, x, y, tx, ty, childHitTest)) {
                updateHitTestResult(result, IntPoint(x - tx, y - ty));
                return true;
            }
        }
    }
    
    return false;
}

Position RenderBlock::positionForBox(InlineBox *box, bool start) const
{
    if (!box)
        return Position();

    if (!box->object()->element())
        return Position(element(), start ? caretMinOffset() : caretMaxOffset());

    if (!box->isInlineTextBox())
        return Position(box->object()->element(), start ? box->object()->caretMinOffset() : box->object()->caretMaxOffset());

    InlineTextBox *textBox = static_cast<InlineTextBox *>(box);
    return Position(box->object()->element(), start ? textBox->start() : textBox->start() + textBox->len());
}

Position RenderBlock::positionForRenderer(RenderObject* renderer, bool start) const
{
    if (!renderer)
        return Position(element(), 0);

    Node* node = renderer->element() ? renderer->element() : element();
    if (!node)
        return Position();

    ASSERT(renderer == node->renderer());

    int offset = start ? renderer->caretMinOffset() : renderer->caretMaxOffset();

    // FIXME: This was a runtime check that seemingly couldn't fail; changed it to an assertion for now.
    ASSERT(!node->isCharacterDataNode() || renderer->isText());

    return Position(node, offset);
}

VisiblePosition RenderBlock::positionForCoordinates(int x, int y)
{
    if (isTable())
        return RenderFlow::positionForCoordinates(x, y); 

    int top = borderTop();
    int bottom = top + paddingTop() + contentHeight() + paddingBottom();

    int left = borderLeft();
    int right = left + paddingLeft() + contentWidth() + paddingRight();

    Node* n = element();
    
    int contentsX = x;
    int contentsY = y;
    offsetForContents(contentsX, contentsY);

    if (isReplaced()) {
        if (y < 0 || y < height() && x < 0)
            return VisiblePosition(n, caretMinOffset(), DOWNSTREAM);
        if (y >= height() || y >= 0 && x >= width())
            return VisiblePosition(n, caretMaxOffset(), DOWNSTREAM);
    } 

    // This above code is appropriate for selecting in web content on the desktop,
    // but it gets in the way of the only selection-like code we have on
    // iPhone: placing the insertion point when editing. On iPhone, we
    // always want the behavior where the insertion point seeks the closest
    // piece of editable content, and not the "at start if above/at end
    // if below" behavior the desktop code provides.

    right = right; // dummy line to keep compiler from issuing unused variable warning

    if (y < top) {
        if (childrenInline()) {
            if (!firstRootBox())
                return VisiblePosition(n, 0, DOWNSTREAM);        
            InlineBox* closestBox = firstRootBox()->closestLeafChildForXPos(x);
            if (closestBox)
                // pass the box a y position that is inside it
                return closestBox->object()->positionForCoordinates(contentsX, closestBox->m_y);
        }
        else {
            // block children
            if (firstChild()) {
                RenderObject* c = firstChild();
                return c->positionForCoordinates(contentsX - toRenderBox(c)->x(), contentsY - toRenderBox(c)->y());
            }
        }
    }
    else if (y >= bottom) {
        if (childrenInline()) {
            if (!lastRootBox())
                return VisiblePosition(n, 0, DOWNSTREAM);        
            InlineBox* closestBox = lastRootBox()->closestLeafChildForXPos(x);
            if (closestBox) {
                if (document()->frame()->singleLineSelectionBehavior() || y - bottom < 8) {
                    // in the box, or close to it
                    // pass the box a y position that is inside it
                    return closestBox->object()->positionForCoordinates(contentsX, closestBox->m_y);
                }
                else {
                    // send selection to the end of this box by passing
                    // a y coordinate beneath it.
                    return closestBox->object()->positionForCoordinates(contentsX, y);
                }
            }
        }
        else {
            // block children
            if (lastChild()) {
                RenderObject* c = firstChild();
                return lastChild()->positionForCoordinates(contentsX - toRenderBox(c)->x(), contentsY - toRenderBox(c)->y());
            }
        }
    }

    if (childrenInline()) {
        if (!firstRootBox())
            return VisiblePosition(n, 0, DOWNSTREAM);

        if (contentsY < firstRootBox()->topOverflow() - verticalLineClickFudgeFactor)
            // y coordinate is above first root line box
            {
                InlineBox* closestBox = firstRootBox()->closestLeafChildForXPos(x);
                if (closestBox)
                    // pass the box a y position that is inside it
                    return closestBox->object()->positionForCoordinates(contentsX, closestBox->m_y);
            }

        
        // look for the closest line box in the root box which is at the passed-in y coordinate
        for (RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox()) {
            // set the bottom based on whether there is a next root box
            if (root->nextRootBox())
                // FIXME: make the break point halfway between the bottom of the previous root box and the top of the next root box
                bottom = root->nextRootBox()->topOverflow();
            else
                bottom = root->bottomOverflow() + verticalLineClickFudgeFactor;
            // check if this root line box is located at this y coordinate
            if (contentsY < bottom && root->firstChild()) {
                InlineBox* closestBox = root->closestLeafChildForXPos(x);
                if (closestBox)
                    // pass the box a y position that is inside it
                    return closestBox->object()->positionForCoordinates(contentsX, closestBox->m_y);
            }
        }

        if (lastRootBox())
            // y coordinate is below last root line box
            {
                InlineBox* closestBox = lastRootBox()->closestLeafChildForXPos(x);
                if (closestBox) {
                    // pass the box a y position that is inside it
                    return closestBox->object()->positionForCoordinates(contentsX, closestBox->m_y);
                }
            }
 
        return VisiblePosition(n, 0, DOWNSTREAM);
    }
    
    // See if any child blocks exist at this y coordinate.
    if (firstChildBox() && contentsY < firstChildBox()->y())
        return VisiblePosition(n, 0, DOWNSTREAM);
    for (RenderBox* renderer = firstChildBox(); renderer; renderer = renderer->nextSiblingBox()) {
        if (renderer->height() == 0 || renderer->style()->visibility() != VISIBLE || renderer->isFloatingOrPositioned())
            continue;
        RenderBox* next = renderer->nextSiblingBox();
        while (next && next->isFloatingOrPositioned())
            next = next->nextSiblingBox();
        if (next) 
            bottom = next->y();
        else
            bottom = top + scrollHeight();
        if (contentsY >= renderer->y() && contentsY < bottom)
            return renderer->positionForCoordinates(contentsX - renderer->x(), contentsY - renderer->y());
    }
    
    return RenderFlow::positionForCoordinates(x, y);
}

void RenderBlock::offsetForContents(int& tx, int& ty) const
{
    if (hasOverflowClip())
        m_layer->addScrolledContentOffset(tx, ty);

    if (m_hasColumns) {
        IntPoint contentsPoint(tx, ty);
        adjustPointToColumnContents(contentsPoint);
        tx = contentsPoint.x();
        ty = contentsPoint.y();
    }
}

int RenderBlock::availableWidth() const
{
    // If we have multiple columns, then the available width is reduced to our column width.
    if (m_hasColumns)
        return desiredColumnWidth();
    return contentWidth();
}

int RenderBlock::columnGap() const
{
    if (style()->hasNormalColumnGap())
        return style()->fontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return static_cast<int>(style()->columnGap());
}

void RenderBlock::calcColumnWidth()
{    
    // Calculate our column width and column count.
    unsigned desiredColumnCount = 1;
    int desiredColumnWidth = contentWidth();
    
    // For now, we don't support multi-column layouts when printing, since we have to do a lot of work for proper pagination.
    if (document()->printing() || (style()->hasAutoColumnCount() && style()->hasAutoColumnWidth())) {
        setDesiredColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
        return;
    }
        
    int availWidth = desiredColumnWidth;
    int colGap = columnGap();
    int colWidth = max(1, static_cast<int>(style()->columnWidth()));
    int colCount = max(1, static_cast<int>(style()->columnCount()));

    if (style()->hasAutoColumnWidth()) {
        if ((colCount - 1) * colGap < availWidth) {
            desiredColumnCount = colCount;
            desiredColumnWidth = (availWidth - (desiredColumnCount - 1) * colGap) / desiredColumnCount;
        } else if (colGap < availWidth) {
            desiredColumnCount = availWidth / colGap;
            desiredColumnWidth = (availWidth - (desiredColumnCount - 1) * colGap) / desiredColumnCount;
        }
    } else if (style()->hasAutoColumnCount()) {
        if (colWidth < availWidth) {
            desiredColumnCount = (availWidth + colGap) / (colWidth + colGap);
            desiredColumnWidth = (availWidth - (desiredColumnCount - 1) * colGap) / desiredColumnCount;
        }
    } else {
        // Both are set.
        if (colCount * colWidth + (colCount - 1) * colGap <= availWidth) {
            desiredColumnCount = colCount;
            desiredColumnWidth = colWidth;
        } else if (colWidth < availWidth) {
            desiredColumnCount = (availWidth + colGap) / (colWidth + colGap);
            desiredColumnWidth = (availWidth - (desiredColumnCount - 1) * colGap) / desiredColumnCount;
        }
    }
    setDesiredColumnCountAndWidth(desiredColumnCount, desiredColumnWidth);
}

void RenderBlock::setDesiredColumnCountAndWidth(int count, int actualWidth)
{
    if (count == 1) {
        if (m_hasColumns) {
            delete gColumnInfoMap->take(this);
            m_hasColumns = false;
        }
    } else {
        ColumnInfo* info;
        if (m_hasColumns)
            info = gColumnInfoMap->get(this);
        else {
            if (!gColumnInfoMap)
                gColumnInfoMap = new ColumnInfoMap;
            info = new ColumnInfo;
            gColumnInfoMap->add(this, info);
            m_hasColumns = true;
        }
        info->m_desiredColumnCount = count;
        info->m_desiredColumnWidth = actualWidth;   
    }
}

int RenderBlock::desiredColumnWidth() const
{
    if (!m_hasColumns)
        return contentWidth();
    return gColumnInfoMap->get(this)->m_desiredColumnWidth;
}

unsigned RenderBlock::desiredColumnCount() const
{
    if (!m_hasColumns)
        return 1;
    return gColumnInfoMap->get(this)->m_desiredColumnCount;
}

Vector<IntRect>* RenderBlock::columnRects() const
{
    if (!m_hasColumns)
        return 0;
    return &gColumnInfoMap->get(this)->m_columnRects;    
}

int RenderBlock::layoutColumns(int endOfContent)
{
    // Don't do anything if we have no columns
    if (!m_hasColumns)
        return -1;

    ColumnInfo* info = gColumnInfoMap->get(this);
    int desiredColumnWidth = info->m_desiredColumnWidth;
    int desiredColumnCount = info->m_desiredColumnCount;
    Vector<IntRect>* columnRects = &info->m_columnRects;
    
    bool computeIntrinsicHeight = (endOfContent == -1);

    // Fill the columns in to the available height.  Attempt to balance the height of the columns
    int availableHeight = contentHeight();
    int colHeight = computeIntrinsicHeight ? availableHeight / desiredColumnCount : availableHeight;
    
    // Add in half our line-height to help with best-guess initial balancing.
    int columnSlop = lineHeight(false) / 2;
    int remainingSlopSpace = columnSlop * desiredColumnCount;

    if (computeIntrinsicHeight)
        colHeight += columnSlop;
                                                                            
    int colGap = columnGap();

    // Compute a collection of column rects.
    columnRects->clear();
    
    // Then we do a simulated "paint" into the column slices and allow the content to slightly adjust our individual column rects.
    // FIXME: We need to take into account layers that are affected by the columns as well here so that they can have an opportunity
    // to adjust column rects also.
    RenderView* v = view();
    int left = borderLeft() + paddingLeft();
    int top = borderTop() + paddingTop();
    int currX = style()->direction() == LTR ? borderLeft() + paddingLeft() : borderLeft() + paddingLeft() + contentWidth() - desiredColumnWidth;
    int currY = top;
    unsigned colCount = desiredColumnCount;
    int maxColBottom = borderTop() + paddingTop();
    int contentBottom = top + availableHeight; 
    for (unsigned i = 0; i < colCount; i++) {
        // If we aren't constrained, then the last column can just get all the remaining space.
        if (computeIntrinsicHeight && i == colCount - 1)
            colHeight = availableHeight;

        // This represents the real column position.
        IntRect colRect(currX, top, desiredColumnWidth, colHeight);
        
        // For the simulated paint, we pretend like everything is in one long strip.
        IntRect pageRect(left, currY, desiredColumnWidth, colHeight);
        v->setPrintRect(pageRect);
        v->setTruncatedAt(currY + colHeight);
        GraphicsContext context((PlatformGraphicsContext*)0);
        RenderObject::PaintInfo paintInfo(&context, pageRect, PaintPhaseForeground, false, 0, 0);
        
        m_hasColumns = false;
        paintObject(paintInfo, 0, 0);
        m_hasColumns = true;

        int adjustedBottom = v->bestTruncatedAt();
        if (adjustedBottom <= currY)
            adjustedBottom = currY + colHeight;
        
        colRect.setHeight(adjustedBottom - currY);
        
        // Add in the lost space to the subsequent columns.
        // FIXME: This will create a "staircase" effect if there are enough columns, but the effect should be pretty subtle.
        if (computeIntrinsicHeight) {
            int lostSpace = colHeight - colRect.height();
            if (lostSpace > remainingSlopSpace) {
                // Redestribute the space among the remaining columns.
                int spaceToRedistribute = lostSpace - remainingSlopSpace;
                int remainingColumns = colCount - i + 1;
                colHeight += spaceToRedistribute / remainingColumns;
            } 
            remainingSlopSpace = max(0, remainingSlopSpace - lostSpace);
        }
        
        if (style()->direction() == LTR)
            currX += desiredColumnWidth + colGap;
        else
            currX -= (desiredColumnWidth + colGap);

        currY += colRect.height();
        availableHeight -= colRect.height();

        maxColBottom = max(colRect.bottom(), maxColBottom);

        columnRects->append(colRect);
        
        // Start adding in more columns as long as there's still content left.
        if (currY < endOfContent && i == colCount - 1 && (computeIntrinsicHeight || contentHeight()))
            colCount++;
    }

    m_overflowWidth = max(width(), currX - colGap);
    m_overflowLeft = min(0, currX + desiredColumnWidth + colGap);

    m_overflowHeight = maxColBottom;
    int toAdd = borderBottom() + paddingBottom() + horizontalScrollbarHeight();
        
    if (computeIntrinsicHeight)
        setHeight(m_overflowHeight + toAdd);

    v->setPrintRect(IntRect());
    v->setTruncatedAt(0);
    
    ASSERT(colCount == columnRects->size());
    
    return contentBottom;
}

void RenderBlock::adjustPointToColumnContents(IntPoint& point) const
{
    // Just bail if we have no columns.
    if (!m_hasColumns)
        return;
    
    Vector<IntRect>* colRects = columnRects();

    // Determine which columns we intersect.
    int colGap = columnGap();
    int leftGap = colGap / 2;
    IntPoint columnPoint(colRects->at(0).location());
    int yOffset = 0;
    for (unsigned i = 0; i < colRects->size(); i++) {
        // Add in half the column gap to the left and right of the rect.
        IntRect colRect = colRects->at(i);
        IntRect gapAndColumnRect(colRect.x() - leftGap, colRect.y(), colRect.width() + colGap, colRect.height());
        
        if (gapAndColumnRect.contains(point)) {
            // We're inside the column.  Translate the x and y into our column coordinate space.
            point.move(columnPoint.x() - colRect.x(), yOffset);
            return;
        }

        // Move to the next position.
        yOffset += colRect.height();
    }
}

void RenderBlock::adjustRectForColumns(IntRect& r) const
{
    // Just bail if we have no columns.
    if (!m_hasColumns)
        return;
    
    Vector<IntRect>* colRects = columnRects();

    // Begin with a result rect that is empty.
    IntRect result;
    
    // Determine which columns we intersect.
    int currXOffset = 0;
    int currYOffset = 0;
    int colGap = columnGap();
    for (unsigned i = 0; i < colRects->size(); i++) {
        IntRect colRect = colRects->at(i);
        
        IntRect repaintRect = r;
        repaintRect.move(currXOffset, currYOffset);
        
        repaintRect.intersect(colRect);
        
        result.unite(repaintRect);

        // Move to the next position.
        if (style()->direction() == LTR)
            currXOffset += colRect.width() + colGap;
        else
            currXOffset -= (colRect.width() + colGap);

        currYOffset -= colRect.height();
    }

    r = result;
}

void RenderBlock::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    updateFirstLetter();

    if (!isTableCell() && style()->width().isFixed() && style()->width().value() > 0)
        m_minPrefWidth = m_maxPrefWidth = calcContentBoxWidth(style()->width().value());
    else {
        m_minPrefWidth = 0;
        m_maxPrefWidth = 0;

        if (childrenInline())
            calcInlinePrefWidths();
        else
            calcBlockPrefWidths();

        m_maxPrefWidth = max(m_minPrefWidth, m_maxPrefWidth);

        if (!style()->autoWrap() && childrenInline()) {
            m_minPrefWidth = m_maxPrefWidth;
            
            // A horizontal marquee with inline children has no minimum width.
            if (m_layer && m_layer->marquee() && m_layer->marquee()->isHorizontal())
                m_minPrefWidth = 0;
        }

        if (isTableCell()) {
            Length w = static_cast<const RenderTableCell*>(this)->styleOrColWidth();
            if (w.isFixed() && w.value() > 0)
                m_maxPrefWidth = max(m_minPrefWidth, calcContentBoxWidth(w.value()));
        }
    }
    
    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPrefWidth = max(m_maxPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minPrefWidth = max(m_minPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
    }
    
    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPrefWidth = min(m_maxPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minPrefWidth = min(m_minPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = 0;
    toAdd = borderLeft() + borderRight() + paddingLeft() + paddingRight();

    m_minPrefWidth += toAdd;
    m_maxPrefWidth += toAdd;

    setPrefWidthsDirty(false);
}

struct InlineMinMaxIterator
{
/* InlineMinMaxIterator is a class that will iterate over all render objects that contribute to
   inline min/max width calculations.  Note the following about the way it walks:
   (1) Positioned content is skipped (since it does not contribute to min/max width of a block)
   (2) We do not drill into the children of floats or replaced elements, since you can't break
       in the middle of such an element.
   (3) Inline flows (e.g., <a>, <span>, <i>) are walked twice, since each side can have
       distinct borders/margin/padding that contribute to the min/max width.
*/
    RenderObject* parent;
    RenderObject* current;
    bool endOfInline;

    InlineMinMaxIterator(RenderObject* p, bool end = false)
        :parent(p), current(p), endOfInline(end) {}

    RenderObject* next();
};

RenderObject* InlineMinMaxIterator::next()
{
    RenderObject* result = 0;
    bool oldEndOfInline = endOfInline;
    endOfInline = false;
    while (current || current == parent) {
        if (!oldEndOfInline &&
            (current == parent ||
             (!current->isFloating() && !current->isReplaced() && !current->isPositioned())))
            result = current->firstChild();
        if (!result) {
            // We hit the end of our inline. (It was empty, e.g., <span></span>.)
            if (!oldEndOfInline && current->isRenderInline()) {
                result = current;
                endOfInline = true;
                break;
            }

            while (current && current != parent) {
                result = current->nextSibling();
                if (result) break;
                current = current->parent();
                if (current && current != parent && current->isRenderInline()) {
                    result = current;
                    endOfInline = true;
                    break;
                }
            }
        }

        if (!result)
            break;

        if (!result->isPositioned() && (result->isText() || result->isFloating() || result->isReplaced() || result->isRenderInline()))
             break;
        
        current = result;
        result = 0;
    }

    // Update our position.
    current = result;
    return current;
}

static int getBPMWidth(int childValue, Length cssUnit)
{
    if (cssUnit.type() != Auto)
        return (cssUnit.isFixed() ? cssUnit.value() : childValue);
    return 0;
}

static int getBorderPaddingMargin(const RenderBox* child, bool endOfInline)
{
    RenderStyle* cstyle = child->style();
    int result = 0;
    bool leftSide = (cstyle->direction() == LTR) ? !endOfInline : endOfInline;
    result += getBPMWidth((leftSide ? child->marginLeft() : child->marginRight()),
                          (leftSide ? cstyle->marginLeft() :
                                      cstyle->marginRight()));
    result += getBPMWidth((leftSide ? child->paddingLeft() : child->paddingRight()),
                          (leftSide ? cstyle->paddingLeft() :
                                      cstyle->paddingRight()));
    result += leftSide ? child->borderLeft() : child->borderRight();
    return result;
}

static inline void stripTrailingSpace(int& inlineMax, int& inlineMin,
                                      RenderObject* trailingSpaceChild)
{
    if (trailingSpaceChild && trailingSpaceChild->isText()) {
        // Collapse away the trailing space at the end of a block.
        RenderText* t = toRenderText(trailingSpaceChild);
        const UChar space = ' ';
        const Font& font = t->style()->font(); // FIXME: This ignores first-line.
        int spaceWidth = font.width(TextRun(&space, 1));
        inlineMax -= spaceWidth + font.wordSpacing();
        if (inlineMin > inlineMax)
            inlineMin = inlineMax;
    }
}

void RenderBlock::calcInlinePrefWidths()
{
    int inlineMax = 0;
    int inlineMin = 0;

    int cw = containingBlock()->contentWidth();

    // If we are at the start of a line, we want to ignore all white-space.
    // Also strip spaces if we previously had text that ended in a trailing space.
    bool stripFrontSpaces = true;
    RenderObject* trailingSpaceChild = 0;

    // Firefox and Opera will allow a table cell to grow to fit an image inside it under
    // very specific cirucumstances (in order to match common WinIE renderings). 
    // Not supporting the quirk has caused us to mis-render some real sites. (See Bugzilla 10517.) 
    bool allowImagesToBreak = !style()->htmlHacks() || !isTableCell() || !style()->width().isIntrinsicOrAuto();

    bool autoWrap, oldAutoWrap;
    autoWrap = oldAutoWrap = style()->autoWrap();

    InlineMinMaxIterator childIterator(this);
    bool addedTextIndent = false; // Only gets added in once.
    RenderObject* prevFloat = 0;
    RenderObject* previousLeaf = 0;
    while (RenderObject* child = childIterator.next()) {
        autoWrap = child->isReplaced() ? child->parent()->style()->autoWrap() : 
            child->style()->autoWrap();

        if (!child->isBR()) {
            // Step One: determine whether or not we need to go ahead and
            // terminate our current line.  Each discrete chunk can become
            // the new min-width, if it is the widest chunk seen so far, and
            // it can also become the max-width.

            // Children fall into three categories:
            // (1) An inline flow object.  These objects always have a min/max of 0,
            // and are included in the iteration solely so that their margins can
            // be added in.
            //
            // (2) An inline non-text non-flow object, e.g., an inline replaced element.
            // These objects can always be on a line by themselves, so in this situation
            // we need to go ahead and break the current line, and then add in our own
            // margins and min/max width on its own line, and then terminate the line.
            //
            // (3) A text object.  Text runs can have breakable characters at the start,
            // the middle or the end.  They may also lose whitespace off the front if
            // we're already ignoring whitespace.  In order to compute accurate min-width
            // information, we need three pieces of information.
            // (a) the min-width of the first non-breakable run.  Should be 0 if the text string
            // starts with whitespace.
            // (b) the min-width of the last non-breakable run. Should be 0 if the text string
            // ends with whitespace.
            // (c) the min/max width of the string (trimmed for whitespace).
            //
            // If the text string starts with whitespace, then we need to go ahead and
            // terminate our current line (unless we're already in a whitespace stripping
            // mode.
            //
            // If the text string has a breakable character in the middle, but didn't start
            // with whitespace, then we add the width of the first non-breakable run and
            // then end the current line.  We then need to use the intermediate min/max width
            // values (if any of them are larger than our current min/max).  We then look at
            // the width of the last non-breakable run and use that to start a new line
            // (unless we end in whitespace).
            RenderStyle* cstyle = child->style();
            int childMin = 0;
            int childMax = 0;

            if (!child->isText()) {
                // Case (1) and (2).  Inline replaced and inline flow elements.
                if (child->isRenderInline()) {
                    // Add in padding/border/margin from the appropriate side of
                    // the element.
                    int bpm = getBorderPaddingMargin(static_cast<RenderFlow*>(child), childIterator.endOfInline);
                    childMin += bpm;
                    childMax += bpm;

                    inlineMin += childMin;
                    inlineMax += childMax;
                    
                    child->setPrefWidthsDirty(false);
                } else {
                    // Inline replaced elts add in their margins to their min/max values.
                    int margins = 0;
                    Length leftMargin = cstyle->marginLeft();
                    Length rightMargin = cstyle->marginRight();
                    if (leftMargin.isFixed())
                        margins += leftMargin.value();
                    if (rightMargin.isFixed())
                        margins += rightMargin.value();
                    childMin += margins;
                    childMax += margins;
                }
            }

            if (!child->isRenderInline() && !child->isText()) {
                // Case (2). Inline replaced elements and floats.
                // Go ahead and terminate the current line as far as
                // minwidth is concerned.
                childMin += child->minPrefWidth();
                childMax += child->maxPrefWidth();

                bool clearPreviousFloat;
                if (child->isFloating()) {
                    clearPreviousFloat = (prevFloat
                        && (prevFloat->style()->floating() == FLEFT && (child->style()->clear() & CLEFT)
                            || prevFloat->style()->floating() == FRIGHT && (child->style()->clear() & CRIGHT)));
                    prevFloat = child;
                } else
                    clearPreviousFloat = false;

                bool canBreakReplacedElement = !child->isImage() || allowImagesToBreak;
                if (canBreakReplacedElement && (autoWrap || oldAutoWrap) || clearPreviousFloat) {
                    m_minPrefWidth = max(inlineMin, m_minPrefWidth);
                    inlineMin = 0;
                }

                // If we're supposed to clear the previous float, then terminate maxwidth as well.
                if (clearPreviousFloat) {
                    m_maxPrefWidth = max(inlineMax, m_maxPrefWidth);
                    inlineMax = 0;
                }

                // Add in text-indent.  This is added in only once.
                int ti = 0;
                if (!addedTextIndent) {
                    addedTextIndent = true;
                    ti = style()->textIndent().calcMinValue(cw);
                    childMin+=ti;
                    childMax+=ti;
                }

                // Add our width to the max.
                inlineMax += childMax;

                if (!autoWrap || !canBreakReplacedElement) {
                    if (child->isFloating())
                        m_minPrefWidth = max(childMin, m_minPrefWidth);
                    else
                        inlineMin += childMin;
                } else {
                    // Now check our line.
                    m_minPrefWidth = max(childMin, m_minPrefWidth);

                    // Now start a new line.
                    inlineMin = 0;
                }

                // We are no longer stripping whitespace at the start of
                // a line.
                if (!child->isFloating()) {
                    stripFrontSpaces = false;
                    trailingSpaceChild = 0;
                }
            } else if (child->isText()) {
                // Case (3). Text.
                RenderText* t = toRenderText(child);

                if (t->isWordBreak()) {
                    m_minPrefWidth = max(inlineMin, m_minPrefWidth);
                    inlineMin = 0;
                    continue;
                }

                // Determine if we have a breakable character.  Pass in
                // whether or not we should ignore any spaces at the front
                // of the string.  If those are going to be stripped out,
                // then they shouldn't be considered in the breakable char
                // check.
                bool hasBreakableChar, hasBreak;
                int beginMin, endMin;
                bool beginWS, endWS;
                int beginMax, endMax;
                t->trimmedPrefWidths(inlineMax, beginMin, beginWS, endMin, endWS,
                                     hasBreakableChar, hasBreak, beginMax, endMax,
                                     childMin, childMax, stripFrontSpaces);

                // This text object will not be rendered, but it may still provide a breaking opportunity.
                if (!hasBreak && childMax == 0) {
                    if (autoWrap && (beginWS || endWS)) {
                        m_minPrefWidth = max(inlineMin, m_minPrefWidth);
                        inlineMin = 0;
                    }
                    continue;
                }
                
                if (stripFrontSpaces)
                    trailingSpaceChild = child;
                else
                    trailingSpaceChild = 0;

                // Add in text-indent.  This is added in only once.
                int ti = 0;
                if (!addedTextIndent) {
                    addedTextIndent = true;
                    ti = style()->textIndent().calcMinValue(cw);
                    childMin+=ti; beginMin += ti;
                    childMax+=ti; beginMax += ti;
                }
                
                // If we have no breakable characters at all,
                // then this is the easy case. We add ourselves to the current
                // min and max and continue.
                if (!hasBreakableChar) {
                    inlineMin += childMin;
                } else {
                    // We have a breakable character.  Now we need to know if
                    // we start and end with whitespace.
                    if (beginWS)
                        // Go ahead and end the current line.
                        m_minPrefWidth = max(inlineMin, m_minPrefWidth);
                    else {
                        inlineMin += beginMin;
                        m_minPrefWidth = max(inlineMin, m_minPrefWidth);
                        childMin -= ti;
                    }

                    inlineMin = childMin;

                    if (endWS) {
                        // We end in whitespace, which means we can go ahead
                        // and end our current line.
                        m_minPrefWidth = max(inlineMin, m_minPrefWidth);
                        inlineMin = 0;
                    } else {
                        m_minPrefWidth = max(inlineMin, m_minPrefWidth);
                        inlineMin = endMin;
                    }
                }

                if (hasBreak) {
                    inlineMax += beginMax;
                    m_maxPrefWidth = max(inlineMax, m_maxPrefWidth);
                    m_maxPrefWidth = max(childMax, m_maxPrefWidth);
                    inlineMax = endMax;
                } else
                    inlineMax += childMax;
            }
        } else {
            m_minPrefWidth = max(inlineMin, m_minPrefWidth);
            m_maxPrefWidth = max(inlineMax, m_maxPrefWidth);
            inlineMin = inlineMax = 0;
            stripFrontSpaces = true;
            trailingSpaceChild = 0;
        }

        oldAutoWrap = autoWrap;
        if (!child->isRenderInline())
            previousLeaf = child;
    }

    if (style()->collapseWhiteSpace())
        stripTrailingSpace(inlineMax, inlineMin, trailingSpaceChild);

    m_minPrefWidth = max(inlineMin, m_minPrefWidth);
    m_maxPrefWidth = max(inlineMax, m_maxPrefWidth);
}

// Use a very large value (in effect infinite).
#define BLOCK_MAX_WIDTH 15000

void RenderBlock::calcBlockPrefWidths()
{
    bool nowrap = style()->whiteSpace() == NOWRAP;

    RenderObject *child = firstChild();
    int floatLeftWidth = 0, floatRightWidth = 0;
    while (child) {
        // Positioned children don't affect the min/max width
        if (child->isPositioned()) {
            child = child->nextSibling();
            continue;
        }

        if (child->isFloating() || child->avoidsFloats()) {
            int floatTotalWidth = floatLeftWidth + floatRightWidth;
            if (child->style()->clear() & CLEFT) {
                m_maxPrefWidth = max(floatTotalWidth, m_maxPrefWidth);
                floatLeftWidth = 0;
            }
            if (child->style()->clear() & CRIGHT) {
                m_maxPrefWidth = max(floatTotalWidth, m_maxPrefWidth);
                floatRightWidth = 0;
            }
        }

        // A margin basically has three types: fixed, percentage, and auto (variable).
        // Auto and percentage margins simply become 0 when computing min/max width.
        // Fixed margins can be added in as is.
        Length ml = child->style()->marginLeft();
        Length mr = child->style()->marginRight();
        int margin = 0, marginLeft = 0, marginRight = 0;
        if (ml.isFixed())
            marginLeft += ml.value();
        if (mr.isFixed())
            marginRight += mr.value();
        margin = marginLeft + marginRight;

        int w = child->minPrefWidth() + margin;
        m_minPrefWidth = max(w, m_minPrefWidth);
        
        // IE ignores tables for calculation of nowrap. Makes some sense.
        if (nowrap && !child->isTable())
            m_maxPrefWidth = max(w, m_maxPrefWidth);

        w = child->maxPrefWidth() + margin;

        if (!child->isFloating()) {
            if (child->avoidsFloats()) {
                // Determine a left and right max value based off whether or not the floats can fit in the
                // margins of the object.  For negative margins, we will attempt to overlap the float if the negative margin
                // is smaller than the float width.
                int maxLeft = marginLeft > 0 ? max(floatLeftWidth, marginLeft) : floatLeftWidth + marginLeft;
                int maxRight = marginRight > 0 ? max(floatRightWidth, marginRight) : floatRightWidth + marginRight;
                w = child->maxPrefWidth() + maxLeft + maxRight;
                w = max(w, floatLeftWidth + floatRightWidth);
            }
            else
                m_maxPrefWidth = max(floatLeftWidth + floatRightWidth, m_maxPrefWidth);
            floatLeftWidth = floatRightWidth = 0;
        }
        
        if (child->isFloating()) {
            if (style()->floating() == FLEFT)
                floatLeftWidth += w;
            else
                floatRightWidth += w;
        } else
            m_maxPrefWidth = max(w, m_maxPrefWidth);

        // A very specific WinIE quirk.
        // Example:
        /*
           <div style="position:absolute; width:100px; top:50px;">
              <div style="position:absolute;left:0px;top:50px;height:50px;background-color:green">
                <table style="width:100%"><tr><td></table>
              </div>
           </div>
        */
        // In the above example, the inner absolute positioned block should have a computed width
        // of 100px because of the table.
        // We can achieve this effect by making the maxwidth of blocks that contain tables
        // with percentage widths be infinite (as long as they are not inside a table cell).
        if (style()->htmlHacks() && child->style()->width().isPercent() &&
            !isTableCell() && child->isTable() && m_maxPrefWidth < BLOCK_MAX_WIDTH) {
            RenderBlock* cb = containingBlock();
            while (!cb->isRenderView() && !cb->isTableCell())
                cb = cb->containingBlock();
            if (!cb->isTableCell())
                m_maxPrefWidth = BLOCK_MAX_WIDTH;
        }
        
        child = child->nextSibling();
    }

    // Always make sure these values are non-negative.
    m_minPrefWidth = max(0, m_minPrefWidth);
    m_maxPrefWidth = max(0, m_maxPrefWidth);

    m_maxPrefWidth = max(floatLeftWidth + floatRightWidth, m_maxPrefWidth);
}

bool RenderBlock::hasLineIfEmpty() const
{
    return element() && (element()->isContentEditable() && element()->rootEditableElement() == element() ||
                         element()->isShadowNode() && element()->shadowParentNode()->hasTagName(inputTag));
}

int RenderBlock::lineHeight(bool b, bool isRootLineBox) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isReplaced() && !isRootLineBox)
        return height() + marginTop() + marginBottom();
    return RenderFlow::lineHeight(b, isRootLineBox);
}

int RenderBlock::baselinePosition(bool b, bool isRootLineBox) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isReplaced() && !isRootLineBox) {
        // For "leaf" theme objects, let the theme decide what the baseline position is.
        // FIXME: Might be better to have a custom CSS property instead, so that if the theme
        // is turned off, checkboxes/radios will still have decent baselines.
        if (style()->hasAppearance() && !theme()->isControlContainer(style()->appearance()))
            return theme()->baselinePosition(this);
            
        // CSS2.1 states that the baseline of an inline block is the baseline of the last line box in
        // the normal flow.  We make an exception for marquees, since their baselines are meaningless
        // (the content inside them moves).  This matches WinIE as well, which just bottom-aligns them.
        // We also give up on finding a baseline if we have a vertical scrollbar, or if we are scrolled
        // vertically (e.g., an overflow:hidden block that has had scrollTop moved) or if the baseline is outside
        // of our content box.
        int baselinePos = (m_layer && (m_layer->marquee() || m_layer->verticalScrollbar() || m_layer->scrollYOffset() != 0)) ? -1 : getBaselineOfLastLineBox();
        if (baselinePos != -1 && baselinePos <= borderTop() + paddingTop() + contentHeight())
            return marginTop() + baselinePos;
        return height() + marginTop() + marginBottom();
    }
    return RenderFlow::baselinePosition(b, isRootLineBox);
}

int RenderBlock::getBaselineOfFirstLineBox() const
{
    if (!isBlockFlow())
        return RenderFlow::getBaselineOfFirstLineBox();

    if (childrenInline()) {
        if (firstLineBox())
            return firstLineBox()->yPos() + firstLineBox()->baseline();
        else
            return -1;
    }
    else {
        for (RenderBox* curr = firstChildBox(); curr; curr = curr->nextSiblingBox()) {
            if (!curr->isFloatingOrPositioned()) {
                int result = curr->getBaselineOfFirstLineBox();
                if (result != -1)
                    return curr->y() + result; // Translate to our coordinate space.
            }
        }
    }

    return -1;
}

int RenderBlock::getBaselineOfLastLineBox() const
{
    if (!isBlockFlow())
        return RenderFlow::getBaselineOfLastLineBox();

    if (childrenInline()) {
        if (!firstLineBox() && hasLineIfEmpty())
            return RenderFlow::baselinePosition(true, true) + borderTop() + paddingTop();
        if (lastLineBox())
            return lastLineBox()->yPos() + lastLineBox()->baseline();
        return -1;
    }
    else {
        bool haveNormalFlowChild = false;
        for (RenderBox* curr = lastChildBox(); curr; curr = curr->previousSiblingBox()) {
            if (!curr->isFloatingOrPositioned()) {
                haveNormalFlowChild = true;
                int result = curr->getBaselineOfLastLineBox();
                if (result != -1)
                    return curr->y() + result; // Translate to our coordinate space.
            }
        }
        if (!haveNormalFlowChild && hasLineIfEmpty())
            return RenderFlow::baselinePosition(true, true) + borderTop() + paddingTop();
    }

    return -1;
}

bool RenderBlock::containsNonZeroBidiLevel() const
{
    for (RootInlineBox* root = firstRootBox(); root; root = root->nextRootBox()) {
        for (InlineBox* box = root->firstLeafChild(); box; box = box->nextLeafChild()) {
            if (box->bidiLevel())
                return true;
        }
    }
    return false;
}

RenderBlock* RenderBlock::firstLineBlock() const
{
    RenderBlock* firstLineBlock = const_cast<RenderBlock*>(this);
    bool hasPseudo = false;
    while (true) {
        hasPseudo = firstLineBlock->style()->hasPseudoStyle(RenderStyle::FIRST_LINE);
        if (hasPseudo)
            break;
        RenderObject* parentBlock = firstLineBlock->parent();
        if (firstLineBlock->isReplaced() || firstLineBlock->isFloating() || 
            !parentBlock || parentBlock->firstChild() != firstLineBlock || !parentBlock->isBlockFlow())
            break;
        ASSERT(parentBlock->isRenderBlock());
        firstLineBlock = static_cast<RenderBlock*>(parentBlock);
    } 
    
    if (!hasPseudo)
        return 0;
    
    return firstLineBlock;
}

void RenderBlock::updateFirstLetter()
{
    if (!document()->usesFirstLetterRules())
        return;
    // Don't recurse
    if (style()->styleType() == RenderStyle::FIRST_LETTER)
        return;

    // FIXME: We need to destroy the first-letter object if it is no longer the first child.  Need to find
    // an efficient way to check for that situation though before implementing anything.
    RenderObject* firstLetterBlock = this;
    bool hasPseudoStyle = false;
    while (true) {
        // We only honor first-letter if the firstLetterBlock can have children in the DOM. This correctly 
        // prevents form controls from honoring first-letter.
        hasPseudoStyle = firstLetterBlock->style()->hasPseudoStyle(RenderStyle::FIRST_LETTER) 
            && firstLetterBlock->canHaveChildren();
        if (hasPseudoStyle)
            break;
        RenderObject* parentBlock = firstLetterBlock->parent();
        if (firstLetterBlock->isReplaced() || !parentBlock || parentBlock->firstChild() != firstLetterBlock || 
            !parentBlock->isBlockFlow())
            break;
        firstLetterBlock = parentBlock;
    } 

    if (!hasPseudoStyle) 
        return;

    // Drill into inlines looking for our first text child.
    RenderObject* currChild = firstLetterBlock->firstChild();
    while (currChild && currChild->needsLayout() && (!currChild->isReplaced() || currChild->isFloatingOrPositioned()) && !currChild->isText()) {
        if (currChild->isFloatingOrPositioned()) {
            if (currChild->style()->styleType() == RenderStyle::FIRST_LETTER)
                break;
            currChild = currChild->nextSibling();
        } else
            currChild = currChild->firstChild();
    }

    // Get list markers out of the way.
    while (currChild && currChild->isListMarker())
        currChild = currChild->nextSibling();

    if (!currChild)
        return;

    RenderObject* firstLetterContainer = currChild->parent();

    // If the child already has style, then it has already been created, so we just want
    // to update it.
    if (currChild->style()->styleType() == RenderStyle::FIRST_LETTER) {
        RenderStyle* pseudo = firstLetterBlock->getCachedPseudoStyle(RenderStyle::FIRST_LETTER,
                                                                     firstLetterContainer->firstLineStyle());
        currChild->setStyle(pseudo);
        for (RenderObject* genChild = currChild->firstChild(); genChild; genChild = genChild->nextSibling()) {
            if (genChild->isText()) 
                genChild->setStyle(pseudo);
        }
        return;
    }

    // If the child does not already have style, we create it here.
    if (currChild->isText() && !currChild->isBR() && currChild->parent()->style()->styleType() != RenderStyle::FIRST_LETTER) {
        // Our layout state is not valid for the repaints we are going to trigger by
        // adding and removing children of firstLetterContainer.
        view()->disableLayoutState();

        RenderText* textObj = toRenderText(currChild);
        
        // Create our pseudo style now that we have our firstLetterContainer determined.
        RenderStyle* pseudoStyle = firstLetterBlock->getCachedPseudoStyle(RenderStyle::FIRST_LETTER,
                                                                          firstLetterContainer->firstLineStyle());
        
        // Force inline display (except for floating first-letters)
        pseudoStyle->setDisplay( pseudoStyle->isFloating() ? BLOCK : INLINE);
        pseudoStyle->setPosition( StaticPosition ); // CSS2 says first-letter can't be positioned.
        
        RenderObject* firstLetter = RenderFlow::createAnonymousFlow(document(), pseudoStyle); // anonymous box
        firstLetterContainer->addChild(firstLetter, currChild);
        
        // The original string is going to be either a generated content string or a DOM node's
        // string.  We want the original string before it got transformed in case first-letter has
        // no text-transform or a different text-transform applied to it.
        RefPtr<StringImpl> oldText = textObj->originalText();
        ASSERT(oldText);
        
        if (oldText && oldText->length() > 0) {
            unsigned int length = 0;
            
            // account for leading spaces and punctuation
            while (length < oldText->length() && (isSpaceOrNewline((*oldText)[length]) || Unicode::isPunct((*oldText)[length])))
                length++;
            
            // account for first letter
            length++;
            
            // construct text fragment for the text after the first letter
            // NOTE: this might empty
            RenderTextFragment* remainingText = 
                new (renderArena()) RenderTextFragment(textObj->node(), oldText.get(), length, oldText->length() - length);
            remainingText->setStyle(textObj->style());
            if (remainingText->element())
                remainingText->element()->setRenderer(remainingText);
            
            RenderObject* nextObj = textObj->nextSibling();
            firstLetterContainer->removeChild(textObj);
            firstLetterContainer->addChild(remainingText, nextObj);
            remainingText->setFirstLetter(firstLetter);
            
            // construct text fragment for the first letter
            RenderTextFragment* letter = 
                new (renderArena()) RenderTextFragment(remainingText->node(), oldText.get(), 0, length);
            RefPtr<RenderStyle> newStyle = RenderStyle::create();
            newStyle->inheritFrom(pseudoStyle);
            letter->setStyle(newStyle.release());
            firstLetter->addChild(letter);

            textObj->destroy();
        }
        view()->enableLayoutState();
    }
}

bool RenderBlock::inRootBlockContext() const
{
    if (isTableCell() || isFloatingOrPositioned() || hasOverflowClip())
        return false;
    
    if (isRoot() || isRenderView())
        return true;
    
    return containingBlock()->inRootBlockContext();
}

// Helper methods for obtaining the last line, computing line counts and heights for line counts
// (crawling into blocks).
static bool shouldCheckLines(RenderObject* obj)
{
    return !obj->isFloatingOrPositioned() && !obj->isRunIn() &&
            obj->isBlockFlow() && obj->style()->height().isAuto() &&
            (!obj->isFlexibleBox() || obj->style()->boxOrient() == VERTICAL);
}

static RootInlineBox* getLineAtIndex(RenderBlock* block, int i, int& count)
{
    if (block->style()->visibility() == VISIBLE) {
        if (block->childrenInline()) {
            for (RootInlineBox* box = block->firstRootBox(); box; box = box->nextRootBox()) {
                if (count++ == i)
                    return box;
            }
        }
        else {
            for (RenderObject* obj = block->firstChild(); obj; obj = obj->nextSibling()) {
                if (shouldCheckLines(obj)) {
                    RootInlineBox *box = getLineAtIndex(static_cast<RenderBlock*>(obj), i, count);
                    if (box)
                        return box;
                }
            }
        }
    }
    return 0;
}

static int getHeightForLineCount(RenderBlock* block, int l, bool includeBottom, int& count)
{
    if (block->style()->visibility() == VISIBLE) {
        if (block->childrenInline()) {
            for (RootInlineBox* box = block->firstRootBox(); box; box = box->nextRootBox()) {
                if (++count == l)
                    return box->bottomOverflow() + (includeBottom ? (block->borderBottom() + block->paddingBottom()) : 0);
            }
        }
        else {
            RenderBox* normalFlowChildWithoutLines = 0;
            for (RenderBox* obj = block->firstChildBox(); obj; obj = obj->nextSiblingBox()) {
                if (shouldCheckLines(obj)) {
                    int result = getHeightForLineCount(static_cast<RenderBlock*>(obj), l, false, count);
                    if (result != -1)
                        return result + obj->y() + (includeBottom ? (block->borderBottom() + block->paddingBottom()) : 0);
                }
                else if (!obj->isFloatingOrPositioned() && !obj->isRunIn())
                    normalFlowChildWithoutLines = obj;
            }
            if (normalFlowChildWithoutLines && l == 0)
                return normalFlowChildWithoutLines->y() + normalFlowChildWithoutLines->height();
        }
    }
    
    return -1;
}

RootInlineBox* RenderBlock::lineAtIndex(int i)
{
    int count = 0;
    return getLineAtIndex(this, i, count);
}

int RenderBlock::lineCount()
{
    int count = 0;
    if (style()->visibility() == VISIBLE) {
        if (childrenInline())
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox())
                count++;
        else
            for (RenderObject* obj = firstChild(); obj; obj = obj->nextSibling())
                if (shouldCheckLines(obj))
                    count += static_cast<RenderBlock*>(obj)->lineCount();
    }
    return count;
}

int RenderBlock::heightForLineCount(int l)
{
    int count = 0;
    return getHeightForLineCount(this, l, true, count);
}

void RenderBlock::adjustForBorderFit(int x, int& left, int& right) const
{
    // We don't deal with relative positioning.  Our assumption is that you shrink to fit the lines without accounting
    // for either overflow or translations via relative positioning.
    if (style()->visibility() == VISIBLE) {
        if (childrenInline()) {
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox()) {
                if (box->firstChild())
                    left = min(left, x + box->firstChild()->xPos());
                if (box->lastChild())
                    right = max(right, x + box->lastChild()->xPos() + box->lastChild()->width());
            }
        }
        else {
            for (RenderBox* obj = firstChildBox(); obj; obj = obj->nextSiblingBox()) {
                if (!obj->isFloatingOrPositioned()) {
                    if (obj->isBlockFlow() && !obj->hasOverflowClip())
                        static_cast<RenderBlock*>(obj)->adjustForBorderFit(x + obj->x(), left, right);
                    else if (obj->style()->visibility() == VISIBLE) {
                        // We are a replaced element or some kind of non-block-flow object.
                        left = min(left, x + obj->x());
                        right = max(right, x + obj->x() + obj->width());
                    }
                }
            }
        }
        
        if (m_floatingObjects) {
            FloatingObject* r;
            DeprecatedPtrListIterator<FloatingObject> it(*m_floatingObjects);
            for (; (r = it.current()); ++it) {
                // Only examine the object if our m_shouldPaint flag is set.
                if (r->m_shouldPaint) {
                    int floatLeft = r->m_left - r->m_renderer->x() + r->m_renderer->marginLeft();
                    int floatRight = floatLeft + r->m_renderer->width();
                    left = min(left, floatLeft);
                    right = max(right, floatRight);
                }
            }
        }
    }
}

void RenderBlock::borderFitAdjust(int& x, int& w) const
{
    if (style()->borderFit() == BorderFitBorder)
        return;

    // Walk any normal flow lines to snugly fit.
    int left = INT_MAX;
    int right = INT_MIN;
    int oldWidth = w;
    adjustForBorderFit(0, left, right);
    if (left != INT_MAX) {
        left -= (borderLeft() + paddingLeft());
        if (left > 0) {
            x += left;
            w -= left;
        }
    }
    if (right != INT_MIN) {
        right += (borderRight() + paddingRight());
        if (right < oldWidth)
            w -= (oldWidth - right);
    }
}

void RenderBlock::clearTruncation()
{
    if (style()->visibility() == VISIBLE) {
        if (childrenInline() && hasMarkupTruncation()) {
            setHasMarkupTruncation(false);
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox())
                box->clearTruncation();
        }
        else
            for (RenderObject* obj = firstChild(); obj; obj = obj->nextSibling())
                if (shouldCheckLines(obj))
                    static_cast<RenderBlock*>(obj)->clearTruncation();
    }
}

void RenderBlock::setMaxTopMargins(int pos, int neg)
{
    if (!m_maxMargin) {
        if (pos == MaxMargin::topPosDefault(this) && neg == MaxMargin::topNegDefault(this))
            return;
        m_maxMargin = new MaxMargin(this);
    }
    m_maxMargin->m_topPos = pos;
    m_maxMargin->m_topNeg = neg;
}

void RenderBlock::setMaxBottomMargins(int pos, int neg)
{
    if (!m_maxMargin) {
        if (pos == MaxMargin::bottomPosDefault(this) && neg == MaxMargin::bottomNegDefault(this))
            return;
        m_maxMargin = new MaxMargin(this);
    }
    m_maxMargin->m_bottomPos = pos;
    m_maxMargin->m_bottomNeg = neg;
}

const char* RenderBlock::renderName() const
{
    if (isBody())
        return "RenderBody"; // FIXME: Temporary hack until we know that the regression tests pass.
    
    if (isFloating())
        return "RenderBlock (floating)";
    if (isPositioned())
        return "RenderBlock (positioned)";
    if (isAnonymousBlock())
        return "RenderBlock (anonymous)";
    else if (isAnonymous())
        return "RenderBlock (generated)";
    if (isRelPositioned())
        return "RenderBlock (relative positioned)";
    if (isRunIn())
        return "RenderBlock (run-in)";
    return "RenderBlock";
}


#define ParagraphMinWordCount 6
#define ParagraphMinLineCount 2
#define ParagraphMaxWidth     0.9

inline static bool isVisibleRenderText(RenderObject *renderer)
{
    if (!renderer->isText())
        return false;
    RenderText *renderText = toRenderText(renderer);
    IntRect boundingBox = renderText->linesBoundingBox();
    return boundingBox.width() != 0 && boundingBox.height() != 0 && !renderText->text()->containsOnlyWhitespace();
}

inline static bool resizeTextPermitted(RenderObject *render) {
    // We disallow resizing for text input fields to address <rdar://problem/5792987>
    RenderObject* renderer = render->parent();
    while (renderer) {
        // Get the first non-shadow HTMLElement and see if it's an input.
        if (renderer->node() && renderer->node()->isHTMLElement() && !renderer->node()->isShadowNode())
            return !static_cast<HTMLElement*>(renderer->node())->hasTagName(inputTag);
        renderer = renderer->parent();
    }
    
    return true;
}

int RenderBlock::immediateLineCount()
{
    // Copied and modified from RenderBlock::lineCount.
    // Only descend into list items.
    int count = 0;
    if (style()->visibility() == VISIBLE) {
        if (childrenInline())
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox())
                count++;
        else
            for (RenderObject* obj = firstChild(); obj; obj = obj->nextSibling())
                if (obj->isListItem())
                    count += static_cast<RenderBlock*>(obj)->lineCount();
    }
    return count;
}

static bool includeNonBlocksOrListItems(const RenderObject *render)
{
    return !render->isRenderBlock() || render->isListItem();
}

//  For now, we auto size single lines of text the same as multiple lines.
//  We've been experimenting with low values for single lines of text.
#define ONE_LINE_TEXT_MULTIPLIER   (MAX((1.0f/log10f(specifiedSize) * 1.7f),1.0f))
#define TEXT_MULTIPLIER   (MAX((1.0f/log10f(specifiedSize) * 1.95f),1.0f))

void RenderBlock::adjustComputedFontSizes(float size, float visibleWidth)
{
    // Don't do any work if the block is smaller than the visible area.
    if (visibleWidth >= width())
        return;
    
    unsigned lineCount;
    if (m_lineCountForTextAutosizing == NOT_SET) {
        int count = immediateLineCount();
        if (count == 0)
            lineCount = NO_LINE;
        else if (count == 1)
            lineCount = ONE_LINE;
        else
            lineCount = MULTI_LINE;
    } else {
        lineCount = m_lineCountForTextAutosizing;
    }
    
    ASSERT(lineCount != NOT_SET);
    if (lineCount == NO_LINE)
        return;
    
    float actualWidth = m_widthForTextAutosizing != -1 ? static_cast<float>(m_widthForTextAutosizing) : static_cast<float>(width());
    float scale = visibleWidth / actualWidth;
    float minFontSize = roundf(size / scale);   

    for (RenderObject *descendent = traverseNext(this, includeNonBlocksOrListItems); descendent; descendent = descendent->traverseNext(this, includeNonBlocksOrListItems)) {
        if (isVisibleRenderText(descendent) && resizeTextPermitted(descendent)) {
            RenderText *text = static_cast<RenderText *>(descendent);
            RenderStyle *oldStyle = text->style();
            FontDescription fontDescription = oldStyle->fontDescription();
            float specifiedSize = fontDescription.specifiedSize();
            float scaledSize = roundf(specifiedSize * scale);
            if (scaledSize > 0 && scaledSize < minFontSize) {
                // Record the width of the block and the line count the first time we resize text and use it from then on for text resizing.
                // This makes text resizing consistent even if the block's width or line count changes (which can be caused by text resizing itself 5159915).
                if (m_lineCountForTextAutosizing == NOT_SET)
                    m_lineCountForTextAutosizing = lineCount;
                if (m_widthForTextAutosizing == -1)
                    m_widthForTextAutosizing = actualWidth;
                
                float candidateNewSize = 0;
                if (lineCount == ONE_LINE)
                    candidateNewSize = roundf(MIN(minFontSize, specifiedSize * ONE_LINE_TEXT_MULTIPLIER));
                else
                    candidateNewSize = roundf(MIN(minFontSize, specifiedSize * TEXT_MULTIPLIER));
                if (candidateNewSize > specifiedSize && candidateNewSize != fontDescription.computedSize() && text->element() && (!oldStyle || oldStyle->textSizeAdjust().isAuto()))
                    document()->addAutoSizingNode (text->element(), candidateNewSize);
            }
        }
    }
}

} // namespace WebCore
