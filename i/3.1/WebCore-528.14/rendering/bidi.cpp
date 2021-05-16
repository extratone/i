/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All right reserved.
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
#include "bidi.h"

#include "CharacterNames.h"
#include "Document.h"
#include "Element.h"
#include "FrameView.h"
#include "InlineTextBox.h"
#include "Logging.h"
#include "RenderArena.h"
#include "RenderLayer.h"
#include "RenderListMarker.h"
#include "RenderView.h"
#include "break_lines.h"
#include <wtf/AlwaysInline.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

using namespace std;
using namespace WTF;
using namespace Unicode;

namespace WebCore {

// We don't let our line box tree for a single line get any deeper than this.
const unsigned cMaxLineDepth = 200;

class InlineIterator {
public:
    InlineIterator()
        : block(0)
        , obj(0)
        , pos(0)
        , nextBreakablePosition(-1)
    {
    }

    InlineIterator(RenderBlock* b, RenderObject* o, unsigned p)
        : block(b)
        , obj(o)
        , pos(p)
        , nextBreakablePosition(-1)
    {
    }

    void increment(InlineBidiResolver* resolver = 0);
    bool atEnd() const;

    UChar current() const;
    WTF::Unicode::Direction direction() const;

    RenderBlock* block;
    RenderObject* obj;
    unsigned pos;
    int nextBreakablePosition;
};

// Midpoint globals.  The goal is not to do any allocation when dealing with
// these midpoints, so we just keep an array around and never clear it.  We track
// the number of items and position using the two other variables.
static Vector<InlineIterator>* smidpoints;
static unsigned sNumMidpoints;
static unsigned sCurrMidpoint;
static bool betweenMidpoints;

static bool isLineEmpty = true;
static bool previousLineBrokeCleanly = true;

static int getBorderPaddingMargin(RenderBox* child, bool endOfInline)
{
    bool leftSide = (child->style()->direction() == LTR) ? !endOfInline : endOfInline;
    if (leftSide)
        return child->marginLeft() + child->paddingLeft() + child->borderLeft();
    return child->marginRight() + child->paddingRight() + child->borderRight();
}

static int inlineWidth(RenderObject* child, bool start = true, bool end = true)
{
    unsigned lineDepth = 1;
    int extraWidth = 0;
    RenderObject* parent = child->parent();
    while (parent->isBox() && parent->isInline() && !parent->isInlineBlockOrInlineTable() && lineDepth++ < cMaxLineDepth) {
        if (start && parent->firstChild() == child)
            extraWidth += getBorderPaddingMargin(toRenderBox(parent), false);
        if (end && parent->lastChild() == child)
            extraWidth += getBorderPaddingMargin(toRenderBox(parent), true);
        child = parent;
        parent = child->parent();
    }
    return extraWidth;
}

#ifndef NDEBUG
static WTF::RefCountedLeakCounter bidiRunCounter("BidiRun");

static bool inBidiRunDestroy;
#endif

void BidiRun::destroy()
{
#ifndef NDEBUG
    inBidiRunDestroy = true;
#endif
    RenderArena* renderArena = m_object->renderArena();
    delete this;
#ifndef NDEBUG
    inBidiRunDestroy = false;
#endif

    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*reinterpret_cast<size_t*>(this), this);
}

void* BidiRun::operator new(size_t sz, RenderArena* renderArena) throw()
{
#ifndef NDEBUG
    bidiRunCounter.increment();
#endif
    return renderArena->allocate(sz);
}

void BidiRun::operator delete(void* ptr, size_t sz)
{
#ifndef NDEBUG
    bidiRunCounter.decrement();
#endif
    ASSERT(inBidiRunDestroy);

    // Stash size where destroy() can find it.
    *(size_t*)ptr = sz;
}

// ---------------------------------------------------------------------

inline bool operator==(const InlineIterator& it1, const InlineIterator& it2)
{
    return it1.pos == it2.pos && it1.obj == it2.obj;
}

inline bool operator!=(const InlineIterator& it1, const InlineIterator& it2)
{
    return it1.pos != it2.pos || it1.obj != it2.obj;
}

static inline RenderObject* bidiNext(RenderBlock* block, RenderObject* current, InlineBidiResolver* resolver = 0, bool skipInlines = true, bool* endOfInlinePtr = 0)
{
    RenderObject* next = 0;
    bool oldEndOfInline = endOfInlinePtr ? *endOfInlinePtr : false;
    bool endOfInline = false;

    while (current) {
        next = 0;
        if (!oldEndOfInline && !current->isFloating() && !current->isReplaced() && !current->isPositioned()) {
            next = current->firstChild();
            if (next && resolver && next->isRenderInline()) {
                EUnicodeBidi ub = next->style()->unicodeBidi();
                if (ub != UBNormal) {
                    TextDirection dir = next->style()->direction();
                    Direction d = (ub == Embed
                        ? (dir == RTL ? RightToLeftEmbedding : LeftToRightEmbedding)
                        : (dir == RTL ? RightToLeftOverride : LeftToRightOverride));
                    resolver->embed(d);
                }
            }
        }

        if (!next) {
            if (!skipInlines && !oldEndOfInline && current->isRenderInline()) {
                next = current;
                endOfInline = true;
                break;
            }

            while (current && current != block) {
                if (resolver && current->isRenderInline() && current->style()->unicodeBidi() != UBNormal)
                    resolver->embed(PopDirectionalFormat);

                next = current->nextSibling();
                if (next) {
                    if (resolver && next->isRenderInline()) {
                        EUnicodeBidi ub = next->style()->unicodeBidi();
                        if (ub != UBNormal) {
                            TextDirection dir = next->style()->direction();
                            Direction d = (ub == Embed
                                ? (dir == RTL ? RightToLeftEmbedding: LeftToRightEmbedding)
                                : (dir == RTL ? RightToLeftOverride : LeftToRightOverride));
                            resolver->embed(d);
                        }
                    }
                    break;
                }
                
                current = current->parent();
                if (!skipInlines && current && current != block && current->isRenderInline()) {
                    next = current;
                    endOfInline = true;
                    break;
                }
            }
        }

        if (!next)
            break;

        if (next->isText() || next->isFloating() || next->isReplaced() || next->isPositioned()
            || ((!skipInlines || !next->firstChild()) // Always return EMPTY inlines.
                && next->isRenderInline()))
            break;
        current = next;
    }

    if (endOfInlinePtr)
        *endOfInlinePtr = endOfInline;

    return next;
}

static RenderObject* bidiFirst(RenderBlock* block, InlineBidiResolver* resolver, bool skipInlines = true)
{
    if (!block->firstChild())
        return 0;
    
    RenderObject* o = block->firstChild();
    if (o->isRenderInline()) {
        if (resolver) {
            EUnicodeBidi ub = o->style()->unicodeBidi();
            if (ub != UBNormal) {
                TextDirection dir = o->style()->direction();
                Direction d = (ub == Embed
                    ? (dir == RTL ? RightToLeftEmbedding : LeftToRightEmbedding)
                    : (dir == RTL ? RightToLeftOverride : LeftToRightOverride));
                resolver->embed(d);
            }
        }
        if (skipInlines && o->firstChild())
            o = bidiNext(block, o, resolver, skipInlines);
        else {
            // Never skip empty inlines.
            if (resolver)
                resolver->commitExplicitEmbedding();
            return o; 
        }
    }

    if (o && !o->isText() && !o->isReplaced() && !o->isFloating() && !o->isPositioned())
        o = bidiNext(block, o, resolver, skipInlines);

    if (resolver)
        resolver->commitExplicitEmbedding();
    return o;
}

inline void InlineIterator::increment(InlineBidiResolver* resolver)
{
    if (!obj)
        return;
    if (obj->isText()) {
        pos++;
        if (pos >= toRenderText(obj)->textLength()) {
            obj = bidiNext(block, obj, resolver);
            pos = 0;
            nextBreakablePosition = -1;
        }
    } else {
        obj = bidiNext(block, obj, resolver);
        pos = 0;
        nextBreakablePosition = -1;
    }
}

template<>
inline void InlineBidiResolver::increment()
{
    current.increment(this);
}

inline bool InlineIterator::atEnd() const
{
    return !obj;
}

inline UChar InlineIterator::current() const
{
    if (!obj || !obj->isText())
        return 0;

    RenderText* text = toRenderText(obj);
    if (pos >= text->textLength())
        return 0;

    return text->characters()[pos];
}

ALWAYS_INLINE Direction InlineIterator::direction() const
{
    if (UChar c = current())
        return Unicode::direction(c);

    if (obj && obj->isListMarker())
        return obj->style()->direction() == LTR ? LeftToRight : RightToLeft;

    return OtherNeutral;
}

// -------------------------------------------------------------------------------------------------

static void chopMidpointsAt(RenderObject* obj, unsigned pos)
{
    if (!sNumMidpoints)
        return;
    InlineIterator* midpoints = smidpoints->data();
    for (int i = sNumMidpoints - 1; i >= 0; i--) {
        const InlineIterator& point = midpoints[i];
        if (point.obj == obj && point.pos == pos) {
            sNumMidpoints = i;
            break;
        }
    }
}

static void checkMidpoints(InlineIterator& lBreak)
{
    // Check to see if our last midpoint is a start point beyond the line break.  If so,
    // shave it off the list, and shave off a trailing space if the previous end point doesn't
    // preserve whitespace.
    if (lBreak.obj && sNumMidpoints && sNumMidpoints % 2 == 0) {
        InlineIterator* midpoints = smidpoints->data();
        InlineIterator& endpoint = midpoints[sNumMidpoints-2];
        const InlineIterator& startpoint = midpoints[sNumMidpoints-1];
        InlineIterator currpoint = endpoint;
        while (!currpoint.atEnd() && currpoint != startpoint && currpoint != lBreak)
            currpoint.increment();
        if (currpoint == lBreak) {
            // We hit the line break before the start point.  Shave off the start point.
            sNumMidpoints--;
            if (endpoint.obj->style()->collapseWhiteSpace()) {
                if (endpoint.obj->isText()) {
                    // Don't shave a character off the endpoint if it was from a soft hyphen.
                    RenderText* textObj = toRenderText(endpoint.obj);
                    if (endpoint.pos + 1 < textObj->textLength()) {
                        if (textObj->characters()[endpoint.pos+1] == softHyphen)
                            return;
                    } else if (startpoint.obj->isText()) {
                        RenderText *startText = toRenderText(startpoint.obj);
                        if (startText->textLength() && startText->characters()[0] == softHyphen)
                            return;
                    }
                }
                endpoint.pos--;
            }
        }
    }    
}

