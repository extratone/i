/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#include "config.h"
#include "bidi.h"

#include "CharacterNames.h"
#include "Document.h"
#include "Element.h"
#include "FrameView.h"
#include "InlineTextBox.h"
#include "Logging.h"
#include "RenderArena.h"
#include "RenderView.h"
#include "break_lines.h"
#include <wtf/AlwaysInline.h>
#include <wtf/Vector.h>

using namespace std;

namespace WebCore {

// an iterator which traverses all the objects within a block
struct BidiIterator {
    BidiIterator() : block(0), obj(0), pos(0) {}
    BidiIterator(RenderBlock* b, RenderObject* o, unsigned int p) 
        : block(b), obj(o), pos(p) {}
    
    void increment(BidiState& state);
    bool atEnd() const;
    
    UChar current() const;
    UCharDirection direction() const;

    RenderBlock* block;
    RenderObject* obj;
    unsigned int pos;
};

struct BidiState {
    BidiState() : context(0), dir(U_OTHER_NEUTRAL), adjustEmbedding(false), reachedEndOfLine(false) {}
    
    BidiIterator sor;
    BidiIterator eor;
    BidiIterator last;
    BidiIterator current;
    RefPtr<BidiContext> context;
    BidiStatus status;
    UCharDirection dir;
    bool adjustEmbedding;
    BidiIterator endOfLine;
    bool reachedEndOfLine;
    BidiIterator lastBeforeET;
};

inline bool operator==(const BidiStatus& status1, const BidiStatus& status2)
{
    return status1.eor == status2.eor && status1.last == status2.last && status1.lastStrong == status2.lastStrong;
}

inline bool operator!=(const BidiStatus& status1, const BidiStatus& status2)
{
    return !(status1 == status2);
}

// Used to track a list of chained bidi runs.
static BidiRun* sFirstBidiRun;
static BidiRun* sLastBidiRun;
static BidiRun* sLogicallyLastBidiRun;
static int sBidiRunCount;
static BidiRun* sCompactFirstBidiRun;
static BidiRun* sCompactLastBidiRun;
static int sCompactBidiRunCount;
static bool sBuildingCompactRuns;

// Midpoint globals.  The goal is not to do any allocation when dealing with
// these midpoints, so we just keep an array around and never clear it.  We track
// the number of items and position using the two other variables.
static Vector<BidiIterator>* smidpoints;
static unsigned sNumMidpoints;
static unsigned sCurrMidpoint;
static bool betweenMidpoints;

static bool isLineEmpty = true;
static bool previousLineBrokeCleanly = true;
static bool emptyRun = true;
static int numSpaces;

static void embed(UCharDirection, BidiState&);
static void appendRun(BidiState&);
static void deleteBidiRuns(RenderArena*);

// FIXME: Merge leaks fix from r19843 for bidiReorderCharacters() if r19414 is ever merged

static int getBPMWidth(int childValue, Length cssUnit)
{
    if (!cssUnit.isIntrinsicOrAuto())
        return (cssUnit.isFixed() ? cssUnit.value() : childValue);
    return 0;
}

static int getBorderPaddingMargin(RenderObject* child, bool endOfInline)
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

static int inlineWidth(RenderObject* child, bool start = true, bool end = true)
{
    int extraWidth = 0;
    RenderObject* parent = child->parent();
    while (parent->isInline() && !parent->isInlineBlockOrInlineTable()) {
        if (start && parent->firstChild() == child)
            extraWidth += getBorderPaddingMargin(parent, false);
        if (end && parent->lastChild() == child)
            extraWidth += getBorderPaddingMargin(parent, true);
        child = parent;
        parent = child->parent();
    }
    return extraWidth;
}

#ifndef NDEBUG
WTFLogChannel LogWebCoreBidiRunLeaks =  { 0x00000000, "", WTFLogChannelOn };

struct BidiRunCounter { 
    static int count; 
    ~BidiRunCounter() 
    { 
        if (count)
            LOG(WebCoreBidiRunLeaks, "LEAK: %d BidiRun\n", count);
    }
};
int BidiRunCounter::count = 0;
static BidiRunCounter bidiRunCounter;

static bool inBidiRunDestroy;
#endif

void BidiRun::destroy(RenderArena* renderArena)
{
#ifndef NDEBUG
    inBidiRunDestroy = true;
#endif
    delete this;
#ifndef NDEBUG
    inBidiRunDestroy = false;
#endif

    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

void* BidiRun::operator new(size_t sz, RenderArena* renderArena) throw()
{
#ifndef NDEBUG
    ++BidiRunCounter::count;
#endif
    return renderArena->allocate(sz);
}

void BidiRun::operator delete(void* ptr, size_t sz)
{
#ifndef NDEBUG
    --BidiRunCounter::count;
#endif
    assert(inBidiRunDestroy);

    // Stash size where destroy() can find it.
    *(size_t*)ptr = sz;
}

static void deleteBidiRuns(RenderArena* arena)
{
    emptyRun = true;
    if (!sFirstBidiRun)
        return;

    BidiRun* curr = sFirstBidiRun;
    while (curr) {
        BidiRun* s = curr->nextRun;
        curr->destroy(arena);
        curr = s;
    }
    
    sFirstBidiRun = 0;
    sLastBidiRun = 0;
    sBidiRunCount = 0;
}

// ---------------------------------------------------------------------

/* a small helper class used internally to resolve Bidi embedding levels.
   Each line of text caches the embedding level at the start of the line for faster
   relayouting
*/
BidiContext::BidiContext(unsigned char l, UCharDirection e, BidiContext *p, bool o)
    : level(l), override(o), m_dir(e)
{
    parent = p;
    if (p) {
        p->ref();
        m_basicDir = p->basicDir();
    } else
        m_basicDir = e;
    count = 0;
}

BidiContext::~BidiContext()
{
    if (parent) 
        parent->deref();
}

void BidiContext::ref() const
{
    count++;
}

void BidiContext::deref() const
{
    count--;
    if (count <= 0)
        delete this;
}

bool operator==(const BidiContext& c1, const BidiContext& c2)
{
    if (&c1 == &c2)
        return true;
    if (c1.level != c2.level || c1.override != c2.override || c1.dir() != c2.dir() || c1.basicDir() != c2.basicDir())
        return false;
    if (!c1.parent)
        return !c2.parent;
    return c2.parent && *c1.parent == *c2.parent;
}

inline bool operator!=(const BidiContext& c1, const BidiContext& c2)
{
    return !(c1 == c2);
}

// ---------------------------------------------------------------------

inline bool operator==(const BidiIterator& it1, const BidiIterator& it2)
{
    if (it1.pos != it2.pos)
        return false;
    if (it1.obj != it2.obj)
        return false;
    return true;
}

inline bool operator!=(const BidiIterator& it1, const BidiIterator& it2)
{
    if (it1.pos != it2.pos)
        return true;
    if (it1.obj != it2.obj)
        return true;
    return false;
}

static inline RenderObject* bidiNext(RenderBlock* block, RenderObject* current, BidiState& bidi,
                                     bool skipInlines = true, bool* endOfInline = 0)
{
    RenderObject* next = 0;
    bool oldEndOfInline = endOfInline ? *endOfInline : false;
    if (endOfInline)
        *endOfInline = false;

    while (current) {
        if (!oldEndOfInline && !current->isFloating() && !current->isReplaced() && !current->isPositioned()) {
            next = current->firstChild();
            if (next && bidi.adjustEmbedding && next->isInlineFlow()) {
                EUnicodeBidi ub = next->style()->unicodeBidi();
                if (ub != UBNormal) {
                    TextDirection dir = next->style()->direction();
                    UCharDirection d = (ub == Embed
                        ? (dir == RTL ? U_RIGHT_TO_LEFT_EMBEDDING : U_LEFT_TO_RIGHT_EMBEDDING)
                        : (dir == RTL ? U_RIGHT_TO_LEFT_OVERRIDE : U_LEFT_TO_RIGHT_OVERRIDE));
                    embed(d, bidi);
                }
            }
        }

        if (!next) {
            if (!skipInlines && !oldEndOfInline && current->isInlineFlow()) {
                next = current;
                if (endOfInline)
                    *endOfInline = true;
                break;
            }

            while (current && current != block) {
                if (bidi.adjustEmbedding && current->isInlineFlow() && current->style()->unicodeBidi() != UBNormal)
                    embed(U_POP_DIRECTIONAL_FORMAT, bidi);

                next = current->nextSibling();
                if (next) {
                    if (bidi.adjustEmbedding && next->isInlineFlow()) {
                        EUnicodeBidi ub = next->style()->unicodeBidi();
                        if (ub != UBNormal) {
                            TextDirection dir = next->style()->direction();
                            UCharDirection d = (ub == Embed
                                ? (dir == RTL ? U_RIGHT_TO_LEFT_EMBEDDING : U_LEFT_TO_RIGHT_EMBEDDING)
                                : (dir == RTL ? U_RIGHT_TO_LEFT_OVERRIDE : U_LEFT_TO_RIGHT_OVERRIDE));
                            embed(d, bidi);
                        }
                    }
                    break;
                }
                
                current = current->parent();
                if (!skipInlines && current && current != block && current->isInlineFlow()) {
                    next = current;
                    if (endOfInline)
                        *endOfInline = true;
                    break;
                }
            }
        }

        if (!next)
            break;

        if (next->isText() || next->isBR() || next->isFloating() || next->isReplaced() || next->isPositioned()
            || ((!skipInlines || !next->firstChild()) // Always return EMPTY inlines.
                && next->isInlineFlow()))
            break;
        current = next;
    }
    return next;
}

static RenderObject* bidiFirst(RenderBlock* block, BidiState& bidi, bool skipInlines = true )
{
    if (!block->firstChild())
        return 0;
    
    RenderObject* o = block->firstChild();
    if (o->isInlineFlow()) {
        if (bidi.adjustEmbedding) {
            EUnicodeBidi ub = o->style()->unicodeBidi();
            if (ub != UBNormal) {
                TextDirection dir = o->style()->direction();
                UCharDirection d = (ub == Embed
                    ? (dir == RTL ? U_RIGHT_TO_LEFT_EMBEDDING : U_LEFT_TO_RIGHT_EMBEDDING)
                    : (dir == RTL ? U_RIGHT_TO_LEFT_OVERRIDE : U_LEFT_TO_RIGHT_OVERRIDE));
                embed(d, bidi);
            }
        }
        if (skipInlines && o->firstChild())
            o = bidiNext(block, o, bidi, skipInlines);
        else
            return o; // Never skip empty inlines.
    }

    if (o && !o->isText() && !o->isBR() && !o->isReplaced() && !o->isFloating() && !o->isPositioned())
        o = bidiNext(block, o, bidi, skipInlines);
    return o;
}

inline void BidiIterator::increment(BidiState& bidi)
{
    if (!obj)
        return;
    if (obj->isText()) {
        pos++;
        if (pos >= static_cast<RenderText *>(obj)->stringLength()) {
            obj = bidiNext(block, obj, bidi);
            pos = 0;
        }
    } else {
        obj = bidiNext(block, obj, bidi);
        pos = 0;
    }
}

inline bool BidiIterator::atEnd() const
{
    return !obj;
}

UChar BidiIterator::current() const
{
    if (!obj || !obj->isText())
        return 0;
    
    RenderText* text = static_cast<RenderText*>(obj);
    if (!text->text())
        return 0;
    
    return text->text()[pos];
}

ALWAYS_INLINE UCharDirection BidiIterator::direction() const
{
    if (!obj)
        return U_OTHER_NEUTRAL;
    if (obj->isListMarker())
        return obj->style()->direction() == LTR ? U_LEFT_TO_RIGHT : U_RIGHT_TO_LEFT;
    if (!obj->isText())
        return U_OTHER_NEUTRAL;
    RenderText* renderTxt = static_cast<RenderText*>(obj);
    if (pos >= renderTxt->stringLength())
        return U_OTHER_NEUTRAL;
    return u_charDirection(renderTxt->text()[pos]);
}

// -------------------------------------------------------------------------------------------------

static void addRun(BidiRun* bidiRun)
{
    if (!sFirstBidiRun)
        sFirstBidiRun = sLastBidiRun = bidiRun;
    else {
        sLastBidiRun->nextRun = bidiRun;
        sLastBidiRun = bidiRun;
    }
    sBidiRunCount++;
    bidiRun->compact = sBuildingCompactRuns;

    // Compute the number of spaces in this run,
    if (bidiRun->obj && bidiRun->obj->isText()) {
        RenderText* text = static_cast<RenderText*>(bidiRun->obj);
        if (text->text()) {
            for (int i = bidiRun->start; i < bidiRun->stop; i++) {
                UChar c = text->text()[i];
                if (c == ' ' || c == '\n' || c == '\t')
                    numSpaces++;
            }
        }
    }
}

static void reverseRuns(int start, int end)
{
    if (start >= end)
        return;

    assert(start >= 0 && end < sBidiRunCount);
    
    // Get the item before the start of the runs to reverse and put it in
    // |beforeStart|.  |curr| should point to the first run to reverse.
    BidiRun* curr = sFirstBidiRun;
    BidiRun* beforeStart = 0;
    int i = 0;
    while (i < start) {
        i++;
        beforeStart = curr;
        curr = curr->nextRun;
    }

    BidiRun* startRun = curr;
    while (i < end) {
        i++;
        curr = curr->nextRun;
    }
    BidiRun* endRun = curr;
    BidiRun* afterEnd = curr->nextRun;

    i = start;
    curr = startRun;
    BidiRun* newNext = afterEnd;
    while (i <= end) {
        // Do the reversal.
        BidiRun* next = curr->nextRun;
        curr->nextRun = newNext;
        newNext = curr;
        curr = next;
        i++;
    }

    // Now hook up beforeStart and afterEnd to the newStart and newEnd.
    if (beforeStart)
        beforeStart->nextRun = endRun;
    else
        sFirstBidiRun = endRun;

    startRun->nextRun = afterEnd;
    if (!afterEnd)
        sLastBidiRun = startRun;
}

static void chopMidpointsAt(RenderObject* obj, unsigned pos)
{
    if (!sNumMidpoints)
        return;
    BidiIterator* midpoints = smidpoints->data();
    for (unsigned i = 0; i < sNumMidpoints; i++) {
        const BidiIterator& point = midpoints[i];
        if (point.obj == obj && point.pos == pos) {
            sNumMidpoints = i;
            break;
        }
    }
}

static void checkMidpoints(BidiIterator& lBreak, BidiState& bidi)
{
    // Check to see if our last midpoint is a start point beyond the line break.  If so,
    // shave it off the list, and shave off a trailing space if the previous end point doesn't
    // preserve whitespace.
    if (lBreak.obj && sNumMidpoints && sNumMidpoints%2 == 0) {
        BidiIterator* midpoints = smidpoints->data();
        BidiIterator& endpoint = midpoints[sNumMidpoints-2];
        const BidiIterator& startpoint = midpoints[sNumMidpoints-1];
        BidiIterator currpoint = endpoint;
        while (!currpoint.atEnd() && currpoint != startpoint && currpoint != lBreak)
            currpoint.increment(bidi);
        if (currpoint == lBreak) {
            // We hit the line break before the start point.  Shave off the start point.
            sNumMidpoints--;
            if (endpoint.obj->style()->collapseWhiteSpace()) {
                if (endpoint.obj->isText()) {
                    // Don't shave a character off the endpoint if it was from a soft hyphen.
                    RenderText* textObj = static_cast<RenderText*>(endpoint.obj);
                    if (endpoint.pos+1 < textObj->length()) {
                        if (textObj->text()[endpoint.pos+1] == SOFT_HYPHEN)
                            return;
                    } else if (startpoint.obj->isText()) {
                        RenderText *startText = static_cast<RenderText*>(startpoint.obj);
                        if (startText->length() > 0 && startText->text()[0] == SOFT_HYPHEN)
                            return;
                    }
                }
                endpoint.pos--;
            }
        }
    }    
}

static void addMidpoint(const BidiIterator& midpoint)
{
    if (!smidpoints)
        return;

    if (smidpoints->size() <= sNumMidpoints)
        smidpoints->resize(sNumMidpoints + 10);

    BidiIterator* midpoints = smidpoints->data();
    midpoints[sNumMidpoints++] = midpoint;
}

static void appendRunsForObject(int start, int end, RenderObject* obj, BidiState &bidi)
{
    if (start > end || obj->isFloating() ||
        (obj->isPositioned() && !obj->hasStaticX() && !obj->hasStaticY() && !obj->container()->isInlineFlow()))
        return;

    bool haveNextMidpoint = (smidpoints && sCurrMidpoint < sNumMidpoints);
    BidiIterator nextMidpoint;
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
            return appendRunsForObject(start, end, obj, bidi);
    }
    else {
        if (!smidpoints || !haveNextMidpoint || (obj != nextMidpoint.obj)) {
            addRun(new (obj->renderArena()) BidiRun(start, end, obj, bidi.context.get(), bidi.dir));
            return;
        }
        
        // An end midpoint has been encountered within our object.  We
        // need to go ahead and append a run with our endpoint.
        if (int(nextMidpoint.pos+1) <= end) {
            betweenMidpoints = true;
            sCurrMidpoint++;
            if (nextMidpoint.pos != UINT_MAX) { // UINT_MAX means stop at the object and don't include any of it.
                if (int(nextMidpoint.pos+1) > start)
                    addRun(new (obj->renderArena())
                        BidiRun(start, nextMidpoint.pos+1, obj, bidi.context.get(), bidi.dir));
                return appendRunsForObject(nextMidpoint.pos+1, end, obj, bidi);
            }
        }
        else
           addRun(new (obj->renderArena()) BidiRun(start, end, obj, bidi.context.get(), bidi.dir));
    }
}

static void appendRun(BidiState &bidi)
{
    if (emptyRun || !bidi.eor.obj)
        return;
#if BIDI_DEBUG > 1
    kdDebug(6041) << "appendRun: dir="<<(int)dir<<endl;
#endif

    bool b = bidi.adjustEmbedding;
    bidi.adjustEmbedding = false;

    int start = bidi.sor.pos;
    RenderObject *obj = bidi.sor.obj;
    while (obj && obj != bidi.eor.obj && obj != bidi.endOfLine.obj) {
        appendRunsForObject(start, obj->length(), obj, bidi);        
        start = 0;
        obj = bidiNext(bidi.sor.block, obj, bidi);
    }
    if (obj) {
        unsigned pos = obj == bidi.eor.obj ? bidi.eor.pos : UINT_MAX;
        if (obj == bidi.endOfLine.obj && bidi.endOfLine.pos <= pos) {
            bidi.reachedEndOfLine = true;
            pos = bidi.endOfLine.pos;
        }
        // It's OK to add runs for zero-length RenderObjects, just don't make the run larger than it should be
        int end = obj->length() ? pos+1 : 0;
        appendRunsForObject(start, end, obj, bidi);
    }
    
    bidi.eor.increment(bidi);
    bidi.sor = bidi.eor;
    bidi.dir = U_OTHER_NEUTRAL;
    bidi.status.eor = U_OTHER_NEUTRAL;
    bidi.adjustEmbedding = b;
}

static void embed(UCharDirection d, BidiState& bidi)
{
    bool b = bidi.adjustEmbedding;
    bidi.adjustEmbedding = false;
    if (d == U_POP_DIRECTIONAL_FORMAT) {
        BidiContext *c = bidi.context->parent;
        if (c) {
            if (!emptyRun && bidi.eor != bidi.last) {
                assert(bidi.status.eor != U_OTHER_NEUTRAL);
                // bidi.sor ... bidi.eor ... bidi.last eor; need to append the bidi.sor-bidi.eor run or extend it through bidi.last
                assert(bidi.status.last == U_EUROPEAN_NUMBER_SEPARATOR
                    || bidi.status.last == U_EUROPEAN_NUMBER_TERMINATOR
                    || bidi.status.last == U_COMMON_NUMBER_SEPARATOR
                    || bidi.status.last == U_BOUNDARY_NEUTRAL
                    || bidi.status.last == U_BLOCK_SEPARATOR
                    || bidi.status.last == U_SEGMENT_SEPARATOR
                    || bidi.status.last == U_WHITE_SPACE_NEUTRAL
                    || bidi.status.last == U_OTHER_NEUTRAL);
                if (bidi.dir == U_OTHER_NEUTRAL)
                    bidi.dir = bidi.context->dir();
                if (bidi.context->dir() == U_LEFT_TO_RIGHT) {
                    // bidi.sor ... bidi.eor ... bidi.last L
                    if (bidi.status.eor == U_EUROPEAN_NUMBER) {
                        if (bidi.status.lastStrong != U_LEFT_TO_RIGHT) {
                            bidi.dir = U_EUROPEAN_NUMBER;
                            appendRun(bidi);
                        }
                    } else if (bidi.status.eor == U_ARABIC_NUMBER) {
                        bidi.dir = U_ARABIC_NUMBER;
                        appendRun(bidi);
                    } else if (bidi.status.eor != U_LEFT_TO_RIGHT)
                        appendRun(bidi);
                } else if (bidi.status.eor != U_RIGHT_TO_LEFT && bidi.status.eor != U_RIGHT_TO_LEFT_ARABIC)
                    appendRun(bidi);
                bidi.eor = bidi.last;
            }
            appendRun(bidi);
            emptyRun = true;
            // sor for the new run is determined by the higher level (rule X10)
            bidi.status.last = bidi.context->dir();
            bidi.status.lastStrong = bidi.context->dir();
            bidi.context = c;
            bidi.status.eor = bidi.context->dir();
            bidi.eor.obj = 0;
        }
    } else {
        UCharDirection runDir;
        if (d == U_RIGHT_TO_LEFT_EMBEDDING || d == U_RIGHT_TO_LEFT_OVERRIDE)
            runDir = U_RIGHT_TO_LEFT;
        else
            runDir = U_LEFT_TO_RIGHT;
        bool override = d == U_LEFT_TO_RIGHT_OVERRIDE || d == U_RIGHT_TO_LEFT_OVERRIDE;

        unsigned char level = bidi.context->level;
        if (runDir == U_RIGHT_TO_LEFT) {
            if (level%2) // we have an odd level
                level += 2;
            else
                level++;
        } else {
            if (level%2) // we have an odd level
                level++;
            else
                level += 2;
        }

        if (level < 61) {
            if (!emptyRun && bidi.eor != bidi.last) {
                assert(bidi.status.eor != U_OTHER_NEUTRAL);
                // bidi.sor ... bidi.eor ... bidi.last eor; need to append the bidi.sor-bidi.eor run or extend it through bidi.last
                assert(bidi.status.last == U_EUROPEAN_NUMBER_SEPARATOR
                    || bidi.status.last == U_EUROPEAN_NUMBER_TERMINATOR
                    || bidi.status.last == U_COMMON_NUMBER_SEPARATOR
                    || bidi.status.last == U_BOUNDARY_NEUTRAL
                    || bidi.status.last == U_BLOCK_SEPARATOR
                    || bidi.status.last == U_SEGMENT_SEPARATOR
                    || bidi.status.last == U_WHITE_SPACE_NEUTRAL
                    || bidi.status.last == U_OTHER_NEUTRAL);
                if (bidi.dir == U_OTHER_NEUTRAL)
                    bidi.dir = runDir;
                if (runDir == U_LEFT_TO_RIGHT) {
                    // bidi.sor ... bidi.eor ... bidi.last L
                    if (bidi.status.eor == U_EUROPEAN_NUMBER) {
                        if (bidi.status.lastStrong != U_LEFT_TO_RIGHT) {
                            bidi.dir = U_EUROPEAN_NUMBER;
                            appendRun(bidi);
                            if (bidi.context->dir() != U_LEFT_TO_RIGHT)
                                bidi.dir = U_RIGHT_TO_LEFT;
                        }
                    } else if (bidi.status.eor == U_ARABIC_NUMBER) {
                        bidi.dir = U_ARABIC_NUMBER;
                        appendRun(bidi);
                        if (bidi.context->dir() != U_LEFT_TO_RIGHT) {
                            bidi.eor = bidi.last;
                            bidi.dir = U_RIGHT_TO_LEFT;
                            appendRun(bidi);
                        }
                    } else if (bidi.status.eor != U_LEFT_TO_RIGHT) {
                        if (bidi.context->dir() == U_LEFT_TO_RIGHT || bidi.status.lastStrong == U_LEFT_TO_RIGHT)
                            appendRun(bidi);
                        else
                            bidi.dir = U_RIGHT_TO_LEFT; 
                    }
                } else if (bidi.status.eor != U_RIGHT_TO_LEFT && bidi.status.eor != U_RIGHT_TO_LEFT_ARABIC) {
                    // bidi.sor ... bidi.eor ... bidi.last R; bidi.eor=L/EN/AN; EN,AN behave like R (rule N1)
                    if (bidi.context->dir() == U_RIGHT_TO_LEFT || bidi.status.lastStrong == U_RIGHT_TO_LEFT || bidi.status.lastStrong == U_RIGHT_TO_LEFT_ARABIC)
                        appendRun(bidi);
                    else
                        bidi.dir = U_LEFT_TO_RIGHT;
                }
                bidi.eor = bidi.last;
            }
            appendRun(bidi);
            emptyRun = true;
            bidi.context = new BidiContext(level, runDir, bidi.context.get(), override);
            bidi.status.last = runDir;
            bidi.status.lastStrong = runDir;
            bidi.status.eor = runDir;
            bidi.eor.obj = 0;
        }
    }
    bidi.adjustEmbedding = b;
}

InlineFlowBox* RenderBlock::createLineBoxes(RenderObject* obj)
{
    // See if we have an unconstructed line box for this object that is also
    // the last item on the line.
    ASSERT(obj->isInlineFlow() || obj == this);
    RenderFlow* flow = static_cast<RenderFlow*>(obj);

    // Get the last box we made for this render object.
    InlineFlowBox* box = flow->lastLineBox();

    // If this box is constructed then it is from a previous line, and we need
    // to make a new box for our line.  If this box is unconstructed but it has
    // something following it on the line, then we know we have to make a new box
    // as well.  In this situation our inline has actually been split in two on
    // the same line (this can happen with very fancy language mixtures).
    if (!box || box->isConstructed() || box->nextOnLine()) {
        // We need to make a new box for this render object.  Once
        // made, we need to place it at the end of the current line.
        InlineBox* newBox = obj->createInlineBox(false, obj == this);
        ASSERT(newBox->isInlineFlowBox());
        box = static_cast<InlineFlowBox*>(newBox);
        box->setFirstLineStyleBit(m_firstLine);
        
        // We have a new box. Append it to the inline box we get by constructing our
        // parent.  If we have hit the block itself, then |box| represents the root
        // inline box for the line, and it doesn't have to be appended to any parent
        // inline.
        if (obj != this) {
            InlineFlowBox* parentBox = createLineBoxes(obj->parent());
            parentBox->addToLine(box);
        }
    }

    return box;
}

RootInlineBox* RenderBlock::constructLine(const BidiIterator& start, const BidiIterator& end)
{
    if (!sFirstBidiRun)
        return 0; // We had no runs. Don't make a root inline box at all. The line is empty.

    InlineFlowBox* parentBox = 0;
    for (BidiRun* r = sFirstBidiRun; r; r = r->nextRun) {
        // Create a box for our object.
        bool isOnlyRun = (sBidiRunCount == 1);
        if (sBidiRunCount == 2 && !r->obj->isListMarker())
            isOnlyRun = ((style()->direction() == RTL) ? sLastBidiRun : sFirstBidiRun)->obj->isListMarker();
        r->box = r->obj->createInlineBox(r->obj->isPositioned(), false, isOnlyRun);
        if (r->box) {
            // If we have no parent box yet, or if the run is not simply a sibling,
            // then we need to construct inline boxes as necessary to properly enclose the
            // run's inline box.
            if (!parentBox || parentBox->object() != r->obj->parent())
                // Create new inline boxes all the way back to the appropriate insertion point.
                parentBox = createLineBoxes(r->obj->parent());

            // Append the inline box to this line.
            parentBox->addToLine(r->box);
            
            if (r->box->isInlineTextBox()) {
                InlineTextBox *text = static_cast<InlineTextBox*>(r->box);
                text->setStart(r->start);
                text->setLen(r->stop-r->start);
            }
        }
    }

    // We should have a root inline box.  It should be unconstructed and
    // be the last continuation of our line list.
    assert(lastLineBox() && !lastLineBox()->isConstructed());

    // Set bits on our inline flow boxes that indicate which sides should
    // paint borders/margins/padding.  This knowledge will ultimately be used when
    // we determine the horizontal positions and widths of all the inline boxes on
    // the line.
    RenderObject* endObject = 0;
    bool lastLine = !end.obj;
    if (end.obj && end.pos == 0)
        endObject = end.obj;
    lastLineBox()->determineSpacingForFlowBoxes(lastLine, endObject);

    // Now mark the line boxes as being constructed.
    lastLineBox()->setConstructed();

    // Return the last line.
    return lastRootBox();
}

// usage: tw - (xpos % tw);
int RenderBlock::tabWidth(bool isWhitespacePre)
{
    if (!isWhitespacePre)
        return 0;

    if (m_tabWidth == -1) {
        const UChar spaceChar = ' ';
        const Font& font = style()->font();
        int spaceWidth = font.width(TextRun(&spaceChar, 1));
        m_tabWidth = spaceWidth * 8;
    }

    return m_tabWidth;
}

void RenderBlock::computeHorizontalPositionsForLine(RootInlineBox* lineBox, BidiState& bidi)
{
    // First determine our total width.
    int availableWidth = lineWidth(m_height);
    int totWidth = lineBox->getFlowSpacingWidth();
    BidiRun* r = 0;
    bool needsWordSpacing = false;
    for (r = sFirstBidiRun; r; r = r->nextRun) {
        if (!r->box || r->obj->isPositioned() || r->box->isLineBreak())
            continue; // Positioned objects are only participating to figure out their
                      // correct static x position.  They have no effect on the width.
                      // Similarly, line break boxes have no effect on the width.
        if (r->obj->isText()) {
            RenderText* rt = static_cast<RenderText*>(r->obj);
            int textWidth = rt->width(r->start, r->stop-r->start, totWidth, m_firstLine);
            int effectiveWidth = textWidth;
            int rtLength = rt->length();
            if (rtLength != 0) {
                if (r->start == 0 && needsWordSpacing && DeprecatedChar(rt->text()[r->start]).isSpace())
                    effectiveWidth += rt->font(m_firstLine)->wordSpacing();
                needsWordSpacing = !DeprecatedChar(rt->text()[r->stop-1]).isSpace() && r->stop == rtLength;          
            }
            r->box->setWidth(textWidth);
        } else if (!r->obj->isInlineFlow()) {
            r->obj->calcWidth();
            r->box->setWidth(r->obj->width());
            if (!r->compact)
                 totWidth += r->obj->marginLeft() + r->obj->marginRight();
        }

        // Compacts don't contribute to the width of the line, since they are placed in the margin.
        if (!r->compact)
            totWidth += r->box->width();
    }

    if (totWidth > availableWidth && sLogicallyLastBidiRun->obj->style(m_firstLine)->autoWrap() &&
        sLogicallyLastBidiRun->obj->style(m_firstLine)->breakOnlyAfterWhiteSpace() &&
        !sLogicallyLastBidiRun->compact) {
        sLogicallyLastBidiRun->box->setWidth(sLogicallyLastBidiRun->box->width() - totWidth + availableWidth);
        totWidth = availableWidth;
    }

    // Armed with the total width of the line (without justification),
    // we now examine our text-align property in order to determine where to position the
    // objects horizontally.  The total width of the line can be increased if we end up
    // justifying text.
    int x = leftOffset(m_height);
    switch(style()->textAlign()) {
        case LEFT:
        case WEBKIT_LEFT:
            // The direction of the block should determine what happens with wide lines.  In
            // particular with RTL blocks, wide lines should still spill out to the left.
            if (style()->direction() == RTL && totWidth > availableWidth)
                x -= (totWidth - availableWidth);
            numSpaces = 0;
            break;
        case JUSTIFY:
            if (numSpaces != 0 && !bidi.current.atEnd() && !lineBox->endsWithBreak())
                break;
            // fall through
        case TAAUTO:
            numSpaces = 0;
            // for right to left fall through to right aligned
            if (bidi.context->basicDir() == U_LEFT_TO_RIGHT)
                break;
        case RIGHT:
        case WEBKIT_RIGHT:
            // Wide lines spill out of the block based off direction.
            // So even if text-align is right, if direction is LTR, wide lines should overflow out of the right
            // side of the block.
            if (style()->direction() == RTL || totWidth < availableWidth)
                x += availableWidth - totWidth;
            numSpaces = 0;
            break;
        case CENTER:
        case WEBKIT_CENTER:
            int xd = (availableWidth - totWidth)/2;
            x += xd > 0 ? xd : 0;
            numSpaces = 0;
            break;
    }

    if (numSpaces > 0) {
        for (r = sFirstBidiRun; r; r = r->nextRun) {
            if (!r->box) continue;

            int spaceAdd = 0;
            if (numSpaces > 0 && r->obj->isText() && !r->compact) {
                // get the number of spaces in the run
                int spaces = 0;
                for ( int i = r->start; i < r->stop; i++ ) {
                    UChar c = static_cast<RenderText*>(r->obj)->text()[i];
                    if (c == ' ' || c == '\n' || c == '\t')
                        spaces++;
                }

                ASSERT(spaces <= numSpaces);

                // Only justify text if whitespace is collapsed.
                if (r->obj->style()->collapseWhiteSpace()) {
                    spaceAdd = (availableWidth - totWidth)*spaces/numSpaces;
                    static_cast<InlineTextBox*>(r->box)->setSpaceAdd(spaceAdd);
                    totWidth += spaceAdd;
                }
                numSpaces -= spaces;
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

void RenderBlock::computeVerticalPositionsForLine(RootInlineBox* lineBox)
{
    lineBox->verticallyAlignBoxes(m_height);
    lineBox->setBlockHeight(m_height);

    // See if the line spilled out.  If so set overflow height accordingly.
    int bottomOfLine = lineBox->bottomOverflow();
    if (bottomOfLine > m_height && bottomOfLine > m_overflowHeight)
        m_overflowHeight = bottomOfLine;
        
    // Now make sure we place replaced render objects correctly.
    for (BidiRun* r = sFirstBidiRun; r; r = r->nextRun) {
        if (!r->box)
            continue; // Skip runs with no line boxes.

        // Align positioned boxes with the top of the line box.  This is
        // a reasonable approximation of an appropriate y position.
        if (r->obj->isPositioned())
            r->box->setYPos(m_height);

        // Position is used to properly position both replaced elements and
        // to update the static normal flow x/y of positioned elements.
        r->obj->position(r->box, r->start, r->stop - r->start, r->level%2, r->override);
    }
}

// collects one line of the paragraph and transforms it to visual order
void RenderBlock::bidiReorderLine(const BidiIterator& start, const BidiIterator& end, BidiState& bidi)
{
    if (start == end) {
        if (start.current() == '\n')
            m_height += lineHeight(m_firstLine, true);
        return;
    }

    sFirstBidiRun = 0;
    sLastBidiRun = 0;
    sBidiRunCount = 0;

    assert(bidi.dir == U_OTHER_NEUTRAL);

    emptyRun = true;

    bidi.eor.obj = 0;

    numSpaces = 0;

    bidi.current = start;
    bidi.last = bidi.current;
    bool pastEnd = false;
    BidiState stateAtEnd;

    while (true) {
        UCharDirection dirCurrent;
        if (pastEnd && (previousLineBrokeCleanly || bidi.current.atEnd())) {
            BidiContext *c = bidi.context.get();
            while (c->parent)
                c = c->parent;
            dirCurrent = c->dir();
            if (previousLineBrokeCleanly) {
                // A deviation from the Unicode Bidi Algorithm in order to match
                // Mac OS X text and WinIE: a hard line break resets bidi state.
                stateAtEnd.context = c;
                stateAtEnd.status.eor = dirCurrent;
                stateAtEnd.status.last = dirCurrent;
                stateAtEnd.status.lastStrong = dirCurrent;
            }
        } else {
            dirCurrent = bidi.current.direction();
            if (bidi.context->override
                    && dirCurrent != U_RIGHT_TO_LEFT_EMBEDDING
                    && dirCurrent != U_LEFT_TO_RIGHT_EMBEDDING
                    && dirCurrent != U_RIGHT_TO_LEFT_OVERRIDE
                    && dirCurrent != U_LEFT_TO_RIGHT_OVERRIDE
                    && dirCurrent != U_POP_DIRECTIONAL_FORMAT)
                dirCurrent = bidi.context->dir();
            else if (dirCurrent == U_DIR_NON_SPACING_MARK)
                dirCurrent = bidi.status.last;
        }

        assert(bidi.status.eor != U_OTHER_NEUTRAL);
        switch (dirCurrent) {

        // embedding and overrides (X1-X9 in the Bidi specs)
        case U_RIGHT_TO_LEFT_EMBEDDING:
        case U_LEFT_TO_RIGHT_EMBEDDING:
        case U_RIGHT_TO_LEFT_OVERRIDE:
        case U_LEFT_TO_RIGHT_OVERRIDE:
        case U_POP_DIRECTIONAL_FORMAT:
            embed(dirCurrent, bidi);
            break;

            // strong types
        case U_LEFT_TO_RIGHT:
            switch(bidi.status.last) {
                case U_RIGHT_TO_LEFT:
                case U_RIGHT_TO_LEFT_ARABIC:
                case U_EUROPEAN_NUMBER:
                case U_ARABIC_NUMBER:
                    if (bidi.status.last != U_EUROPEAN_NUMBER || bidi.status.lastStrong != U_LEFT_TO_RIGHT)
                        appendRun(bidi);
                    break;
                case U_LEFT_TO_RIGHT:
                    break;
                case U_EUROPEAN_NUMBER_SEPARATOR:
                case U_EUROPEAN_NUMBER_TERMINATOR:
                case U_COMMON_NUMBER_SEPARATOR:
                case U_BOUNDARY_NEUTRAL:
                case U_BLOCK_SEPARATOR:
                case U_SEGMENT_SEPARATOR:
                case U_WHITE_SPACE_NEUTRAL:
                case U_OTHER_NEUTRAL:
                    if (bidi.status.eor == U_EUROPEAN_NUMBER) {
                        if (bidi.status.lastStrong != U_LEFT_TO_RIGHT) {
                            // the numbers need to be on a higher embedding level, so let's close that run
                            bidi.dir = U_EUROPEAN_NUMBER;
                            appendRun(bidi);
                            if (bidi.context->dir() != U_LEFT_TO_RIGHT) {
                                // the neutrals take the embedding direction, which is R
                                bidi.eor = bidi.last;
                                bidi.dir = U_RIGHT_TO_LEFT;
                                appendRun(bidi);
                            }
                        }
                    } else if (bidi.status.eor == U_ARABIC_NUMBER) {
                        // Arabic numbers are always on a higher embedding level, so let's close that run
                        bidi.dir = U_ARABIC_NUMBER;
                        appendRun(bidi);
                        if (bidi.context->dir() != U_LEFT_TO_RIGHT) {
                            // the neutrals take the embedding direction, which is R
                            bidi.eor = bidi.last;
                            bidi.dir = U_RIGHT_TO_LEFT;
                            appendRun(bidi);
                        }
                    } else if(bidi.status.eor != U_LEFT_TO_RIGHT) {
                        //last stuff takes embedding dir
                        if (bidi.context->dir() != U_LEFT_TO_RIGHT && bidi.status.lastStrong != U_LEFT_TO_RIGHT) {
                            bidi.eor = bidi.last; 
                            bidi.dir = U_RIGHT_TO_LEFT; 
                        }
                        appendRun(bidi); 
                    }
                default:
                    break;
            }
            bidi.eor = bidi.current;
            bidi.status.eor = U_LEFT_TO_RIGHT;
            bidi.status.lastStrong = U_LEFT_TO_RIGHT;
            bidi.dir = U_LEFT_TO_RIGHT;
            break;
        case U_RIGHT_TO_LEFT_ARABIC:
        case U_RIGHT_TO_LEFT:
            switch (bidi.status.last) {
                case U_LEFT_TO_RIGHT:
                case U_EUROPEAN_NUMBER:
                case U_ARABIC_NUMBER:
                    appendRun(bidi);
                case U_RIGHT_TO_LEFT:
                case U_RIGHT_TO_LEFT_ARABIC:
                    break;
                case U_EUROPEAN_NUMBER_SEPARATOR:
                case U_EUROPEAN_NUMBER_TERMINATOR:
                case U_COMMON_NUMBER_SEPARATOR:
                case U_BOUNDARY_NEUTRAL:
                case U_BLOCK_SEPARATOR:
                case U_SEGMENT_SEPARATOR:
                case U_WHITE_SPACE_NEUTRAL:
                case U_OTHER_NEUTRAL:
                    if (bidi.status.eor != U_RIGHT_TO_LEFT && bidi.status.eor != U_RIGHT_TO_LEFT_ARABIC) {
                        //last stuff takes embedding dir
                        if (bidi.context->dir() != U_RIGHT_TO_LEFT && bidi.status.lastStrong != U_RIGHT_TO_LEFT 
                            && bidi.status.lastStrong != U_RIGHT_TO_LEFT_ARABIC) {
                            bidi.eor = bidi.last;
                            bidi.dir = U_LEFT_TO_RIGHT; 
                        }
                        appendRun(bidi);
                    }
                default:
                    break;
            }
            bidi.eor = bidi.current;
            bidi.status.eor = U_RIGHT_TO_LEFT;
            bidi.status.lastStrong = dirCurrent;
            bidi.dir = U_RIGHT_TO_LEFT;
            break;

            // weak types:

        case U_EUROPEAN_NUMBER:
            if (bidi.status.lastStrong != U_RIGHT_TO_LEFT_ARABIC) {
                // if last strong was AL change EN to AN
                switch (bidi.status.last) {
                    case U_EUROPEAN_NUMBER:
                    case U_LEFT_TO_RIGHT:
                        break;
                    case U_RIGHT_TO_LEFT:
                    case U_RIGHT_TO_LEFT_ARABIC:
                    case U_ARABIC_NUMBER:
                        bidi.eor = bidi.last;
                        appendRun(bidi);
                        bidi.dir = U_EUROPEAN_NUMBER;
                        break;
                    case U_EUROPEAN_NUMBER_SEPARATOR:
                    case U_COMMON_NUMBER_SEPARATOR:
                        if (bidi.status.eor == U_EUROPEAN_NUMBER)
                            break;
                    case U_EUROPEAN_NUMBER_TERMINATOR:
                    case U_BOUNDARY_NEUTRAL:
                    case U_BLOCK_SEPARATOR:
                    case U_SEGMENT_SEPARATOR:
                    case U_WHITE_SPACE_NEUTRAL:
                    case U_OTHER_NEUTRAL:
                        if (bidi.status.eor == U_RIGHT_TO_LEFT) {
                            // neutrals go to R
                            bidi.eor = bidi.status.last == U_EUROPEAN_NUMBER_TERMINATOR ? bidi.lastBeforeET : bidi.last;
                            appendRun(bidi);
                            bidi.dir = U_EUROPEAN_NUMBER;
                        } else if (bidi.status.eor != U_LEFT_TO_RIGHT &&
                                 (bidi.status.eor != U_EUROPEAN_NUMBER || bidi.status.lastStrong != U_LEFT_TO_RIGHT) &&
                                 bidi.dir != U_LEFT_TO_RIGHT) {
                            // numbers on both sides, neutrals get right to left direction
                            appendRun(bidi);
                            bidi.eor = bidi.status.last == U_EUROPEAN_NUMBER_TERMINATOR ? bidi.lastBeforeET : bidi.last;
                            bidi.dir = U_RIGHT_TO_LEFT;
                            appendRun(bidi);
                            bidi.dir = U_EUROPEAN_NUMBER;
                        }
                    default:
                        break;
                }
                bidi.eor = bidi.current;
                bidi.status.eor = U_EUROPEAN_NUMBER;
                if (bidi.dir == U_OTHER_NEUTRAL)
                    bidi.dir = U_LEFT_TO_RIGHT;
                break;
            }
        case U_ARABIC_NUMBER:
            dirCurrent = U_ARABIC_NUMBER;
            switch (bidi.status.last) {
                case U_LEFT_TO_RIGHT:
                    if (bidi.context->dir() == U_LEFT_TO_RIGHT)
                        appendRun(bidi);
                    break;
                case U_ARABIC_NUMBER:
                    break;
                case U_RIGHT_TO_LEFT:
                case U_RIGHT_TO_LEFT_ARABIC:
                case U_EUROPEAN_NUMBER:
                    bidi.eor = bidi.last;
                    appendRun(bidi);
                    break;
                case U_COMMON_NUMBER_SEPARATOR:
                    if (bidi.status.eor == U_ARABIC_NUMBER)
                        break;
                case U_EUROPEAN_NUMBER_SEPARATOR:
                case U_EUROPEAN_NUMBER_TERMINATOR:
                case U_BOUNDARY_NEUTRAL:
                case U_BLOCK_SEPARATOR:
                case U_SEGMENT_SEPARATOR:
                case U_WHITE_SPACE_NEUTRAL:
                case U_OTHER_NEUTRAL:
                    if (bidi.status.eor != U_RIGHT_TO_LEFT && bidi.status.eor != U_RIGHT_TO_LEFT_ARABIC) {
                        // run of L before neutrals, neutrals take embedding dir (N2)
                        if (bidi.context->dir() == U_RIGHT_TO_LEFT || bidi.status.lastStrong == U_RIGHT_TO_LEFT 
                            || bidi.status.lastStrong == U_RIGHT_TO_LEFT_ARABIC) { 
                            // the embedding direction is R
                            // close the L run
                            appendRun(bidi);
                            // neutrals become an R run
                            bidi.dir = U_RIGHT_TO_LEFT;
                        } else {
                            // the embedding direction is L
                            // append neutrals to the L run and close it
                            bidi.dir = U_LEFT_TO_RIGHT; 
                        }
                    }
                    bidi.eor = bidi.last;
                    appendRun(bidi);
                default:
                    break;
            }
            bidi.eor = bidi.current;
            bidi.status.eor = U_ARABIC_NUMBER;
            if (bidi.dir == U_OTHER_NEUTRAL)
                bidi.dir = U_ARABIC_NUMBER;
            break;
        case U_EUROPEAN_NUMBER_SEPARATOR:
        case U_COMMON_NUMBER_SEPARATOR:
            break;
        case U_EUROPEAN_NUMBER_TERMINATOR:
            if (bidi.status.last == U_EUROPEAN_NUMBER) {
                dirCurrent = U_EUROPEAN_NUMBER;
                bidi.eor = bidi.current;
                bidi.status.eor = dirCurrent;
            } else if (bidi.status.last != U_EUROPEAN_NUMBER_TERMINATOR)
                bidi.lastBeforeET = emptyRun ? bidi.eor : bidi.last;
            break;

        // boundary neutrals should be ignored
        case U_BOUNDARY_NEUTRAL:
            if (bidi.eor == bidi.last)
                bidi.eor = bidi.current;
            break;
            // neutrals
        case U_BLOCK_SEPARATOR:
            // ### what do we do with newline and paragraph seperators that come to here?
            break;
        case U_SEGMENT_SEPARATOR:
            // ### implement rule L1
            break;
        case U_WHITE_SPACE_NEUTRAL:
            break;
        case U_OTHER_NEUTRAL:
            break;
        default:
            break;
        }

        if (pastEnd) {
            if (bidi.eor == bidi.current) {
                if (!bidi.reachedEndOfLine) {
                    bidi.eor = bidi.endOfLine;
                    switch (bidi.status.eor) {
                        case U_LEFT_TO_RIGHT:
                        case U_RIGHT_TO_LEFT:
                        case U_ARABIC_NUMBER:
                            bidi.dir = bidi.status.eor;
                            break;
                        case U_EUROPEAN_NUMBER:
                            bidi.dir = bidi.status.lastStrong == U_LEFT_TO_RIGHT ? U_LEFT_TO_RIGHT : U_EUROPEAN_NUMBER;
                            break;
                        default:
                            assert(false);
                    }
                    appendRun(bidi);
                }
                bidi = stateAtEnd;
                bidi.dir = U_OTHER_NEUTRAL;
                break;
            }
        }

        // set status.last as needed.
        switch (dirCurrent) {
            case U_EUROPEAN_NUMBER_TERMINATOR:
                if (bidi.status.last != U_EUROPEAN_NUMBER)
                    bidi.status.last = U_EUROPEAN_NUMBER_TERMINATOR;
                break;
            case U_EUROPEAN_NUMBER_SEPARATOR:
            case U_COMMON_NUMBER_SEPARATOR:
            case U_SEGMENT_SEPARATOR:
            case U_WHITE_SPACE_NEUTRAL:
            case U_OTHER_NEUTRAL:
                switch(bidi.status.last) {
                    case U_LEFT_TO_RIGHT:
                    case U_RIGHT_TO_LEFT:
                    case U_RIGHT_TO_LEFT_ARABIC:
                    case U_EUROPEAN_NUMBER:
                    case U_ARABIC_NUMBER:
                        bidi.status.last = dirCurrent;
                        break;
                    default:
                        bidi.status.last = U_OTHER_NEUTRAL;
                    }
                break;
            case U_DIR_NON_SPACING_MARK:
            case U_BOUNDARY_NEUTRAL:
            case U_RIGHT_TO_LEFT_EMBEDDING:
            case U_LEFT_TO_RIGHT_EMBEDDING:
            case U_RIGHT_TO_LEFT_OVERRIDE:
            case U_LEFT_TO_RIGHT_OVERRIDE:
            case U_POP_DIRECTIONAL_FORMAT:
                // ignore these
                break;
            case U_EUROPEAN_NUMBER:
                // fall through
            default:
                bidi.status.last = dirCurrent;
        }

        bidi.last = bidi.current;

        if (emptyRun && !(dirCurrent == U_RIGHT_TO_LEFT_EMBEDDING
                || dirCurrent == U_LEFT_TO_RIGHT_EMBEDDING
                || dirCurrent == U_RIGHT_TO_LEFT_OVERRIDE
                || dirCurrent == U_LEFT_TO_RIGHT_OVERRIDE
                || dirCurrent == U_POP_DIRECTIONAL_FORMAT)) {
            bidi.sor = bidi.current;
            emptyRun = false;
        }

        // this causes the operator ++ to open and close embedding levels as needed
        // for the CSS unicode-bidi property
        bidi.adjustEmbedding = true;
        bidi.current.increment(bidi);
        bidi.adjustEmbedding = false;
        if (emptyRun && (dirCurrent == U_RIGHT_TO_LEFT_EMBEDDING
                || dirCurrent == U_LEFT_TO_RIGHT_EMBEDDING
                || dirCurrent == U_RIGHT_TO_LEFT_OVERRIDE
                || dirCurrent == U_LEFT_TO_RIGHT_OVERRIDE
                || dirCurrent == U_POP_DIRECTIONAL_FORMAT)) {
            // exclude the embedding char itself from the new run so that ATSUI will never see it
            bidi.eor.obj = 0;
            bidi.last = bidi.current;
            bidi.sor = bidi.current;
        }

        if (!pastEnd && (bidi.current == end || bidi.current.atEnd())) {
            if (emptyRun)
                break;
            stateAtEnd = bidi;
            bidi.endOfLine = bidi.last;
            pastEnd = true;
        }
    }

    sLogicallyLastBidiRun = sLastBidiRun;

    // reorder line according to run structure...
    // do not reverse for visually ordered web sites
    if (!style()->visuallyOrdered()) {

        // first find highest and lowest levels
        unsigned char levelLow = 128;
        unsigned char levelHigh = 0;
        BidiRun* r = sFirstBidiRun;
        while (r) {
            if (r->level > levelHigh)
                levelHigh = r->level;
            if (r->level < levelLow)
                levelLow = r->level;
            r = r->nextRun;
        }

        // implements reordering of the line (L2 according to Bidi spec):
        // L2. From the highest level found in the text to the lowest odd level on each line,
        // reverse any contiguous sequence of characters that are at that level or higher.

        // reversing is only done up to the lowest odd level
        if (!(levelLow%2))
            levelLow++;

        int count = sBidiRunCount - 1;

        while (levelHigh >= levelLow) {
            int i = 0;
            BidiRun* currRun = sFirstBidiRun;
            while (i < count) {
                while (i < count && currRun && currRun->level < levelHigh) {
                    i++;
                    currRun = currRun->nextRun;
                }
                int start = i;
                while (i <= count && currRun && currRun->level >= levelHigh) {
                    i++;
                    currRun = currRun->nextRun;
                }
                int end = i-1;
                reverseRuns(start, end);
            }
            levelHigh--;
        }
    }
    bidi.endOfLine.obj = 0;
}

static void buildCompactRuns(RenderObject* compactObj, BidiState& bidi)
{
    sBuildingCompactRuns = true;
    if (!compactObj->isRenderBlock()) {
        // Just append a run for our object.
        isLineEmpty = false;
        addRun(new (compactObj->renderArena()) BidiRun(0, compactObj->length(), compactObj, bidi.context.get(), bidi.dir));
    }
    else {
        // Format the compact like it is its own single line.  We build up all the runs for
        // the little compact and then reorder them for bidi.
        RenderBlock* compactBlock = static_cast<RenderBlock*>(compactObj);
        bidi.adjustEmbedding = true;
        BidiIterator start(compactBlock, bidiFirst(compactBlock, bidi), 0);
        bidi.adjustEmbedding = false;
        BidiIterator end = start;
    
        betweenMidpoints = false;
        isLineEmpty = true;
        previousLineBrokeCleanly = true;
        
        end = compactBlock->findNextLineBreak(start, bidi);
        if (!isLineEmpty)
            compactBlock->bidiReorderLine(start, end, bidi);
    }

    sCompactFirstBidiRun = sFirstBidiRun;
    sCompactLastBidiRun = sLastBidiRun;
    sCompactBidiRunCount = sBidiRunCount;
    
    sNumMidpoints = 0;
    sCurrMidpoint = 0;
    betweenMidpoints = false;
    sBuildingCompactRuns = false;
}

IntRect RenderBlock::layoutInlineChildren(bool relayoutChildren)
{
    BidiState bidi;

    bool useRepaintRect = false;
    IntRect repaintRect(0,0,0,0);

    m_overflowHeight = 0;
    
    invalidateVerticalPositions();
    
    m_height = borderTop() + paddingTop();
    int toAdd = borderBottom() + paddingBottom();
    if (includeHorizontalScrollbarSize())
        toAdd += m_layer->horizontalScrollbarHeight();
    
    // Figure out if we should clear out our line boxes.
    // FIXME: Handle resize eventually!
    // FIXME: Do something better when floats are present.
    bool fullLayout = !firstLineBox() || !firstChild() || selfNeedsLayout() || relayoutChildren || containsFloats();
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

    int oldLineBottom = lastRootBox() ? lastRootBox()->bottomOverflow() : m_height;
    int startLineBottom = 0;

    if (firstChild()) {
        // layout replaced elements
        bool endOfInline = false;
        RenderObject *o = bidiFirst(this, bidi, false);
        bool hasFloat = false;
        while (o) {
            if (o->isReplaced() || o->isFloating() || o->isPositioned()) {
                if (relayoutChildren || o->style()->width().isPercent() || o->style()->height().isPercent())
                    o->setChildNeedsLayout(true, false);
                if (o->isPositioned())
                    o->containingBlock()->insertPositionedObject(o);
                else {
                    if (o->isFloating())
                        hasFloat = true;
                    else if (fullLayout || o->needsLayout()) // Replaced elements
                        o->dirtyLineBoxes(fullLayout);
                    o->layoutIfNeeded();
                }
            }
            else if (o->isText() || (o->isInlineFlow() && !endOfInline)) {
                if (fullLayout || o->selfNeedsLayout())
                    o->dirtyLineBoxes(fullLayout);
                o->setNeedsLayout(false);
            }
            o = bidiNext(this, o, bidi, false, &endOfInline);
        }

        if (hasFloat)
            fullLayout = true; // FIXME: Will need to find a way to optimize floats some day.
        
        if (fullLayout && !selfNeedsLayout()) {
            setNeedsLayout(true, false);  // Mark ourselves as needing a full layout. This way we'll repaint like
                                          // we're supposed to.
            if (!document()->view()->needsFullRepaint() && m_layer) {
                // Because we waited until we were already inside layout to discover
                // that the block really needed a full layout, we missed our chance to repaint the layer
                // before layout started.  Luckily the layer has cached the repaint rect for its original
                // position and size, and so we can use that to make a repaint happen now.
                RenderView* c = view();
                if (c && !c->printingMode())
                    c->repaintViewRectangle(m_layer->repaintRect());
            }
        }

        BidiContext *startEmbed;
        if (style()->direction() == LTR) {
            startEmbed = new BidiContext( 0, U_LEFT_TO_RIGHT, NULL, style()->unicodeBidi() == Override );
            bidi.status.eor = U_LEFT_TO_RIGHT;
        } else {
            startEmbed = new BidiContext( 1, U_RIGHT_TO_LEFT, NULL, style()->unicodeBidi() == Override );
            bidi.status.eor = U_RIGHT_TO_LEFT;
        }

        bidi.status.lastStrong = startEmbed->dir();
        bidi.status.last = startEmbed->dir();
        bidi.status.eor = startEmbed->dir();
        bidi.context = startEmbed;
        bidi.dir = U_OTHER_NEUTRAL;
        
        if (!smidpoints)
            smidpoints = new Vector<BidiIterator>();
        
        sNumMidpoints = 0;
        sCurrMidpoint = 0;
        sCompactFirstBidiRun = sCompactLastBidiRun = 0;
        sCompactBidiRunCount = 0;
        
        // We want to skip ahead to the first dirty line
        BidiIterator start;
        RootInlineBox* startLine = determineStartPosition(fullLayout, start, bidi);
        
        // We also find the first clean line and extract these lines.  We will add them back
        // if we determine that we're able to synchronize after handling all our dirty lines.
        BidiIterator cleanLineStart;
        BidiStatus cleanLineBidiStatus;
        BidiContext* cleanLineBidiContext;
        int endLineYPos;
        RootInlineBox* endLine = (fullLayout || !startLine) ? 
                                 0 : determineEndPosition(startLine, cleanLineStart, cleanLineBidiStatus, cleanLineBidiContext, endLineYPos);
        if (endLine && cleanLineBidiContext)
            cleanLineBidiContext->ref();
        if (startLine) {
            useRepaintRect = true;
            startLineBottom = startLine->bottomOverflow();
            repaintRect.setY(min(m_height, startLine->topOverflow()));
            RenderArena* arena = renderArena();
            RootInlineBox* box = startLine;
            while (box) {
                RootInlineBox* next = box->nextRootBox();
                box->deleteLine(arena);
                box = next;
            }
            startLine = 0;
        }
        
        BidiIterator end = start;

        bool endLineMatched = false;
        while (!end.atEnd()) {
            start = end;
            if (endLine && (endLineMatched = matchedEndLine(start, bidi.status, bidi.context.get(), cleanLineStart, cleanLineBidiStatus, cleanLineBidiContext, endLine, endLineYPos)))
                break;

            betweenMidpoints = false;
            isLineEmpty = true;
            if (m_firstLine && firstChild() && firstChild()->isCompact() && firstChild()->isRenderBlock()) {
                buildCompactRuns(firstChild(), bidi);
                start.obj = firstChild()->nextSibling();
                end = start;
            }
            end = findNextLineBreak(start, bidi);
            if (start.atEnd()) {
                deleteBidiRuns(renderArena());
                break;
            }
            if (!isLineEmpty) {
                bidiReorderLine(start, end, bidi);

                // Now that the runs have been ordered, we create the line boxes.
                // At the same time we figure out where border/padding/margin should be applied for
                // inline flow boxes.
                if (sCompactFirstBidiRun) {
                    // We have a compact line sharing this line.  Link the compact runs
                    // to our runs to create a single line of runs.
                    sCompactLastBidiRun->nextRun = sFirstBidiRun;
                    sFirstBidiRun = sCompactFirstBidiRun;
                    sBidiRunCount += sCompactBidiRunCount;
                }

                RootInlineBox* lineBox = 0;
                if (sBidiRunCount) {
                    lineBox = constructLine(start, end);
                    if (lineBox) {
                        lineBox->setEndsWithBreak(previousLineBrokeCleanly);
                        
                        // Now we position all of our text runs horizontally.
                        computeHorizontalPositionsForLine(lineBox, bidi);
        
                        // Now position our text runs vertically.
                        computeVerticalPositionsForLine(lineBox);
                    }
                }

                deleteBidiRuns(renderArena());
                
                if (end == start) {
                    bidi.adjustEmbedding = true;
                    end.increment(bidi);
                    bidi.adjustEmbedding = false;
                }

                if (lineBox)
                    lineBox->setLineBreakInfo(end.obj, end.pos, &bidi.status, bidi.context.get());
                
                m_firstLine = false;
                newLine();
            }
             
            sNumMidpoints = 0;
            sCurrMidpoint = 0;
            sCompactFirstBidiRun = sCompactLastBidiRun = 0;
            sCompactBidiRunCount = 0;
        }
        
        if (endLine) {
            if (endLineMatched) {
                // Note our current y-position for correct repainting when no lines move.  If no lines move, we still have to
                // repaint up to the maximum of the bottom overflow of the old start line or the bottom overflow of the new last line.
                int currYPos = max(startLineBottom, m_height);
                if (lastRootBox())
                    currYPos = max(currYPos, lastRootBox()->bottomOverflow());
                
                // Attach all the remaining lines, and then adjust their y-positions as needed.
                for (RootInlineBox* line = endLine; line; line = line->nextRootBox())
                    line->attachLine();
                
                // Now apply the offset to each line if needed.
                int delta = m_height - endLineYPos;
                if (delta) {
                    for (RootInlineBox* line = endLine; line; line = line->nextRootBox())
                        line->adjustPosition(0, delta);
                }
                m_height = lastRootBox()->blockHeight();
                m_overflowHeight = max(m_height, m_overflowHeight);
                int bottomOfLine = lastRootBox()->bottomOverflow();
                if (bottomOfLine > m_height && bottomOfLine > m_overflowHeight)
                    m_overflowHeight = bottomOfLine;
                if (delta)
                    repaintRect.setHeight(max(m_overflowHeight-delta, m_overflowHeight) - repaintRect.y());
                else
                    repaintRect.setHeight(currYPos - repaintRect.y());
            }
            else {
                // Delete all the remaining lines.
                m_overflowHeight = max(m_height, m_overflowHeight);
                InlineRunBox* line = endLine;
                RenderArena* arena = renderArena();
                while (line) {
                    InlineRunBox* next = line->nextLineBox();
                    if (!next)
                        repaintRect.setHeight(max(m_overflowHeight, line->bottomOverflow()) - repaintRect.y());
                    line->deleteLine(arena);
                    line = next;
                }
            }
            if (cleanLineBidiContext)
                cleanLineBidiContext->deref();
        }
    }

    sNumMidpoints = 0;
    sCurrMidpoint = 0;

    // in case we have a float on the last line, it might not be positioned up to now.
    // This has to be done before adding in the bottom border/padding, or the float will
    // include the padding incorrectly. -dwh
    positionNewFloats();
    
    // Now add in the bottom border/padding.
    m_height += toAdd;

    // Always make sure this is at least our height.
    m_overflowHeight = max(m_height, m_overflowHeight);
    
    // See if any lines spill out of the block.  If so, we need to update our overflow width.
    checkLinesForOverflow();

    if (useRepaintRect) {
        repaintRect.setX(m_overflowLeft);
        repaintRect.setWidth(max((int)m_width, m_overflowWidth) - m_overflowLeft);
        if (repaintRect.height() == 0)
            repaintRect.setHeight(max(oldLineBottom, m_overflowHeight) - repaintRect.y());
        if (hasOverflowClip())
            // Don't allow this rect to spill out of our overflow box.
            repaintRect.intersect(IntRect(0, 0, m_width, m_height));
    }

    if (!firstLineBox() && hasLineIfEmpty())
        m_height += lineHeight(true);

    // See if we have any lines that spill out of our block.  If we do, then we will possibly need to
    // truncate text.
    if (hasTextOverflow)
        checkLinesForTextOverflow();

    return repaintRect;

#if BIDI_DEBUG > 1
    kdDebug(6041) << " ------- bidi end " << this << " -------" << endl;
#endif
}

RootInlineBox* RenderBlock::determineStartPosition(bool fullLayout, BidiIterator& start, BidiState& bidi)
{
    RootInlineBox* curr = 0;
    RootInlineBox* last = 0;
    RenderObject* startObj = 0;
    int pos = 0;
    
    if (fullLayout) {
        deleteLineBoxes();        
        ASSERT(!m_firstLineBox && !m_lastLineBox);
    } else {
        for (curr = firstRootBox(); curr && !curr->isDirty(); curr = curr->nextRootBox());
        if (curr) {
            // We have a dirty line.
            if (RootInlineBox* prevRootBox = curr->prevRootBox()) {
                // We have a previous line.
                if (!prevRootBox->endsWithBreak() || prevRootBox->lineBreakObj()->isText() && prevRootBox->lineBreakPos() >= static_cast<RenderText*>(prevRootBox->lineBreakObj())->stringLength())
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
    
    m_firstLine = !last;
    previousLineBrokeCleanly = !last || last->endsWithBreak();
    if (last) {
        m_height = last->blockHeight();
        int bottomOfLine = last->bottomOverflow();
        if (bottomOfLine > m_height && bottomOfLine > m_overflowHeight)
            m_overflowHeight = bottomOfLine;
        startObj = last->lineBreakObj();
        pos = last->lineBreakPos();
        bidi.status = last->lineBreakBidiStatus();
        bidi.context = last->lineBreakBidiContext();
    } else {
        bidi.adjustEmbedding = true;
        startObj = bidiFirst(this, bidi, 0);
        bidi.adjustEmbedding = false;
    }
        
    start = BidiIterator(this, startObj, pos);
    
    return curr;
}

RootInlineBox* RenderBlock::determineEndPosition(RootInlineBox* startLine, BidiIterator& cleanLineStart,
                                                 BidiStatus& cleanLineBidiStatus, BidiContext*& cleanLineBidiContext,
                                                 int& yPos)
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
    cleanLineStart = BidiIterator(this, prev->lineBreakObj(), prev->lineBreakPos());
    cleanLineBidiStatus = prev->lineBreakBidiStatus();
    cleanLineBidiContext = prev->lineBreakBidiContext();
    yPos = last->prevRootBox()->blockHeight();
    
    for (RootInlineBox* line = last; line; line = line->nextRootBox())
        line->extractLine(); // Disconnect all line boxes from their render objects while preserving
                             // their connections to one another.
    
    return last;
}

bool RenderBlock::matchedEndLine(const BidiIterator& start, const BidiStatus& status, BidiContext* context,
                                 const BidiIterator& endLineStart, const BidiStatus& endLineStatus,
                                 BidiContext* endLineContext, RootInlineBox*& endLine, int& endYPos)
{
    if (start == endLineStart)
        return status == endLineStatus && *context == *endLineContext;
    else {
        // The first clean line doesn't match, but we can check a handful of following lines to try
        // to match back up.
        static int numLines = 8; // The # of lines we're willing to match against.
        RootInlineBox* line = endLine;
        for (int i = 0; i < numLines && line; i++, line = line->nextRootBox()) {
            if (line->lineBreakObj() == start.obj && line->lineBreakPos() == start.pos) {
                // We have a match.
                if (line->lineBreakBidiStatus() != status || *line->lineBreakBidiContext() != *context)
                    return false; // ...but the bidi state doesn't match.
                RootInlineBox* result = line->nextRootBox();
                                
                // Set our yPos to be the block height of endLine.
                if (result)
                    endYPos = line->blockHeight();
                
                // Now delete the lines that we failed to sync.
                RootInlineBox* boxToDelete = endLine;
                RenderArena* arena = renderArena();
                while (boxToDelete && boxToDelete != result) {
                    RootInlineBox* next = boxToDelete->nextRootBox();
                    boxToDelete->deleteLine(arena);
                    boxToDelete = next;
                }

                endLine = result;
                return result;
            }
        }
    }
    return false;
}

static const unsigned short nonBreakingSpace = 0xa0;

static inline bool skipNonBreakingSpace(BidiIterator &it)
{
    if (it.obj->style()->nbspMode() != SPACE || it.current() != nonBreakingSpace)
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

    return object->style()->preserveNewline();
}

int RenderBlock::skipWhitespace(BidiIterator &it, BidiState &bidi)
{
    // FIXME: The entire concept of the skipWhitespace function is flawed, since we really need to be building
    // line boxes even for containers that may ultimately collapse away.  Otherwise we'll never get positioned
    // elements quite right.  In other words, we need to build this function's work into the normal line
    // object iteration process.
    int w = lineWidth(m_height);
    while (!it.atEnd() && (it.obj->isFloatingOrPositioned() || it.obj->isInlineFlow() || 
           (shouldCollapseWhiteSpace(it.obj->style()) && !it.obj->isBR() &&
            (it.current() == ' ' || it.current() == '\t' || (!shouldPreserveNewline(it.obj) && it.current() == '\n') ||
             it.current() == softHyphen || skipNonBreakingSpace(it))))) {
        if (it.obj->isFloatingOrPositioned()) {
            RenderObject *o = it.obj;
            // add to special objects...
            if (o->isFloating()) {
                insertFloatingObject(o);
                positionNewFloats();
                w = lineWidth(m_height);
            }
            else if (o->isPositioned()) {
                // FIXME: The math here is actually not really right.  It's a best-guess approximation that
                // will work for the common cases
                RenderObject* c = o->container();
                if (c->isInlineFlow()) {
                    // A relative positioned inline encloses us.  In this case, we also have to determine our
                    // position as though we were an inline.  Set |staticX| and |staticY| on the relative positioned
                    // inline so that we can obtain the value later.
                    c->setStaticX(style()->direction() == LTR ?
                                  leftOffset(m_height) : rightOffset(m_height));
                    c->setStaticY(m_height);
                }
                
                if (o->hasStaticX()) {
                    bool wasInline = o->style()->isOriginalDisplayInlineType();
                    if (wasInline)
                        o->setStaticX(style()->direction() == LTR ?
                                      leftOffset(m_height) :
                                      width() - rightOffset(m_height));
                    else
                        o->setStaticX(style()->direction() == LTR ?
                                      borderLeft() + paddingLeft() :
                                      borderRight() + paddingRight());
                }
                if (o->hasStaticY())
                    o->setStaticY(m_height);
            }
        }
        
        bidi.adjustEmbedding = true;
        it.increment(bidi);
        bidi.adjustEmbedding = false;
    }
    return w;
}

BidiIterator RenderBlock::findNextLineBreak(BidiIterator &start, BidiState &bidi)
{
    // eliminate spaces at beginning of line
    int width = skipWhitespace(start, bidi);
    int w = 0;
    int tmpW = 0;

    if (start.atEnd())
        return start;

    // This variable is used only if whitespace isn't set to PRE, and it tells us whether
    // or not we are currently ignoring whitespace.
    bool ignoringSpaces = false;
    BidiIterator ignoreStart;
    
    // This variable tracks whether the very last character we saw was a space.  We use
    // this to detect when we encounter a second space so we know we have to terminate
    // a run.
    bool currentCharacterIsSpace = false;
    bool currentCharacterIsWS = false;
    RenderObject* trailingSpaceObject = 0;

    BidiIterator lBreak = start;

    RenderObject *o = start.obj;
    RenderObject *last = o;
    RenderObject *previous = o;
    int pos = start.pos;
    bool atStart = true;

    bool prevLineBrokeCleanly = previousLineBrokeCleanly;
    previousLineBrokeCleanly = false;
    
    EWhiteSpace currWS = style()->whiteSpace();
    EWhiteSpace lastWS = currWS;
    while (o) {
        currWS = o->isReplaced() ? o->parent()->style()->whiteSpace() : o->style()->whiteSpace();
        lastWS = last->isReplaced() ? last->parent()->style()->whiteSpace() : last->style()->whiteSpace();
        
        bool autoWrap = RenderStyle::autoWrap(currWS);
        bool preserveNewline = RenderStyle::preserveNewline(currWS);
        bool collapseWhiteSpace = RenderStyle::collapseWhiteSpace(currWS);
        
        if (o->isBR()) {
            if (w + tmpW <= width) {
                lBreak.obj = o;
                lBreak.pos = 0;
                lBreak.increment(bidi);

                // A <br> always breaks a line, so don't let the line be collapsed
                // away. Also, the space at the end of a line with a <br> does not
                // get collapsed away.  It only does this if the previous line broke
                // cleanly.  Otherwise the <br> has no effect on whether the line is
                // empty or not.
                if (prevLineBrokeCleanly)
                    isLineEmpty = false;
                trailingSpaceObject = 0;
                previousLineBrokeCleanly = true;

                if (!isLineEmpty) {
                    // only check the clear status for non-empty lines.
                    EClear clear = o->style()->clear();
                    if (clear != CNONE)
                        m_clearStatus = (EClear) (m_clearStatus | clear);
                }
            }
            goto end;
        }
        
        if (o->isFloatingOrPositioned()) {
            // add to special objects...
            if (o->isFloating()) {
                insertFloatingObject(o);
                // check if it fits in the current line.
                // If it does, position it now, otherwise, position
                // it after moving to next line (in newLine() func)
                if (o->width() + o->marginLeft() + o->marginRight() + w + tmpW <= width) {
                    positionNewFloats();
                    width = lineWidth(m_height);
                }
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
                    o->setStaticY(m_height);
                    needToSetStaticY = false;
                }
                
                bool needToCreateLineBox = needToSetStaticX || needToSetStaticY;
                RenderObject* c = o->container();
                if (c->isInlineFlow() && (!needToSetStaticX || !needToSetStaticY))
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
        } else if (o->isInlineFlow()) {
            // Only empty inlines matter.  We treat those similarly to replaced elements.
            assert(!o->firstChild());
            tmpW += o->marginLeft() + o->borderLeft() + o->paddingLeft()+
                    o->marginRight() + o->borderRight() + o->paddingRight();
        } else if (o->isReplaced()) {
            // Break on replaced elements if either has normal white-space.
            if (autoWrap || RenderStyle::autoWrap(lastWS)) {
                w += tmpW;
                tmpW = 0;
                lBreak.obj = o;
                lBreak.pos = 0;
            }
            
            if (ignoringSpaces) {
                BidiIterator startMid( 0, o, 0 );
                addMidpoint(startMid);
            }
            isLineEmpty = false;
            ignoringSpaces = false;
            currentCharacterIsSpace = false;
            currentCharacterIsWS = false;
            trailingSpaceObject = 0;
            
            if (o->isListMarker() && o->style()->listStylePosition() == OUTSIDE) {
                // The marker must not have an effect on whitespace at the start
                // of the line.  We start ignoring spaces to make sure that any additional
                // spaces we see will be discarded. 
                //
                // Optimize for a common case. If we can't find whitespace after the list
                // item, then this is all moot. -dwh
                RenderObject* next = bidiNext(start.block, o, bidi);
                if (style()->collapseWhiteSpace() && next && !next->isBR() && next->isText() && static_cast<RenderText*>(next)->stringLength() > 0) {
                    RenderText *nextText = static_cast<RenderText*>(next);
                    UChar nextChar = nextText->text()[0];
                    if (nextText->style()->isCollapsibleWhiteSpace(nextChar)) {
                        currentCharacterIsSpace = true;
                        currentCharacterIsWS = true;
                        ignoringSpaces = true;
                        BidiIterator endMid( 0, o, 0 );
                        addMidpoint(endMid);
                    }
                }
            } else
                tmpW += o->width() + o->marginLeft() + o->marginRight() + inlineWidth(o);
        } else if (o->isText()) {
            RenderText *t = static_cast<RenderText *>(o);
            int strlen = t->stringLength();
            int len = strlen - pos;
            const UChar* str = t->text();

            const Font *f = t->font(m_firstLine);
            // proportional font, needs a bit more work.
            int lastSpace = pos;
            int wordSpacing = o->style()->wordSpacing();
            int lastSpaceWordSpacing = 0;

            bool appliedStartWidth = pos > 0; // If the span originated on a previous line,
                                              // then assume the start width has been applied.
            int wrapW = tmpW + inlineWidth(o, !appliedStartWidth, true);
            int charWidth = 0;
            int nextBreakable = -1;

            while (len) {
                bool previousCharacterIsSpace = currentCharacterIsSpace;
                bool previousCharacterIsWS = currentCharacterIsWS;
                UChar c = str[pos];
                currentCharacterIsSpace = c == ' ' || c == '\t' || (!preserveNewline && (c == '\n'));

                if (!collapseWhiteSpace || !currentCharacterIsSpace)
                    isLineEmpty = false;
                
                // Check for soft hyphens.  Go ahead and ignore them.
                if (c == SOFT_HYPHEN) {
                    if (!ignoringSpaces) {
                        // Ignore soft hyphens
                        BidiIterator endMid;
                        if (pos > 0)
                            endMid = BidiIterator(0, o, pos-1);
                        else
                            endMid = BidiIterator(0, previous, previous->isText() ? static_cast<RenderText *>(previous)->stringLength() - 1 : 0);
                        // Two consecutive soft hyphens. Avoid overlapping midpoints.
                        if (sNumMidpoints && smidpoints->at(sNumMidpoints - 1).obj == endMid.obj && smidpoints->at(sNumMidpoints - 1).pos > endMid.pos)
                            sNumMidpoints--;
                        else
                            addMidpoint(endMid);
                        
                        // Add the width up to but not including the hyphen.
                        tmpW += t->width(lastSpace, pos - lastSpace, f, w+tmpW) + lastSpaceWordSpacing;
                        
                        // For wrapping text only, include the hyphen.  We need to ensure it will fit
                        // on the line if it shows when we break.
                        if (autoWrap)
                            tmpW += t->width(pos, 1, f, w+tmpW);
                        
                        BidiIterator startMid(0, o, pos+1);
                        addMidpoint(startMid);
                    }
                    
                    pos++;
                    len--;
                    lastSpaceWordSpacing = 0;
                    lastSpace = pos; // Cheesy hack to prevent adding in widths of the run twice.
                    continue;
                }
                
                bool applyWordSpacing = false;
                bool breakNBSP = autoWrap && o->style()->nbspMode() == SPACE;
                
                // Auto-wrapping text should wrap in the middle of a word only if it could not wrap before the word,
                // which is only possible if the word is the first thing on the line, that is, if |w| is zero.
                bool breakWords = o->style()->breakWords() && ((autoWrap && !w) || currWS == PRE);
                bool midWordBreak = false;
                bool breakAll = o->style()->wordBreak() == BreakAllWordBreak && autoWrap;

                currentCharacterIsWS = currentCharacterIsSpace || (breakNBSP && c == nonBreakingSpace);

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
                            BidiIterator startMid ( 0, o, pos );
                            addMidpoint(startMid);
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

                    if (autoWrap && w + tmpW > width && w == 0) {
                        int fb = nearestFloatBottom(m_height);
                        int newLineWidth = lineWidth(fb);
                        // See if |tmpW| will fit on the new line.  As long as it does not,
                        // keep adjusting our float bottom until we find some room.
                        int lastFloatBottom = m_height;
                        while (lastFloatBottom < fb && tmpW > newLineWidth) {
                            lastFloatBottom = fb;
                            fb = nearestFloatBottom(fb);
                            newLineWidth = lineWidth(fb);
                        }
                        
                        if (!w && m_height < fb && width < newLineWidth) {
                            m_height = fb;
                            width = newLineWidth;
                        }
                    }
        
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
                                if (pos > 0) {
                                    // Separate the trailing space into its own box, which we will
                                    // resize to fit on the line in computeHorizontalPositionsForLine().
                                    BidiIterator midpoint(0, o, pos);
                                    addMidpoint(BidiIterator(0, o, pos-1)); // Stop
                                    addMidpoint(BidiIterator(0, o, pos)); // Start
                                }
                                skipWhitespace(lBreak, bidi);
                            }
                        }
                        if (lineWasTooWide || w + tmpW > width) {
                            if (lBreak.obj && lBreak.obj->style()->preserveNewline() && lBreak.obj->isText() && static_cast<RenderText*>(lBreak.obj)->text()[lBreak.pos] == '\n') {
                                if (!stoppedIgnoringSpaces && pos > 0) {
                                    // We need to stop right before the newline and then start up again.
                                    BidiIterator midpoint(0, o, pos);
                                    addMidpoint(BidiIterator(0, o, pos-1)); // Stop
                                    addMidpoint(BidiIterator(0, o, pos)); // Start
                                }
                                lBreak.increment(bidi);
                                previousLineBrokeCleanly = true;
                            }
                            goto end; // Didn't fit. Jump to the end.
                        } else {
                            if (!betweenWords || (midWordBreak && !autoWrap))
                                tmpW -= additionalTmpW;
                            if (pos > 0 && str[pos-1] == SOFT_HYPHEN)
                                // Subtract the width of the soft hyphen out since we fit on a line.
                                tmpW -= t->width(pos-1, 1, f, w+tmpW);
                        }
                    }

                    if (c == '\n' && preserveNewline) {
                        if (!stoppedIgnoringSpaces && pos > 0) {
                            // We need to stop right before the newline and then start up again.
                            BidiIterator midpoint(0, o, pos);
                            addMidpoint(BidiIterator(0, o, pos-1)); // Stop
                            addMidpoint(BidiIterator(0, o, pos)); // Start
                        }
                        lBreak.obj = o;
                        lBreak.pos = pos;
                        lBreak.increment(bidi);
                        previousLineBrokeCleanly = true;
                        return lBreak;
                    }

                    if (autoWrap && betweenWords) {
                        w += tmpW;
                        wrapW = 0;
                        tmpW = 0;
                        lBreak.obj = o;
                        lBreak.pos = pos;
                    }
                    
                    if (midWordBreak) {
                        // Remember this as a breakable position in case
                        // adding the end width forces a break.
                        lBreak.obj = o;
                        lBreak.pos = pos;
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
                    BidiIterator startMid(0, o, pos);
                    addMidpoint(startMid);
                }

                if (currentCharacterIsSpace && !previousCharacterIsSpace) {
                    ignoreStart.obj = o;
                    ignoreStart.pos = pos;
                }

                if (!currentCharacterIsWS && previousCharacterIsWS) {
                    if (autoWrap && o->style()->breakOnlyAfterWhiteSpace()) {
                        lBreak.obj = o;
                        lBreak.pos = pos;
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
            if (!appliedStartWidth)
                tmpW += inlineWidth(o, true, false);
            tmpW += inlineWidth(o, false, true);
        } else
            ASSERT( false );

        RenderObject* next = bidiNext(start.block, o, bidi);
        bool checkForBreak = autoWrap;
        if (w && w + tmpW > width && lBreak.obj && currWS == NOWRAP)
            checkForBreak = true;
        else if (next && o->isText() && next->isText() && !next->isBR()) {
            if (autoWrap || (next->style()->autoWrap())) {
                if (currentCharacterIsSpace)
                    checkForBreak = true;
                else {
                    checkForBreak = false;
                    RenderText* nextText = static_cast<RenderText*>(next);
                    if (nextText->stringLength() != 0) {
                        UChar c = nextText->text()[0];
                        if (c == ' ' || c == '\t' || (c == '\n' && !next->style()->preserveNewline()))
                            // If the next item on the line is text, and if we did not end with
                            // a space, then the next text run continues our word (and so it needs to
                            // keep adding to |tmpW|.  Just update and continue.
                            checkForBreak = true;
                    }
                    bool canPlaceOnLine = (w + tmpW <= width) || !autoWrap;
                    if (canPlaceOnLine && checkForBreak) {
                        w += tmpW;
                        tmpW = 0;
                        lBreak.obj = next;
                        lBreak.pos = 0;
                    }
                }
            }
        }

        if (checkForBreak && (w + tmpW > width)) {
            // if we have floats, try to get below them.
            if (currentCharacterIsSpace && !ignoringSpaces && o->style()->collapseWhiteSpace())
                trailingSpaceObject = 0;
            
            int fb = nearestFloatBottom(m_height);
            int newLineWidth = lineWidth(fb);
            // See if |tmpW| will fit on the new line.  As long as it does not,
            // keep adjusting our float bottom until we find some room.
            int lastFloatBottom = m_height;
            while (lastFloatBottom < fb && tmpW > newLineWidth) {
                lastFloatBottom = fb;
                fb = nearestFloatBottom(fb);
                newLineWidth = lineWidth(fb);
            }            
            if (!w && m_height < fb && width < newLineWidth) {
                m_height = fb;
                width = newLineWidth;
            }

            // |width| may have been adjusted because we got shoved down past a float (thus
            // giving us more room), so we need to retest, and only jump to
            // the end label if we still don't fit on the line. -dwh
            if (w + tmpW > width)
                goto end;
        }

        last = o;
        if (!o->isFloating() && (!o->isPositioned() || o->hasStaticX() || o->hasStaticY() || !o->container()->isInlineFlow()))
            previous = o;
        o = next;

        if (!last->isFloatingOrPositioned() && last->isReplaced() && autoWrap && 
            (!last->isListMarker() || last->style()->listStylePosition()==INSIDE)) {
            // Go ahead and add in tmpW.
            w += tmpW;
            tmpW = 0;
            lBreak.obj = o;
            lBreak.pos = 0;
        }

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
    }

 end:

    if (lBreak == start && !lBreak.obj->isBR()) {
        // we just add as much as possible
        if (style()->whiteSpace() == PRE) {
            // FIXME: Don't really understand this case.
            if (pos != 0) {
                lBreak.obj = o;
                lBreak.pos = pos - 1;
            } else {
                lBreak.obj = last;
                lBreak.pos = last->isText() ? last->length() : 0;
            }
        } else if (lBreak.obj) {
            if (last != o && !last->isListMarker()) {
                // better to break between object boundaries than in the middle of a word (except for list markers)
                lBreak.obj = o;
                lBreak.pos = 0;
            } else {
                // Don't ever break in the middle of a word if we can help it.
                // There's no room at all. We just have to be on this line,
                // even though we'll spill out.
                lBreak.obj = o;
                lBreak.pos = pos;
            }
        }
    }

    // make sure we consume at least one char/object.
    if (lBreak == start)
        lBreak.increment(bidi);

    // Sanity check our midpoints.
    checkMidpoints(lBreak, bidi);
        
    if (trailingSpaceObject) {
        // This object is either going to be part of the last midpoint, or it is going
        // to be the actual endpoint.  In both cases we just decrease our pos by 1 level to
        // exclude the space, allowing it to - in effect - collapse into the newline.
        if (sNumMidpoints%2==1) {
            BidiIterator* midpoints = smidpoints->data();
            midpoints[sNumMidpoints-1].pos--;
        }
        //else if (lBreak.pos > 0)
        //    lBreak.pos--;
        else if (lBreak.obj == 0 && trailingSpaceObject->isText()) {
            // Add a new end midpoint that stops right at the very end.
            RenderText* text = static_cast<RenderText *>(trailingSpaceObject);
            unsigned pos = text->length() >=2 ? text->length() - 2 : UINT_MAX;
            BidiIterator endMid(0, trailingSpaceObject, pos);
            addMidpoint(endMid);
        }
    }

    // We might have made lBreak an iterator that points past the end
    // of the object. Do this adjustment to make it point to the start
    // of the next object instead to avoid confusing the rest of the
    // code.
    if (lBreak.pos > 0) {
        lBreak.pos--;
        lBreak.increment(bidi);
    }

    if (lBreak.obj && lBreak.pos >= 2 && lBreak.obj->isText()) {
        // For soft hyphens on line breaks, we have to chop out the midpoints that made us
        // ignore the hyphen so that it will render at the end of the line.
        UChar c = static_cast<RenderText*>(lBreak.obj)->text()[lBreak.pos-1];
        if (c == SOFT_HYPHEN)
            chopMidpointsAt(lBreak.obj, lBreak.pos-2);
    }
    
    return lBreak;
}

void RenderBlock::checkLinesForOverflow()
{
    // FIXME: Inline blocks can have overflow.  Need to understand when those objects are present on a line
    // and factor that in somehow.
    m_overflowWidth = m_width;
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
    const UChar ellipsis = 0x2026; // FIXME: CSS3 says this is configurable, also need to use 0x002E (FULL STOP) if 0x2026 not renderable
    TextRun ellipsisRun(&ellipsis, 1);
    static AtomicString ellipsisStr(&ellipsis, 1);
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