static void addMidpoint(const InlineIterator& midpoint)
{
    if (smidpoints->size() <= sNumMidpoints)
        smidpoints->grow(sNumMidpoints + 10);

    InlineIterator* midpoints = smidpoints->data();
    midpoints[sNumMidpoints++] = midpoint;
}

static void appendRunsForObject(int start, int end, RenderObject* obj, InlineBidiResolver& resolver)
{
    if (start > end || obj->isFloating() ||
        (obj->isPositioned() && !obj->hasStaticX() && !obj->hasStaticY() && !obj->container()->isRenderInline()))
        return;

    bool haveNextMidpoint = (sCurrMidpoint < sNumMidpoints);
    InlineIterator nextMidpoint;
    if (haveNextMidpoint)
        nextMidpoint = smidpoints->at(sCurrMidpoint);
    if (betweenMidpoints) {
        if (!(haveNextMidpoint && nextMidpoint.obj == obj))
            return;
        // This is a new start point. Stop ignoring objects and 
        // adjust our start.
        betweenMidpoints = false;
        start = nextMidpoint.pos;
        sCurrMidpoint++;
        if (start < end)
            return appendRunsForObject(start, end, obj, resolver);
    } else {
        if (!haveNextMidpoint || (obj != nextMidpoint.obj)) {
            resolver.addRun(new (obj->renderArena()) BidiRun(start, end, obj, resolver.context(), resolver.dir()));
            return;
        }

        // An end midpoint has been encountered within our object.  We
        // need to go ahead and append a run with our endpoint.
        if (static_cast<int>(nextMidpoint.pos + 1) <= end) {
            betweenMidpoints = true;
            sCurrMidpoint++;
            if (nextMidpoint.pos != UINT_MAX) { // UINT_MAX means stop at the object and don't include any of it.
                if (static_cast<int>(nextMidpoint.pos + 1) > start)
                    resolver.addRun(new (obj->renderArena())
                        BidiRun(start, nextMidpoint.pos + 1, obj, resolver.context(), resolver.dir()));
                return appendRunsForObject(nextMidpoint.pos + 1, end, obj, resolver);
            }
        } else
           resolver.addRun(new (obj->renderArena()) BidiRun(start, end, obj, resolver.context(), resolver.dir()));
    }
}

template <>
void InlineBidiResolver::appendRun()
{
    if (!emptyRun && !eor.atEnd()) {
        int start = sor.pos;
        RenderObject *obj = sor.obj;
        while (obj && obj != eor.obj && obj != endOfLine.obj) {
            appendRunsForObject(start, obj->length(), obj, *this);        
            start = 0;
            obj = bidiNext(sor.block, obj);
        }
        if (obj) {
            unsigned pos = obj == eor.obj ? eor.pos : UINT_MAX;
            if (obj == endOfLine.obj && endOfLine.pos <= pos) {
                reachedEndOfLine = true;
                pos = endOfLine.pos;
            }
            // It's OK to add runs for zero-length RenderObjects, just don't make the run larger than it should be
            int end = obj->length() ? pos+1 : 0;
            appendRunsForObject(start, end, obj, *this);
        }
        
        eor.increment();
        sor = eor;
    }

    m_direction = OtherNeutral;
    m_status.eor = OtherNeutral;
}

InlineFlowBox* RenderBlock::createLineBoxes(RenderObject* obj)
{
    // See if we have an unconstructed line box for this object that is also
    // the last item on the line.
    unsigned lineDepth = 1;
    InlineFlowBox* childBox = 0;
    InlineFlowBox* parentBox = 0;
    InlineFlowBox* result = 0;
    do {
        ASSERT(obj->isRenderInline() || obj == this);
        RenderFlow* flow = static_cast<RenderFlow*>(obj);

        // Get the last box we made for this render object.
        parentBox = flow->lastLineBox();

        // If this box is constructed then it is from a previous line, and we need
        // to make a new box for our line.  If this box is unconstructed but it has
        // something following it on the line, then we know we have to make a new box
        // as well.  In this situation our inline has actually been split in two on
        // the same line (this can happen with very fancy language mixtures).
        bool constructedNewBox = false;
        if (!parentBox || parentBox->isConstructed() || parentBox->nextOnLine()) {
            // We need to make a new box for this render object.  Once
            // made, we need to place it at the end of the current line.
            InlineBox* newBox = obj->createInlineBox(false, obj == this);
            ASSERT(newBox->isInlineFlowBox());
            parentBox = static_cast<InlineFlowBox*>(newBox);
            parentBox->setFirstLineStyleBit(m_firstLine);
            constructedNewBox = true;
        }

        if (!result)
            result = parentBox;

        // If we have hit the block itself, then |box| represents the root
        // inline box for the line, and it doesn't have to be appended to any parent
        // inline.
        if (childBox)
            parentBox->addToLine(childBox);

        if (!constructedNewBox || obj == this)
            break;

        childBox = parentBox;        

        // If we've exceeded our line depth, then jump straight to the root and skip all the remaining
        // intermediate inline flows.
        obj = (++lineDepth >= cMaxLineDepth) ? this : obj->parent();

    } while (true);

    return result;
}

RootInlineBox* RenderBlock::constructLine(unsigned runCount, BidiRun* firstRun, BidiRun* lastRun, bool lastLine, RenderObject* endObject)
{
    ASSERT(firstRun);

    InlineFlowBox* parentBox = 0;
    for (BidiRun* r = firstRun; r; r = r->next()) {
        // Create a box for our object.
        bool isOnlyRun = (runCount == 1);
        if (runCount == 2 && !r->m_object->isListMarker())
            isOnlyRun = ((style()->direction() == RTL) ? lastRun : firstRun)->m_object->isListMarker();

        InlineBox* box = r->m_object->createInlineBox(r->m_object->isPositioned(), false, isOnlyRun);
        r->m_box = box;

        if (box) {
            // If we have no parent box yet, or if the run is not simply a sibling,
            // then we need to construct inline boxes as necessary to properly enclose the
            // run's inline box.
            if (!parentBox || parentBox->object() != r->m_object->parent())
                // Create new inline boxes all the way back to the appropriate insertion point.
                parentBox = createLineBoxes(r->m_object->parent());

            // Append the inline box to this line.
            parentBox->addToLine(box);

            bool visuallyOrdered = r->m_object->style()->visuallyOrdered();
            box->setBidiLevel(visuallyOrdered ? 0 : r->level());

            if (box->isInlineTextBox()) {
                InlineTextBox* text = static_cast<InlineTextBox*>(box);
                text->setStart(r->m_start);
                text->setLen(r->m_stop - r->m_start);
                text->m_dirOverride = r->dirOverride(visuallyOrdered);
            }
        }
    }

    // We should have a root inline box.  It should be unconstructed and
    // be the last continuation of our line list.
    ASSERT(lastLineBox() && !lastLineBox()->isConstructed());

    // Set bits on our inline flow boxes that indicate which sides should
    // paint borders/margins/padding.  This knowledge will ultimately be used when
    // we determine the horizontal positions and widths of all the inline boxes on
    // the line.
    lastLineBox()->determineSpacingForFlowBoxes(lastLine, endObject);

    // Now mark the line boxes as being constructed.
    lastLineBox()->setConstructed();

    // Return the last line.
    return lastRootBox();
}

void RenderBlock::computeHorizontalPositionsForLine(RootInlineBox* lineBox, BidiRun* firstRun, BidiRun* trailingSpaceRun, bool reachedEnd)
{
    // First determine our total width.
    int availableWidth = lineWidth(height());
    int totWidth = lineBox->getFlowSpacingWidth();
    bool needsWordSpacing = false;
    unsigned numSpaces = 0;
    ETextAlign textAlign = style()->textAlign();

    for (BidiRun* r = firstRun; r; r = r->next()) {
        if (!r->m_box || r->m_object->isPositioned() || r->m_box->isLineBreak())
            continue; // Positioned objects are only participating to figure out their
                      // correct static x position.  They have no effect on the width.
                      // Similarly, line break boxes have no effect on the width.
        if (r->m_object->isText()) {
            RenderText* rt = toRenderText(r->m_object);

            if (textAlign == JUSTIFY && r != trailingSpaceRun) {
                const UChar* characters = rt->characters();
                for (int i = r->m_start; i < r->m_stop; i++) {
                    UChar c = characters[i];
                    if (c == ' ' || c == '\n' || c == '\t')
                        numSpaces++;
                }
            }

            if (int length = rt->textLength()) {
                if (!r->m_start && needsWordSpacing && isSpaceOrNewline(rt->characters()[r->m_start]))
                    totWidth += rt->style(m_firstLine)->font().wordSpacing();
                needsWordSpacing = !isSpaceOrNewline(rt->characters()[r->m_stop - 1]) && r->m_stop == length;          
            }
            r->m_box->setWidth(rt->width(r->m_start, r->m_stop - r->m_start, totWidth, m_firstLine));
        } else if (!r->m_object->isRenderInline()) {
            RenderBox* renderBox = toRenderBox(r->m_object);
            renderBox->calcWidth();
            r->m_box->setWidth(renderBox->width());
            totWidth += renderBox->marginLeft() + renderBox->marginRight();
        }

        totWidth += r->m_box->width();
    }

    // Armed with the total width of the line (without justification),
    // we now examine our text-align property in order to determine where to position the
    // objects horizontally.  The total width of the line can be increased if we end up
    // justifying text.
    int x = leftOffset(height());
    switch(textAlign) {
        case LEFT:
        case WEBKIT_LEFT:
            // The direction of the block should determine what happens with wide lines.  In
            // particular with RTL blocks, wide lines should still spill out to the left.
            if (style()->direction() == LTR) {
                if (totWidth > availableWidth && trailingSpaceRun)
                    trailingSpaceRun->m_box->setWidth(max(0, trailingSpaceRun->m_box->width() - totWidth + availableWidth));
            } else {
                if (trailingSpaceRun)
                    trailingSpaceRun->m_box->setWidth(0);
                else if (totWidth > availableWidth)
                    x -= (totWidth - availableWidth);
            }
            break;
        case JUSTIFY:
            if (numSpaces && !reachedEnd && !lineBox->endsWithBreak()) {
                if (trailingSpaceRun) {
                    totWidth -= trailingSpaceRun->m_box->width();
                    trailingSpaceRun->m_box->setWidth(0);
                }
                break;
            }
            // fall through
        case TAAUTO:
            numSpaces = 0;
            // for right to left fall through to right aligned
            if (style()->direction() == LTR) {
                if (totWidth > availableWidth && trailingSpaceRun)
                    trailingSpaceRun->m_box->setWidth(max(0, trailingSpaceRun->m_box->width() - totWidth + availableWidth));
                break;
            }
        case RIGHT:
        case WEBKIT_RIGHT:
            // Wide lines spill out of the block based off direction.
            // So even if text-align is right, if direction is LTR, wide lines should overflow out of the right
            // side of the block.
            if (style()->direction() == LTR) {
                if (trailingSpaceRun) {
                    totWidth -= trailingSpaceRun->m_box->width();
                    trailingSpaceRun->m_box->setWidth(0);
                }
                if (totWidth < availableWidth)
                    x += availableWidth - totWidth;
            } else {
                if (totWidth > availableWidth && trailingSpaceRun) {
                    trailingSpaceRun->m_box->setWidth(max(0, trailingSpaceRun->m_box->width() - totWidth + availableWidth));
                    totWidth -= trailingSpaceRun->m_box->width();
                } else
                    x += availableWidth - totWidth;
            }
            break;
        case CENTER:
        case WEBKIT_CENTER:
            int trailingSpaceWidth = 0;
            if (trailingSpaceRun) {
                totWidth -= trailingSpaceRun->m_box->width();
                trailingSpaceWidth = min(trailingSpaceRun->m_box->width(), (availableWidth - totWidth + 1) / 2);
                trailingSpaceRun->m_box->setWidth(max(0, trailingSpaceWidth));
            }
            if (style()->direction() == LTR)
                x += max((availableWidth - totWidth) / 2, 0);
            else
                x += totWidth > availableWidth ? (availableWidth - totWidth) : (availableWidth - totWidth) / 2 - trailingSpaceWidth;
            break;
    }

    if (numSpaces) {
        for (BidiRun* r = firstRun; r; r = r->next()) {
            if (!r->m_box || r == trailingSpaceRun)
                continue;

            int spaceAdd = 0;
            if (r->m_object->isText()) {
                unsigned spaces = 0;
                const UChar* characters = toRenderText(r->m_object)->characters();
                for (int i = r->m_start; i < r->m_stop; i++) {
                    UChar c = characters[i];
                    if (c == ' ' || c == '\n' || c == '\t')
                        spaces++;
                }

                ASSERT(spaces <= numSpaces);

                // Only justify text if whitespace is collapsed.
                if (r->m_object->style()->collapseWhiteSpace()) {
                    spaceAdd = (availableWidth - totWidth) * spaces / numSpaces;
                    static_cast<InlineTextBox*>(r->m_box)->setSpaceAdd(spaceAdd);
                    totWidth += spaceAdd;
                }
                numSpaces -= spaces;
                if (!numSpaces)
                    break;
            }
        }
    }

    // The widths of all runs are now known.  We can now place every inline box (and
    // compute accurate widths for the inline flow boxes).
    int leftPosition = x;
    int rightPosition = x;
    needsWordSpacing = false;
    lineBox->placeBoxesHorizontally(x, leftPosition, rightPosition, needsWordSpacing);
    lineBox->setHorizontalOverflowPositions(leftPosition, rightPosition);
}

void RenderBlock::computeVerticalPositionsForLine(RootInlineBox* lineBox, BidiRun* firstRun)
{
    setHeight(lineBox->verticallyAlignBoxes(height()));
    lineBox->setBlockHeight(height());

    // See if the line spilled out.  If so set overflow height accordingly.
    int bottomOfLine = lineBox->bottomOverflow();
    if (bottomOfLine > height() && bottomOfLine > m_overflowHeight)
        m_overflowHeight = bottomOfLine;

    // Now make sure we place replaced render objects correctly.
    for (BidiRun* r = firstRun; r; r = r->next()) {
        if (!r->m_box)
            continue; // Skip runs with no line boxes.

        // Align positioned boxes with the top of the line box.  This is
        // a reasonable approximation of an appropriate y position.
        if (r->m_object->isPositioned())
            r->m_box->setYPos(height());

        // Position is used to properly position both replaced elements and
        // to update the static normal flow x/y of positioned elements.
        r->m_object->position(r->m_box);
    }
    // Positioned objects and zero-length text nodes destroy their boxes in
    // position(), which unnecessarily dirties the line.
    lineBox->markDirty(false);
}

// collects one line of the paragraph and transforms it to visual order
void RenderBlock::bidiReorderLine(InlineBidiResolver& resolver, const InlineIterator& end)
{
    resolver.createBidiRunsForLine(end, style()->visuallyOrdered(), previousLineBrokeCleanly);
}

static inline bool isCollapsibleSpace(UChar character, RenderText* renderer)
{
    if (character == ' ' || character == '\t' || character == softHyphen)
        return true;
    if (character == '\n')
        return !renderer->style()->preserveNewline();
    if (character == noBreakSpace)
        return renderer->style()->nbspMode() == SPACE;
    return false;
}

void RenderBlock::layoutInlineChildren(bool relayoutChildren, int& repaintTop, int& repaintBottom)
{
    bool useRepaintBounds = false;

    invalidateVerticalPosition();
    
    m_overflowHeight = 0;
        
    setHeight(borderTop() + paddingTop());
    int toAdd = borderBottom() + paddingBottom() + horizontalScrollbarHeight();

    // Figure out if we should clear out our line boxes.
    // FIXME: Handle resize eventually!
    // FIXME: Do something better when floats are present.
    bool fullLayout = !firstLineBox() || !firstChild() || selfNeedsLayout() || relayoutChildren;
    if (fullLayout)
        deleteLineBoxes();

    // Text truncation only kicks in if your overflow isn't visible and your text-overflow-mode isn't
    // clip.
    // FIXME: CSS3 says that descendants that are clipped must also know how to truncate.  This is insanely
    // difficult to figure out (especially in the middle of doing layout), and is really an esoteric pile of nonsense
    // anyway, so we won't worry about following the draft here.
    bool hasTextOverflow = style()->textOverflow() && hasOverflowClip();

    // Walk all the lines and delete our ellipsis line boxes if they exist.
    if (hasTextOverflow)
         deleteEllipsisLineBoxes();

    if (firstChild()) {
        // layout replaced elements
        bool endOfInline = false;
        RenderObject* o = bidiFirst(this, 0, false);
        Vector<FloatWithRect> floats;
        int containerWidth = max(0, containingBlockWidth());
        while (o) {
            o->invalidateVerticalPosition();
            if (o->isReplaced() || o->isFloating() || o->isPositioned()) {
                RenderBox* box = toRenderBox(o);
                
                if (relayoutChildren || o->style()->width().isPercent() || o->style()->height().isPercent())
                    o->setChildNeedsLayout(true, false);
                    
                // If relayoutChildren is set and we have percentage padding, we also need to invalidate the child's pref widths.
                if (relayoutChildren && (o->style()->paddingLeft().isPercent() || o->style()->paddingRight().isPercent()))
                    o->setPrefWidthsDirty(true, false);
            
                if (o->isPositioned())
                    o->containingBlock()->insertPositionedObject(box);
                else {
                    if (o->isFloating())
                        floats.append(FloatWithRect(box));
                    else if (fullLayout || o->needsLayout()) // Replaced elements
                        o->dirtyLineBoxes(fullLayout);

                    o->layoutIfNeeded();
                }
            } else if (o->isText() || (o->isRenderInline() && !endOfInline)) {
                if (fullLayout || o->selfNeedsLayout())
                    o->dirtyLineBoxes(fullLayout);
                
                // Calculate margins of inline flows so that they can be used later by line layout.
                if (o->isRenderInline())
                    static_cast<RenderFlow*>(o)->calcMargins(containerWidth);
                o->setNeedsLayout(false);
            }
            o = bidiNext(this, o, 0, false, &endOfInline);
        }

        // We want to skip ahead to the first dirty line
        InlineBidiResolver resolver;
        unsigned floatIndex;
        RootInlineBox* startLine = determineStartPosition(fullLayout, resolver, floats, floatIndex);

        if (fullLayout && !selfNeedsLayout()) {
            setNeedsLayout(true, false);  // Mark ourselves as needing a full layout. This way we'll repaint like
                                          // we're supposed to.
            RenderView* v = view();
            if (v && !v->doingFullRepaint() && m_layer) {
                // Because we waited until we were already inside layout to discover
                // that the block really needed a full layout, we missed our chance to repaint the layer
                // before layout started.  Luckily the layer has cached the repaint rect for its original
                // position and size, and so we can use that to make a repaint happen now.
                repaintUsingContainer(containerForRepaint(), m_layer->repaintRect());
            }
        }

        FloatingObject* lastFloat = m_floatingObjects ? m_floatingObjects->last() : 0;

        if (!smidpoints)
            smidpoints = new Vector<InlineIterator>();

        sNumMidpoints = 0;
        sCurrMidpoint = 0;

        // We also find the first clean line and extract these lines.  We will add them back
        // if we determine that we're able to synchronize after handling all our dirty lines.
        InlineIterator cleanLineStart;
        BidiStatus cleanLineBidiStatus;
        int endLineYPos = 0;
        RootInlineBox* endLine = (fullLayout || !startLine) ? 
                                 0 : determineEndPosition(startLine, cleanLineStart, cleanLineBidiStatus, endLineYPos);

        if (startLine) {
            useRepaintBounds = true;
            repaintTop = height();
            repaintBottom = height();
            RenderArena* arena = renderArena();
            RootInlineBox* box = startLine;
            while (box) {
                repaintTop = min(repaintTop, box->topOverflow());
                repaintBottom = max(repaintBottom, box->bottomOverflow());
                RootInlineBox* next = box->nextRootBox();
                box->deleteLine(arena);
                box = next;
            }
        }

        InlineIterator end = resolver.position();

        if (!fullLayout && lastRootBox() && lastRootBox()->endsWithBreak()) {
            // If the last line before the start line ends with a line break that clear floats,
            // adjust the height accordingly.
            // A line break can be either the first or the last object on a line, depending on its direction.
            if (InlineBox* lastLeafChild = lastRootBox()->lastLeafChild()) {
                RenderObject* lastObject = lastLeafChild->object();
                if (!lastObject->isBR())
                    lastObject = lastRootBox()->firstLeafChild()->object();
                if (lastObject->isBR()) {
                    EClear clear = lastObject->style()->clear();
                    if (clear != CNONE)
                        newLine(clear);
                }
            }
        }

        bool endLineMatched = false;
        bool checkForEndLineMatch = endLine;
        bool checkForFloatsFromLastLine = false;
        int lastHeight = height();

        while (!end.atEnd()) {
            // FIXME: Is this check necessary before the first iteration or can it be moved to the end?
            if (checkForEndLineMatch && (endLineMatched = matchedEndLine(resolver, cleanLineStart, cleanLineBidiStatus, endLine, endLineYPos, repaintBottom, repaintTop)))
                break;

            betweenMidpoints = false;
            isLineEmpty = true;
            
            EClear clear = CNONE;
            end = findNextLineBreak(resolver, &clear);
            if (resolver.position().atEnd()) {
                resolver.deleteRuns();
                checkForFloatsFromLastLine = true;
                break;
            }
            ASSERT(end != resolver.position());

            if (!isLineEmpty) {
                bidiReorderLine(resolver, end);
                ASSERT(resolver.position() == end);

                BidiRun* trailingSpaceRun = 0;
                if (!previousLineBrokeCleanly && resolver.runCount() && resolver.logicallyLastRun()->m_object->style()->breakOnlyAfterWhiteSpace()
                        && resolver.logicallyLastRun()->m_object->style()->autoWrap()) {
                    trailingSpaceRun = resolver.logicallyLastRun();
                    RenderObject* lastObject = trailingSpaceRun->m_object;
                    if (lastObject->isText()) {
                        RenderText* lastText = toRenderText(lastObject);
                        const UChar* characters = lastText->characters();
                        int firstSpace = trailingSpaceRun->stop();
                        while (firstSpace > trailingSpaceRun->start()) {
                            UChar current = characters[firstSpace - 1];
                            if (!isCollapsibleSpace(current, lastText))
                                break;
                            firstSpace--;
                        }
                        if (firstSpace == trailingSpaceRun->stop())
                            trailingSpaceRun = 0;
                        else {
                            TextDirection direction = style()->direction();
                            bool shouldReorder = trailingSpaceRun != (direction == LTR ? resolver.lastRun() : resolver.firstRun());
                            if (firstSpace != trailingSpaceRun->start()) {
                                ETextAlign textAlign = style()->textAlign();
                                // If the trailing white space is at the right hand side of a left-aligned line, then computeHorizontalPositionsForLine()
                                // does not care if trailingSpaceRun includes non-spaces at the beginning. In all other cases, trailingSpaceRun has to
                                // contain only the spaces, either because we re-order them or because computeHorizontalPositionsForLine() needs to know
                                // their width.
                                bool shouldSeparateSpaces = textAlign != LEFT && textAlign != WEBKIT_LEFT && textAlign != TAAUTO || trailingSpaceRun->m_level % 2 || direction == RTL || shouldReorder;
                                if (shouldSeparateSpaces) {
                                    BidiContext* baseContext = resolver.context();
                                    while (BidiContext* parent = baseContext->parent())
                                        baseContext = parent;

                                    BidiRun* newTrailingRun = new (renderArena()) BidiRun(firstSpace, trailingSpaceRun->m_stop, trailingSpaceRun->m_object, baseContext, OtherNeutral);
                                    trailingSpaceRun->m_stop = firstSpace;
                                    if (direction == LTR)
                                        resolver.addRun(newTrailingRun);
                                    else
                                        resolver.prependRun(newTrailingRun);
                                    trailingSpaceRun = newTrailingRun;
                                    shouldReorder = false;
                                }
                            }
                            if (shouldReorder) {
                                if (direction == LTR) {
                                    resolver.moveRunToEnd(trailingSpaceRun);
                                    trailingSpaceRun->m_level = 0;
                                } else {
                                    resolver.moveRunToBeginning(trailingSpaceRun);
                                    trailingSpaceRun->m_level = 1;
                                }
                            }
                        }
                    } else
                        trailingSpaceRun = 0;
                }

                // Now that the runs have been ordered, we create the line boxes.
                // At the same time we figure out where border/padding/margin should be applied for
                // inline flow boxes.

                RootInlineBox* lineBox = 0;
                if (resolver.runCount()) {
                    lineBox = constructLine(resolver.runCount(), resolver.firstRun(), resolver.lastRun(), !end.obj, end.obj && !end.pos ? end.obj : 0);
                    if (lineBox) {
                        lineBox->setEndsWithBreak(previousLineBrokeCleanly);

                        // Now we position all of our text runs horizontally.
                        computeHorizontalPositionsForLine(lineBox, resolver.firstRun(), trailingSpaceRun, end.atEnd());

                        // Now position our text runs vertically.
                        computeVerticalPositionsForLine(lineBox, resolver.firstRun());

#if ENABLE(SVG)
                        // Special SVG text layout code
                        lineBox->computePerCharacterLayoutInformation();
#endif

#if PLATFORM(MAC)
                        // Highlight acts as an overflow inflation.
                        if (style()->highlight() != nullAtom)
                            lineBox->addHighlightOverflow();
#endif
                    }
                }

                resolver.deleteRuns();

                if (lineBox) {
                    lineBox->setLineBreakInfo(end.obj, end.pos, resolver.status());
                    if (useRepaintBounds) {
                        repaintTop = min(repaintTop, lineBox->topOverflow());
                        repaintBottom = max(repaintBottom, lineBox->bottomOverflow());
                    }
                }

                m_firstLine = false;
                newLine(clear);
            }

            if (m_floatingObjects && lastRootBox()) {
                if (lastFloat) {
                    for (FloatingObject* f = m_floatingObjects->last(); f != lastFloat; f = m_floatingObjects->prev()) {
                    }
                    m_floatingObjects->next();
                } else
                    m_floatingObjects->first();
                for (FloatingObject* f = m_floatingObjects->current(); f; f = m_floatingObjects->next()) {
                    if (f->m_bottom > lastHeight)
                        lastRootBox()->floats().append(f->m_renderer);
                    ASSERT(f->m_renderer == floats[floatIndex].object);
                    // If a float's geometry has changed, give up on syncing with clean lines.
                    if (floats[floatIndex].rect != IntRect(f->m_left, f->m_top, f->m_width, f->m_bottom - f->m_top))
                        checkForEndLineMatch = false;
                    floatIndex++;
                }
                lastFloat = m_floatingObjects->last();
            }

            lastHeight = height();
            sNumMidpoints = 0;
            sCurrMidpoint = 0;
            resolver.setPosition(end);
        }

        if (endLine) {
            if (endLineMatched) {
                // Attach all the remaining lines, and then adjust their y-positions as needed.
                int delta = height() - endLineYPos;
                for (RootInlineBox* line = endLine; line; line = line->nextRootBox()) {
                    line->attachLine();
                    if (delta) {
                        repaintTop = min(repaintTop, line->topOverflow() + min(delta, 0));
                        repaintBottom = max(repaintBottom, line->bottomOverflow() + max(delta, 0));
                        line->adjustPosition(0, delta);
                    }
                    if (Vector<RenderBox*>* cleanLineFloats = line->floatsPtr()) {
                        Vector<RenderBox*>::iterator end = cleanLineFloats->end();
                        for (Vector<RenderBox*>::iterator f = cleanLineFloats->begin(); f != end; ++f) {
                            int floatTop = (*f)->y() - (*f)->marginTop();
                            insertFloatingObject(*f);
                            setHeight(floatTop + delta);
                            positionNewFloats();
                        }
                    }
                }
                setHeight(lastRootBox()->blockHeight());
            } else {
                // Delete all the remaining lines.
                InlineRunBox* line = endLine;
                RenderArena* arena = renderArena();
                while (line) {
                    repaintTop = min(repaintTop, line->topOverflow());
                    repaintBottom = max(repaintBottom, line->bottomOverflow());
                    InlineRunBox* next = line->nextLineBox();
                    line->deleteLine(arena);
                    line = next;
                }
            }
        }
        if (m_floatingObjects && (checkForFloatsFromLastLine || positionNewFloats()) && lastRootBox()) {
            // In case we have a float on the last line, it might not be positioned up to now.
            // This has to be done before adding in the bottom border/padding, or the float will
            // include the padding incorrectly. -dwh
            if (lastFloat) {
                for (FloatingObject* f = m_floatingObjects->last(); f != lastFloat; f = m_floatingObjects->prev()) {
                }
                m_floatingObjects->next();
            } else
                m_floatingObjects->first();
            for (FloatingObject* f = m_floatingObjects->current(); f; f = m_floatingObjects->next()) {
                if (f->m_bottom > lastHeight)
                    lastRootBox()->floats().append(f->m_renderer);
            }
            lastFloat = m_floatingObjects->last();
        }
    }

    sNumMidpoints = 0;
    sCurrMidpoint = 0;

    // Now add in the bottom border/padding.
    setHeight(height() + toAdd);

    // Always make sure this is at least our height.
    m_overflowHeight = max(height(), m_overflowHeight);

    // See if any lines spill out of the block.  If so, we need to update our overflow width.
    checkLinesForOverflow();

    if (!firstLineBox() && hasLineIfEmpty())
        setHeight(height() + lineHeight(true, true));

    // See if we have any lines that spill out of our block.  If we do, then we will possibly need to
    // truncate text.
    if (hasTextOverflow)
        checkLinesForTextOverflow();
}

RootInlineBox* RenderBlock::determineStartPosition(bool& fullLayout, InlineBidiResolver& resolver, Vector<FloatWithRect>& floats, unsigned& numCleanFloats)
{
    RootInlineBox* curr = 0;
    RootInlineBox* last = 0;

    bool dirtiedByFloat = false;
    if (!fullLayout) {
        size_t floatIndex = 0;
        for (curr = firstRootBox(); curr && !curr->isDirty(); curr = curr->nextRootBox()) {
            if (Vector<RenderBox*>* cleanLineFloats = curr->floatsPtr()) {
                Vector<RenderBox*>::iterator end = cleanLineFloats->end();
                for (Vector<RenderBox*>::iterator o = cleanLineFloats->begin(); o != end; ++o) {
                    RenderBox* f = *o;
                    IntSize newSize(f->width() + f->marginLeft() +f->marginRight(), f->height() + f->marginTop() + f->marginBottom());
                    ASSERT(floatIndex < floats.size());
                    if (floats[floatIndex].object != f) {
                        // A new float has been inserted before this line or before its last known float.
                        // Just do a full layout.
                        fullLayout = true;
                        break;
                    }
                    if (floats[floatIndex].rect.size() != newSize) {
                        int floatTop = floats[floatIndex].rect.y();
                        curr->markDirty();
                        markLinesDirtyInVerticalRange(curr->blockHeight(), floatTop + max(floats[floatIndex].rect.height(), newSize.height()));
                        floats[floatIndex].rect.setSize(newSize);
                        dirtiedByFloat = true;
                    }
                    floatIndex++;
                }
            }
            if (dirtiedByFloat || fullLayout)
                break;
        }
        // Check if a new float has been inserted after the last known float.
        if (!curr && floatIndex < floats.size())
            fullLayout = true;
    }

    if (fullLayout) {
        // Nuke all our lines.
        if (firstRootBox()) {
            RenderArena* arena = renderArena();
            curr = firstRootBox(); 
            while (curr) {
                RootInlineBox* next = curr->nextRootBox();
                curr->deleteLine(arena);
                curr = next;
            }
            ASSERT(!firstLineBox() && !lastLineBox());
        }
    } else {
        if (curr) {
            // We have a dirty line.
            if (RootInlineBox* prevRootBox = curr->prevRootBox()) {
                // We have a previous line.
                if (!dirtiedByFloat && (!prevRootBox->endsWithBreak() || prevRootBox->lineBreakObj()->isText() && prevRootBox->lineBreakPos() >= toRenderText(prevRootBox->lineBreakObj())->textLength()))
                    // The previous line didn't break cleanly or broke at a newline
                    // that has been deleted, so treat it as dirty too.
                    curr = prevRootBox;
            }
        } else {
            // No dirty lines were found.
            // If the last line didn't break cleanly, treat it as dirty.
            if (lastRootBox() && !lastRootBox()->endsWithBreak())
                curr = lastRootBox();
        }

        // If we have no dirty lines, then last is just the last root box.
        last = curr ? curr->prevRootBox() : lastRootBox();
    }

    numCleanFloats = 0;
    if (!floats.isEmpty()) {
        int savedHeight = height();
        // Restore floats from clean lines.
        RootInlineBox* line = firstRootBox();
        while (line != curr) {
            if (Vector<RenderBox*>* cleanLineFloats = line->floatsPtr()) {
                Vector<RenderBox*>::iterator end = cleanLineFloats->end();
                for (Vector<RenderBox*>::iterator f = cleanLineFloats->begin(); f != end; ++f) {
                    insertFloatingObject(*f);
                    setHeight((*f)->y() - (*f)->marginTop());
                    positionNewFloats();
                    ASSERT(floats[numCleanFloats].object == *f);
                    numCleanFloats++;
                }
            }
            line = line->nextRootBox();
        }
        setHeight(savedHeight);
    }

    m_firstLine = !last;
    previousLineBrokeCleanly = !last || last->endsWithBreak();

    RenderObject* startObj;
    int pos = 0;
    if (last) {
        setHeight(last->blockHeight());
        startObj = last->lineBreakObj();
        pos = last->lineBreakPos();
        resolver.setStatus(last->lineBreakBidiStatus());
    } else {
        bool ltr = style()->direction() == LTR
    #if ENABLE(SVG)   
            || (style()->unicodeBidi() == UBNormal && isSVGText())
    #endif
            ;

        BidiContext* context = new BidiContext(ltr ? 0 : 1, ltr ? LeftToRight : RightToLeft, style()->unicodeBidi() == Override);

        resolver.setLastStrongDir(context->dir());
        resolver.setLastDir(context->dir());
        resolver.setEorDir(context->dir());
        resolver.setContext(context);
        startObj = bidiFirst(this, &resolver);
    }

    resolver.setPosition(InlineIterator(this, startObj, pos));

    return curr;
}

RootInlineBox* RenderBlock::determineEndPosition(RootInlineBox* startLine, InlineIterator& cleanLineStart, BidiStatus& cleanLineBidiStatus, int& yPos)
{
    RootInlineBox* last = 0;
    if (!startLine)
        last = 0;
    else {
        for (RootInlineBox* curr = startLine->nextRootBox(); curr; curr = curr->nextRootBox()) {
            if (curr->isDirty())
                last = 0;
            else if (!last)
                last = curr;
        }
    }

    if (!last)
        return 0;

    RootInlineBox* prev = last->prevRootBox();
    cleanLineStart = InlineIterator(this, prev->lineBreakObj(), prev->lineBreakPos());
    cleanLineBidiStatus = prev->lineBreakBidiStatus();
    yPos = prev->blockHeight();

    for (RootInlineBox* line = last; line; line = line->nextRootBox())
        line->extractLine(); // Disconnect all line boxes from their render objects while preserving
                             // their connections to one another.

    return last;
}

bool RenderBlock::matchedEndLine(const InlineBidiResolver& resolver, const InlineIterator& endLineStart, const BidiStatus& endLineStatus, RootInlineBox*& endLine, int& endYPos, int& repaintBottom, int& repaintTop)
{
    if (resolver.position() == endLineStart) {
        if (resolver.status() != endLineStatus)
            return false;

        int delta = height() - endYPos;
        if (!delta || !m_floatingObjects)
            return true;

        // See if any floats end in the range along which we want to shift the lines vertically.
        int top = min(height(), endYPos);

        RootInlineBox* lastLine = endLine;
        while (RootInlineBox* nextLine = lastLine->nextRootBox())
            lastLine = nextLine;

        int bottom = lastLine->blockHeight() + abs(delta);

        for (FloatingObject* f = m_floatingObjects->first(); f; f = m_floatingObjects->next()) {
            if (f->m_bottom >= top && f->m_bottom < bottom)
                return false;
        }

        return true;
    }

    // The first clean line doesn't match, but we can check a handful of following lines to try
    // to match back up.
    static int numLines = 8; // The # of lines we're willing to match against.
    RootInlineBox* line = endLine;
    for (int i = 0; i < numLines && line; i++, line = line->nextRootBox()) {
        if (line->lineBreakObj() == resolver.position().obj && line->lineBreakPos() == resolver.position().pos) {
            // We have a match.
            if (line->lineBreakBidiStatus() != resolver.status())
                return false; // ...but the bidi state doesn't match.
            RootInlineBox* result = line->nextRootBox();

            // Set our yPos to be the block height of endLine.
            if (result)
                endYPos = line->blockHeight();

            int delta = height() - endYPos;
            if (delta && m_floatingObjects) {
                // See if any floats end in the range along which we want to shift the lines vertically.
                int top = min(height(), endYPos);

                RootInlineBox* lastLine = endLine;
                while (RootInlineBox* nextLine = lastLine->nextRootBox())
                    lastLine = nextLine;

                int bottom = lastLine->blockHeight() + abs(delta);

                for (FloatingObject* f = m_floatingObjects->first(); f; f = m_floatingObjects->next()) {
                    if (f->m_bottom >= top && f->m_bottom < bottom)
                        return false;
                }
            }

            // Now delete the lines that we failed to sync.
            RootInlineBox* boxToDelete = endLine;
            RenderArena* arena = renderArena();
            while (boxToDelete && boxToDelete != result) {
                repaintTop = min(repaintTop, boxToDelete->topOverflow());
                repaintBottom = max(repaintBottom, boxToDelete->bottomOverflow());
                RootInlineBox* next = boxToDelete->nextRootBox();
                boxToDelete->deleteLine(arena);
                boxToDelete = next;
            }

            endLine = result;
            return result;
        }
    }

    return false;
}

static inline bool skipNonBreakingSpace(const InlineIterator& it)
{
    if (it.obj->style()->nbspMode() != SPACE || it.current() != noBreakSpace)
        return false;

    // FIXME: This is bad.  It makes nbsp inconsistent with space and won't work correctly
    // with m_minWidth/m_maxWidth.
    // Do not skip a non-breaking space if it is the first character
    // on a line after a clean line break (or on the first line, since previousLineBrokeCleanly starts off
    // |true|).
    if (isLineEmpty && previousLineBrokeCleanly)
        return false;

    return true;
}

static inline bool shouldCollapseWhiteSpace(const RenderStyle* style)
{
    return style->collapseWhiteSpace() || (style->whiteSpace() == PRE_WRAP && (!isLineEmpty || !previousLineBrokeCleanly));
}

static inline bool shouldPreserveNewline(RenderObject* object)
{
#if ENABLE(SVG)
    if (object->isSVGText())
        return false;
#endif

    return object->style()->preserveNewline();
}

static bool inlineFlowRequiresLineBox(RenderBox* flow)
{
    // FIXME: Right now, we only allow line boxes for inlines that are truly empty.
    // We need to fix this, though, because at the very least, inlines containing only
    // ignorable whitespace should should also have line boxes. 
    return flow->isRenderInline() && !flow->firstChild() && flow->hasHorizontalBordersPaddingOrMargin();
}

static inline bool requiresLineBox(const InlineIterator& it)
{
    if (it.obj->isFloatingOrPositioned())
        return false;

    if (it.obj->isRenderInline() && !inlineFlowRequiresLineBox(toRenderBox(it.obj)))
        return false;

    if (!shouldCollapseWhiteSpace(it.obj->style()) || it.obj->isBR())
        return true;

    UChar current = it.current();
    return current != ' ' && current != '\t' && current != softHyphen && (current != '\n' || shouldPreserveNewline(it.obj)) && !skipNonBreakingSpace(it);
}

bool RenderBlock::generatesLineBoxesForInlineChild(RenderObject* inlineObj)
{
    ASSERT(inlineObj->parent() == this);

    InlineIterator it(this, inlineObj, 0);
    while (!it.atEnd() && !requiresLineBox(it))
        it.increment();

    return !it.atEnd();
}

// FIXME: The entire concept of the skipTrailingWhitespace function is flawed, since we really need to be building
// line boxes even for containers that may ultimately collapse away.  Otherwise we'll never get positioned
// elements quite right.  In other words, we need to build this function's work into the normal line
// object iteration process.
// NB. this function will insert any floating elements that would otherwise
// be skipped but it will not position them.
void RenderBlock::skipTrailingWhitespace(InlineIterator& iterator)
{
    while (!iterator.atEnd() && !requiresLineBox(iterator)) {
        RenderObject* object = iterator.obj;
        if (object->isFloating()) {
            insertFloatingObject(toRenderBox(object));
        } else if (object->isPositioned()) {
            // FIXME: The math here is actually not really right.  It's a best-guess approximation that
            // will work for the common cases
            RenderObject* c = object->container();
            if (c->isRenderInline()) {
                // A relative positioned inline encloses us.  In this case, we also have to determine our
                // position as though we were an inline.  Set |staticX| and |staticY| on the relative positioned
                // inline so that we can obtain the value later.
                c->setStaticX(style()->direction() == LTR ? leftOffset(height()) : rightOffset(height()));
                c->setStaticY(height());
            }
    
            if (object->hasStaticX()) {
                if (object->style()->isOriginalDisplayInlineType())
                    object->setStaticX(style()->direction() == LTR ? leftOffset(height()) : width() - rightOffset(height()));
                else
                    object->setStaticX(style()->direction() == LTR ? borderLeft() + paddingLeft() : borderRight() + paddingRight());
            }
    
            if (object->hasStaticY())
                object->setStaticY(height());
        }
        iterator.increment();
    }
}

int RenderBlock::skipLeadingWhitespace(InlineBidiResolver& resolver)
{
    int availableWidth = lineWidth(height());
    while (!resolver.position().atEnd() && !requiresLineBox(resolver.position())) {
        RenderObject* object = resolver.position().obj;
        if (object->isFloating()) {
            insertFloatingObject(toRenderBox(object));
            positionNewFloats();
            availableWidth = lineWidth(height());
        } else if (object->isPositioned()) {
            // FIXME: The math here is actually not really right.  It's a best-guess approximation that
            // will work for the common cases
            RenderObject* c = object->container();
            if (c->isRenderInline()) {
                // A relative positioned inline encloses us.  In this case, we also have to determine our
                // position as though we were an inline.  Set |staticX| and |staticY| on the relative positioned
                // inline so that we can obtain the value later.
                c->setStaticX(style()->direction() == LTR ? leftOffset(height()) : rightOffset(height()));
                c->setStaticY(height());
            }
    
            if (object->hasStaticX()) {
                if (object->style()->isOriginalDisplayInlineType())
                    object->setStaticX(style()->direction() == LTR ? leftOffset(height()) : width() - rightOffset(height()));
                else
                    object->setStaticX(style()->direction() == LTR ? borderLeft() + paddingLeft() : borderRight() + paddingRight());
            }
    
            if (object->hasStaticY())
                object->setStaticY(height());
        }
        resolver.increment();
    }
    resolver.commitExplicitEmbedding();
    return availableWidth;
}

// This is currently just used for list markers and inline flows that have line boxes. Neither should 
// have an effect on whitespace at the start of the line. 
static bool shouldSkipWhitespaceAfterStartObject(RenderBlock* block, RenderObject* o)
{
    RenderObject* next = bidiNext(block, o);
    if (next && !next->isBR() && next->isText() && toRenderText(next)->textLength() > 0) {
        RenderText* nextText = toRenderText(next);
        UChar nextChar = nextText->characters()[0];
        if (nextText->style()->isCollapsibleWhiteSpace(nextChar)) {
            addMidpoint(InlineIterator(0, o, 0));
            return true;
        }
    }

    return false;
}

void RenderBlock::fitBelowFloats(int widthToFit, int& availableWidth)
{
    ASSERT(widthToFit > availableWidth);

    int floatBottom;
    int lastFloatBottom = height();
    int newLineWidth = availableWidth;
    while (true) {
        floatBottom = nextFloatBottomBelow(lastFloatBottom);
        if (!floatBottom)
            break;

        newLineWidth = lineWidth(floatBottom);
        lastFloatBottom = floatBottom;
        if (newLineWidth >= widthToFit)
            break;
    }

    if (newLineWidth > availableWidth) {
        setHeight(lastFloatBottom);
        availableWidth = newLineWidth;
    }
}

InlineIterator RenderBlock::findNextLineBreak(InlineBidiResolver& resolver, EClear* clear)
{
    ASSERT(resolver.position().block == this);

    bool appliedStartWidth = resolver.position().pos > 0;

    int width = skipLeadingWhitespace(resolver);

    int w = 0;
    int tmpW = 0;

    if (resolver.position().atEnd())
        return resolver.position();

    // This variable is used only if whitespace isn't set to PRE, and it tells us whether
    // or not we are currently ignoring whitespace.
    bool ignoringSpaces = false;
    InlineIterator ignoreStart;
    
    // This variable tracks whether the very last character we saw was a space.  We use
    // this to detect when we encounter a second space so we know we have to terminate
    // a run.
    bool currentCharacterIsSpace = false;
    bool currentCharacterIsWS = false;
    RenderObject* trailingSpaceObject = 0;

    InlineIterator lBreak = resolver.position();

    RenderObject *o = resolver.position().obj;
    RenderObject *last = o;
    unsigned pos = resolver.position().pos;
    int nextBreakable = resolver.position().nextBreakablePosition;
    bool atStart = true;

    bool prevLineBrokeCleanly = previousLineBrokeCleanly;
    previousLineBrokeCleanly = false;

    bool autoWrapWasEverTrueOnLine = false;
    bool floatsFitOnLine = true;
    
    // Firefox and Opera will allow a table cell to grow to fit an image inside it under
    // very specific circumstances (in order to match common WinIE renderings). 
    // Not supporting the quirk has caused us to mis-render some real sites. (See Bugzilla 10517.) 
    bool allowImagesToBreak = !style()->htmlHacks() || !isTableCell() || !style()->width().isIntrinsicOrAuto();

    EWhiteSpace currWS = style()->whiteSpace();
    EWhiteSpace lastWS = currWS;
    while (o) {
        currWS = o->isReplaced() ? o->parent()->style()->whiteSpace() : o->style()->whiteSpace();
        lastWS = last->isReplaced() ? last->parent()->style()->whiteSpace() : last->style()->whiteSpace();
        
        bool autoWrap = RenderStyle::autoWrap(currWS);
        autoWrapWasEverTrueOnLine = autoWrapWasEverTrueOnLine || autoWrap;

#if ENABLE(SVG)
        bool preserveNewline = o->isSVGText() ? false : RenderStyle::preserveNewline(currWS);
#else
        bool preserveNewline = RenderStyle::preserveNewline(currWS);
#endif

        bool collapseWhiteSpace = RenderStyle::collapseWhiteSpace(currWS);
            
        if (o->isBR()) {
            if (w + tmpW <= width) {
                lBreak.obj = o;
                lBreak.pos = 0;
                lBreak.nextBreakablePosition = -1;
                lBreak.increment();

                // A <br> always breaks a line, so don't let the line be collapsed
                // away. Also, the space at the end of a line with a <br> does not
                // get collapsed away.  It only does this if the previous line broke
                // cleanly.  Otherwise the <br> has no effect on whether the line is
                // empty or not.
                if (prevLineBrokeCleanly)
                    isLineEmpty = false;
                trailingSpaceObject = 0;
                previousLineBrokeCleanly = true;

                if (!isLineEmpty && clear)
                    *clear = o->style()->clear();
            }
            goto end;
        }

        if (o->isFloatingOrPositioned()) {
            // add to special objects...
            if (o->isFloating()) {
                RenderBox* floatBox = toRenderBox(o);
                insertFloatingObject(floatBox);
                // check if it fits in the current line.
                // If it does, position it now, otherwise, position
                // it after moving to next line (in newLine() func)
                if (floatsFitOnLine && floatBox->width() + floatBox->marginLeft() + floatBox->marginRight() + w + tmpW <= width) {
                    positionNewFloats();
                    width = lineWidth(height());
                } else
                    floatsFitOnLine = false;
            } else if (o->isPositioned()) {
                // If our original display wasn't an inline type, then we can
                // go ahead and determine our static x position now.
                bool isInlineType = o->style()->isOriginalDisplayInlineType();
                bool needToSetStaticX = o->hasStaticX();
                if (o->hasStaticX() && !isInlineType) {
                    o->setStaticX(o->parent()->style()->direction() == LTR ?
                                  borderLeft() + paddingLeft() :
                                  borderRight() + paddingRight());
                    needToSetStaticX = false;
                }

                // If our original display was an INLINE type, then we can go ahead
                // and determine our static y position now.
                bool needToSetStaticY = o->hasStaticY();
                if (o->hasStaticY() && isInlineType) {
                    o->setStaticY(height());
                    needToSetStaticY = false;
                }
                
                bool needToCreateLineBox = needToSetStaticX || needToSetStaticY;
                RenderObject* c = o->container();
                if (c->isRenderInline() && (!needToSetStaticX || !needToSetStaticY))
                    needToCreateLineBox = true;

                // If we're ignoring spaces, we have to stop and include this object and
                // then start ignoring spaces again.
                if (needToCreateLineBox) {
                    trailingSpaceObject = 0;
                    ignoreStart.obj = o;
                    ignoreStart.pos = 0;
                    if (ignoringSpaces) {
                        addMidpoint(ignoreStart); // Stop ignoring spaces.
                        addMidpoint(ignoreStart); // Start ignoring again.
                    }
                    
                }
            }
        } else if (o->isRenderInline()) {
            // Right now, we should only encounter empty inlines here.
            ASSERT(!o->firstChild());
    
            RenderBox* flowBox = toRenderBox(o);
            
            // Now that some inline flows have line boxes, if we are already ignoring spaces, we need 
            // to make sure that we stop to include this object and then start ignoring spaces again. 
            // If this object is at the start of the line, we need to behave like list markers and 
            // start ignoring spaces.
            if (inlineFlowRequiresLineBox(flowBox)) {
                isLineEmpty = false;
                if (ignoringSpaces) {
                    trailingSpaceObject = 0;
                    addMidpoint(InlineIterator(0, o, 0)); // Stop ignoring spaces.
                    addMidpoint(InlineIterator(0, o, 0)); // Start ignoring again.
                } else if (style()->collapseWhiteSpace() && resolver.position().obj == o
                    && shouldSkipWhitespaceAfterStartObject(this, o)) {
                    // Like with list markers, we start ignoring spaces to make sure that any 
                    // additional spaces we see will be discarded.
                    currentCharacterIsSpace = true;
                    currentCharacterIsWS = true;
                    ignoringSpaces = true;
                }
            }

            tmpW += flowBox->marginLeft() + flowBox->borderLeft() + flowBox->paddingLeft() +
                    flowBox->marginRight() + flowBox->borderRight() + flowBox->paddingRight();
        } else if (o->isReplaced()) {
            RenderBox* replacedBox = toRenderBox(o);

            // Break on replaced elements if either has normal white-space.
            if ((autoWrap || RenderStyle::autoWrap(lastWS)) && (!o->isImage() || allowImagesToBreak)) {
                w += tmpW;
                tmpW = 0;
                lBreak.obj = o;
                lBreak.pos = 0;
                lBreak.nextBreakablePosition = -1;
            }

            if (ignoringSpaces)
                addMidpoint(InlineIterator(0, o, 0));

            isLineEmpty = false;
            ignoringSpaces = false;
            currentCharacterIsSpace = false;
            currentCharacterIsWS = false;
            trailingSpaceObject = 0;
            
            // Optimize for a common case. If we can't find whitespace after the list
            // item, then this is all moot. -dwh
            if (o->isListMarker() && !static_cast<RenderListMarker*>(o)->isInside()) {
                if (style()->collapseWhiteSpace() && shouldSkipWhitespaceAfterStartObject(this, o)) {
                    // Like with inline flows, we start ignoring spaces to make sure that any 
                    // additional spaces we see will be discarded.
                    currentCharacterIsSpace = true;
                    currentCharacterIsWS = true;
                    ignoringSpaces = true;
                }
            } else
                tmpW += replacedBox->width() + replacedBox->marginLeft() + replacedBox->marginRight() + inlineWidth(o);
        } else if (o->isText()) {
            if (!pos)
                appliedStartWidth = false;

            RenderText* t = toRenderText(o);

            int strlen = t->textLength();
            int len = strlen - pos;
            const UChar* str = t->characters();

            const Font& f = t->style(m_firstLine)->font();

            int lastSpace = pos;
            int wordSpacing = o->style()->wordSpacing();
            int lastSpaceWordSpacing = 0;

            int wrapW = tmpW + inlineWidth(o, !appliedStartWidth, true);
            int charWidth = 0;
            bool breakNBSP = autoWrap && o->style()->nbspMode() == SPACE;
            // Auto-wrapping text should wrap in the middle of a word only if it could not wrap before the word,
            // which is only possible if the word is the first thing on the line, that is, if |w| is zero.
            bool breakWords = o->style()->breakWords() && ((autoWrap && !w) || currWS == PRE);
            bool midWordBreak = false;
            bool breakAll = o->style()->wordBreak() == BreakAllWordBreak && autoWrap;

            if (t->isWordBreak()) {
                w += tmpW;
                tmpW = 0;
                lBreak.obj = o;
                lBreak.pos = 0;
                lBreak.nextBreakablePosition = -1;
                ASSERT(!len);
            }

            while (len) {
                bool previousCharacterIsSpace = currentCharacterIsSpace;
                bool previousCharacterIsWS = currentCharacterIsWS;
                UChar c = str[pos];
                currentCharacterIsSpace = c == ' ' || c == '\t' || (!preserveNewline && (c == '\n'));

                if (!collapseWhiteSpace || !currentCharacterIsSpace)
                    isLineEmpty = false;
                
                // Check for soft hyphens.  Go ahead and ignore them.
                if (c == softHyphen) {
                    if (!ignoringSpaces) {
                        // Ignore soft hyphens
                        InlineIterator beforeSoftHyphen;
                        if (pos)
                            beforeSoftHyphen = InlineIterator(0, o, pos - 1);
                        else
                            beforeSoftHyphen = InlineIterator(0, last, last->isText() ? toRenderText(last)->textLength() - 1 : 0);
                        // Two consecutive soft hyphens. Avoid overlapping midpoints.
                        if (sNumMidpoints && smidpoints->at(sNumMidpoints - 1).obj == o && smidpoints->at(sNumMidpoints - 1).pos == pos)
                            sNumMidpoints--;
                        else
                            addMidpoint(beforeSoftHyphen);

                        // Add the width up to but not including the hyphen.
                        tmpW += t->width(lastSpace, pos - lastSpace, f, w + tmpW) + lastSpaceWordSpacing;

                        // For wrapping text only, include the hyphen.  We need to ensure it will fit
                        // on the line if it shows when we break.
                        if (autoWrap)
                            tmpW += t->width(pos, 1, f, w + tmpW);

                        InlineIterator afterSoftHyphen(0, o, pos);
                        afterSoftHyphen.increment();
                        addMidpoint(afterSoftHyphen);
                    }

                    pos++;
                    len--;
                    lastSpaceWordSpacing = 0;
                    lastSpace = pos; // Cheesy hack to prevent adding in widths of the run twice.
                    continue;
                }
                
                bool applyWordSpacing = false;
                
                currentCharacterIsWS = currentCharacterIsSpace || (breakNBSP && c == noBreakSpace);

                if ((breakAll || breakWords) && !midWordBreak) {
                    wrapW += charWidth;
                    charWidth = t->width(pos, 1, f, w + wrapW);
                    midWordBreak = w + wrapW + charWidth > width;
                }

                bool betweenWords = c == '\n' || (currWS != PRE && !atStart && isBreakable(str, pos, strlen, nextBreakable, breakNBSP));
    
                if (betweenWords || midWordBreak) {
                    if (strlen > 0 && pos > 0 && (str[pos - 1] == '-' || str[pos - 1] == '?'))
                        midWordBreak = true;
                    bool stoppedIgnoringSpaces = false;
                    if (ignoringSpaces) {
                        if (!currentCharacterIsSpace) {
                            // Stop ignoring spaces and begin at this
                            // new point.
                            ignoringSpaces = false;
                            lastSpaceWordSpacing = 0;
                            lastSpace = pos; // e.g., "Foo    goo", don't add in any of the ignored spaces.
                            addMidpoint(InlineIterator(0, o, pos));
                            stoppedIgnoringSpaces = true;
                        } else {
                            // Just keep ignoring these spaces.
                            pos++;
                            len--;
                            continue;
                        }
                    }

                    int additionalTmpW = t->width(lastSpace, pos - lastSpace, f, w+tmpW) + lastSpaceWordSpacing;
                    tmpW += additionalTmpW;
                    if (!appliedStartWidth) {
                        tmpW += inlineWidth(o, true, false);
                        appliedStartWidth = true;
                    }
                    
                    applyWordSpacing =  wordSpacing && currentCharacterIsSpace && !previousCharacterIsSpace;

                    if (!w && autoWrap && tmpW > width)
                        fitBelowFloats(tmpW, width);

                    if (autoWrap || breakWords) {
                        // If we break only after white-space, consider the current character
                        // as candidate width for this line.
                        bool lineWasTooWide = false;
                        if (w + tmpW <= width && currentCharacterIsWS && o->style()->breakOnlyAfterWhiteSpace() && !midWordBreak) {
                            int charWidth = t->width(pos, 1, f, w + tmpW) + (applyWordSpacing ? wordSpacing : 0);
                            // Check if line is too big even without the extra space
                            // at the end of the line. If it is not, do nothing. 
                            // If the line needs the extra whitespace to be too long, 
                            // then move the line break to the space and skip all 
                            // additional whitespace.
                            if (w + tmpW + charWidth > width) {
                                lineWasTooWide = true;
                                lBreak.obj = o;
                                lBreak.pos = pos;
                                lBreak.nextBreakablePosition = nextBreakable;
                                skipTrailingWhitespace(lBreak);
                            }
                        }
                        if (lineWasTooWide || w + tmpW > width) {
                            if (lBreak.obj && shouldPreserveNewline(lBreak.obj) && lBreak.obj->isText() && !toRenderText(lBreak.obj)->isWordBreak() && toRenderText(lBreak.obj)->characters()[lBreak.pos] == '\n') {
                                if (!stoppedIgnoringSpaces && pos > 0) {
                                    // We need to stop right before the newline and then start up again.
                                    addMidpoint(InlineIterator(0, o, pos - 1)); // Stop
                                    addMidpoint(InlineIterator(0, o, pos)); // Start
                                }
                                lBreak.increment();
                                previousLineBrokeCleanly = true;
                            }
                            goto end; // Didn't fit. Jump to the end.
                        } else {
                            if (!betweenWords || (midWordBreak && !autoWrap))
                                tmpW -= additionalTmpW;
                            if (pos > 0 && str[pos-1] == softHyphen)
                                // Subtract the width of the soft hyphen out since we fit on a line.
                                tmpW -= t->width(pos-1, 1, f, w+tmpW);
                        }
                    }

                    if (c == '\n' && preserveNewline) {
                        if (!stoppedIgnoringSpaces && pos > 0) {
                            // We need to stop right before the newline and then start up again.
                            addMidpoint(InlineIterator(0, o, pos - 1)); // Stop
                            addMidpoint(InlineIterator(0, o, pos)); // Start
                        }
                        lBreak.obj = o;
                        lBreak.pos = pos;
                        lBreak.nextBreakablePosition = nextBreakable;
                        lBreak.increment();
                        previousLineBrokeCleanly = true;
                        return lBreak;
                    }

                    if (autoWrap && betweenWords) {
                        w += tmpW;
                        wrapW = 0;
                        tmpW = 0;
                        lBreak.obj = o;
                        lBreak.pos = pos;
                        lBreak.nextBreakablePosition = nextBreakable;
                        // Auto-wrapping text should not wrap in the middle of a word once it has had an
                        // opportunity to break after a word.
                        breakWords = false;
                    }
                    
                    if (midWordBreak) {
                        // Remember this as a breakable position in case
                        // adding the end width forces a break.
                        lBreak.obj = o;
                        lBreak.pos = pos;
                        lBreak.nextBreakablePosition = nextBreakable;
                        midWordBreak &= (breakWords || breakAll);
                    }

                    if (betweenWords) {
                        lastSpaceWordSpacing = applyWordSpacing ? wordSpacing : 0;
                        lastSpace = pos;
                    }
                    
                    if (!ignoringSpaces && o->style()->collapseWhiteSpace()) {
                        // If we encounter a newline, or if we encounter a
                        // second space, we need to go ahead and break up this
                        // run and enter a mode where we start collapsing spaces.
                        if (currentCharacterIsSpace && previousCharacterIsSpace) {
                            ignoringSpaces = true;
                            
                            // We just entered a mode where we are ignoring
                            // spaces. Create a midpoint to terminate the run
                            // before the second space. 
                            addMidpoint(ignoreStart);
                        }
                    }
                } else if (ignoringSpaces) {
                    // Stop ignoring spaces and begin at this
                    // new point.
                    ignoringSpaces = false;
                    lastSpaceWordSpacing = applyWordSpacing ? wordSpacing : 0;
                    lastSpace = pos; // e.g., "Foo    goo", don't add in any of the ignored spaces.
                    addMidpoint(InlineIterator(0, o, pos));
                }

                if (currentCharacterIsSpace && !previousCharacterIsSpace) {
                    ignoreStart.obj = o;
                    ignoreStart.pos = pos;
                }

                if (!currentCharacterIsWS && previousCharacterIsWS) {
                    if (autoWrap && o->style()->breakOnlyAfterWhiteSpace()) {
                        lBreak.obj = o;
                        lBreak.pos = pos;
                        lBreak.nextBreakablePosition = nextBreakable;
                    }
                }
                
                if (collapseWhiteSpace && currentCharacterIsSpace && !ignoringSpaces)
                    trailingSpaceObject = o;
                else if (!o->style()->collapseWhiteSpace() || !currentCharacterIsSpace)
                    trailingSpaceObject = 0;
                    
                pos++;
                len--;
                atStart = false;
            }

            // IMPORTANT: pos is > length here!
            if (!ignoringSpaces)
                tmpW += t->width(lastSpace, pos - lastSpace, f, w+tmpW) + lastSpaceWordSpacing;
            tmpW += inlineWidth(o, !appliedStartWidth, true);
        } else
            ASSERT_NOT_REACHED();

        RenderObject* next = bidiNext(this, o);
        bool checkForBreak = autoWrap;
        if (w && w + tmpW > width && lBreak.obj && currWS == NOWRAP)
            checkForBreak = true;
        else if (next && o->isText() && next->isText() && !next->isBR()) {
            if (autoWrap || (next->style()->autoWrap())) {
                if (currentCharacterIsSpace)
                    checkForBreak = true;
                else {
                    checkForBreak = false;
                    RenderText* nextText = toRenderText(next);
                    if (nextText->textLength()) {
                        UChar c = nextText->characters()[0];
                        if (c == ' ' || c == '\t' || (c == '\n' && !shouldPreserveNewline(next)))
                            // If the next item on the line is text, and if we did not end with
                            // a space, then the next text run continues our word (and so it needs to
                            // keep adding to |tmpW|.  Just update and continue.
                            checkForBreak = true;
                    } else if (nextText->isWordBreak())
                        checkForBreak = true;
                    bool willFitOnLine = w + tmpW <= width;
                    if (!willFitOnLine && !w) {
                        fitBelowFloats(tmpW, width);
                        willFitOnLine = tmpW <= width;
                    }
                    bool canPlaceOnLine = willFitOnLine || !autoWrapWasEverTrueOnLine;
                    if (canPlaceOnLine && checkForBreak) {
                        w += tmpW;
                        tmpW = 0;
                        lBreak.obj = next;
                        lBreak.pos = 0;
                        lBreak.nextBreakablePosition = -1;
                    }
                }
            }
        }

        if (checkForBreak && (w + tmpW > width)) {
            // if we have floats, try to get below them.
            if (currentCharacterIsSpace && !ignoringSpaces && o->style()->collapseWhiteSpace())
                trailingSpaceObject = 0;

            if (w)
                goto end;

            fitBelowFloats(tmpW, width);

            // |width| may have been adjusted because we got shoved down past a float (thus
            // giving us more room), so we need to retest, and only jump to
            // the end label if we still don't fit on the line. -dwh
            if (w + tmpW > width)
                goto end;
        }

        if (!o->isFloatingOrPositioned()) {
            last = o;
            if (last->isReplaced() && autoWrap && (!last->isImage() || allowImagesToBreak) && (!last->isListMarker() || static_cast<RenderListMarker*>(last)->isInside())) {
                w += tmpW;
                tmpW = 0;
                lBreak.obj = next;
                lBreak.pos = 0;
                lBreak.nextBreakablePosition = -1;
            }
        }

        o = next;
        nextBreakable = -1;

        // Clear out our character space bool, since inline <pre>s don't collapse whitespace
        // with adjacent inline normal/nowrap spans.
        if (!collapseWhiteSpace)
            currentCharacterIsSpace = false;
        
        pos = 0;
        atStart = false;
    }

    
    if (w + tmpW <= width || lastWS == NOWRAP) {
        lBreak.obj = 0;
        lBreak.pos = 0;
        lBreak.nextBreakablePosition = -1;
    }

 end:

    if (lBreak == resolver.position() && !lBreak.obj->isBR()) {
        // we just add as much as possible
        if (style()->whiteSpace() == PRE) {
            // FIXME: Don't really understand this case.
            if (pos != 0) {
                lBreak.obj = o;
                lBreak.pos = pos - 1;
            } else {
                lBreak.obj = last;
                lBreak.pos = last->isText() ? last->length() : 0;
                lBreak.nextBreakablePosition = -1;
            }
        } else if (lBreak.obj) {
            if (last != o && !last->isListMarker()) {
                // better to break between object boundaries than in the middle of a word (except for list markers)
                lBreak.obj = o;
                lBreak.pos = 0;
                lBreak.nextBreakablePosition = -1;
            } else {
                // Don't ever break in the middle of a word if we can help it.
                // There's no room at all. We just have to be on this line,
                // even though we'll spill out.
                lBreak.obj = o;
                lBreak.pos = pos;
                lBreak.nextBreakablePosition = -1;
            }
        }
    }

    // make sure we consume at least one char/object.
    if (lBreak == resolver.position())
        lBreak.increment();

    // Sanity check our midpoints.
    checkMidpoints(lBreak);
        
    if (trailingSpaceObject) {
        // This object is either going to be part of the last midpoint, or it is going
        // to be the actual endpoint.  In both cases we just decrease our pos by 1 level to
        // exclude the space, allowing it to - in effect - collapse into the newline.
        if (sNumMidpoints%2==1) {
            InlineIterator* midpoints = smidpoints->data();
            midpoints[sNumMidpoints-1].pos--;
        }
        //else if (lBreak.pos > 0)
        //    lBreak.pos--;
        else if (lBreak.obj == 0 && trailingSpaceObject->isText()) {
            // Add a new end midpoint that stops right at the very end.
            RenderText* text = toRenderText(trailingSpaceObject);
            unsigned length = text->textLength();
            unsigned pos = length >= 2 ? length - 2 : UINT_MAX;
            InlineIterator endMid(0, trailingSpaceObject, pos);
            addMidpoint(endMid);
        }
    }

    // We might have made lBreak an iterator that points past the end
    // of the object. Do this adjustment to make it point to the start
    // of the next object instead to avoid confusing the rest of the
    // code.
    if (lBreak.pos > 0) {
        lBreak.pos--;
        lBreak.increment();
    }

    if (lBreak.obj && lBreak.pos >= 2 && lBreak.obj->isText()) {
        // For soft hyphens on line breaks, we have to chop out the midpoints that made us
        // ignore the hyphen so that it will render at the end of the line.
        UChar c = toRenderText(lBreak.obj)->characters()[lBreak.pos-1];
        if (c == softHyphen)
            chopMidpointsAt(lBreak.obj, lBreak.pos-2);
    }
    
    return lBreak;
}

void RenderBlock::checkLinesForOverflow()
{
    m_overflowWidth = width();
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        m_overflowLeft = min(curr->leftOverflow(), m_overflowLeft);
        m_overflowTop = min(curr->topOverflow(), m_overflowTop);
        m_overflowWidth = max(curr->rightOverflow(), m_overflowWidth);
        m_overflowHeight = max(curr->bottomOverflow(), m_overflowHeight);
    }
}

void RenderBlock::deleteEllipsisLineBoxes()
{
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox())
        curr->clearTruncation();
}

void RenderBlock::checkLinesForTextOverflow()
{
    // Determine the width of the ellipsis using the current font.
    // FIXME: CSS3 says this is configurable, also need to use 0x002E (FULL STOP) if horizontal ellipsis is "not renderable"
    TextRun ellipsisRun(&horizontalEllipsis, 1);
    DEFINE_STATIC_LOCAL(AtomicString, ellipsisStr, (&horizontalEllipsis, 1));
    const Font& firstLineFont = firstLineStyle()->font();
    const Font& font = style()->font();
    int firstLineEllipsisWidth = firstLineFont.width(ellipsisRun);
    int ellipsisWidth = (font == firstLineFont) ? firstLineEllipsisWidth : font.width(ellipsisRun);

    // For LTR text truncation, we want to get the right edge of our padding box, and then we want to see
    // if the right edge of a line box exceeds that.  For RTL, we use the left edge of the padding box and
    // check the left edge of the line box to see if it is less
    // Include the scrollbar for overflow blocks, which means we want to use "contentWidth()"
    bool ltr = style()->direction() == LTR;
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        int blockEdge = ltr ? rightOffset(curr->yPos()) : leftOffset(curr->yPos());
        int lineBoxEdge = ltr ? curr->xPos() + curr->width() : curr->xPos();
        if ((ltr && lineBoxEdge > blockEdge) || (!ltr && lineBoxEdge < blockEdge)) {
            // This line spills out of our box in the appropriate direction.  Now we need to see if the line
            // can be truncated.  In order for truncation to be possible, the line must have sufficient space to
            // accommodate our truncation string, and no replaced elements (images, tables) can overlap the ellipsis
            // space.
            int width = curr == firstRootBox() ? firstLineEllipsisWidth : ellipsisWidth;
            if (curr->canAccommodateEllipsis(ltr, blockEdge, lineBoxEdge, width))
                curr->placeEllipsis(ellipsisStr, ltr, blockEdge, width);
        }
    }
}

}
