/*
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
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
 *
 */

#ifndef RenderObject_h
#define RenderObject_h

#include "CachedResourceClient.h"
#include "RenderStyle.h"
#include "ScrollBar.h"
#include "VisiblePosition.h"
#include "Element.h"
#include "Document.h"

/*
 *  The painting of a layer occurs in three distinct phases.  Each phase involves
 *  a recursive descent into the layer's render objects. The first phase is the background phase.
 *  The backgrounds and borders of all blocks are painted.  Inlines are not painted at all.
 *  Floats must paint above block backgrounds but entirely below inline content that can overlap them.
 *  In the foreground phase, all inlines are fully painted.  Inline replaced elements will get all
 *  three phases invoked on them during this phase.
 */

namespace WebCore {

class AffineTransform;
class CollapsedBorderValue;
class Color;
class Document;
class Element;
class Event;
class FloatRect;
class FrameView;
class HTMLAreaElement;
class InlineBox;
class InlineFlowBox;
class Position;
class RenderArena;
class RenderBlock;
class RenderFlow;
class RenderFrameSet;
class RenderLayer;
class RenderTable;
class RenderText;
class RenderView;
class String;
class TextStream;
class VisiblePosition;

enum PaintPhase {
    PaintPhaseBlockBackground,
    PaintPhaseChildBlockBackground,
    PaintPhaseChildBlockBackgrounds,
    PaintPhaseFloat,
    PaintPhaseForeground,
    PaintPhaseOutline,
    PaintPhaseChildOutlines,
    PaintPhaseSelfOutline,
    PaintPhaseSelection,
    PaintPhaseCollapsedTableBorders
};

enum PaintRestriction {
    PaintRestrictionNone,
    PaintRestrictionSelectionOnly,
    PaintRestrictionSelectionOnlyWhiteText
};

enum HitTestFilter {
    HitTestAll,
    HitTestSelf,
    HitTestDescendants
};

enum HitTestAction {
    HitTestBlockBackground,
    HitTestChildBlockBackground,
    HitTestChildBlockBackgrounds,
    HitTestFloat,
    HitTestForeground
};

struct DashboardRegionValue
{
    String label;
    IntRect bounds;
    IntRect clip;
    int type;

    bool operator==(const DashboardRegionValue& o) const
    {
        return type == o.type && bounds == o.bounds && label == o.label;
    }
};

// FIXME: This should be a HashSequencedSet, but we don't have that data structure yet.
// This means the paint order of outlines will be wrong, although this is a minor issue.
typedef HashSet<RenderFlow*> RenderFlowSequencedSet;

/**
 * Base Class for all rendering tree objects.
 */
class RenderObject : public CachedResourceClient
{
public:
    // Anonymous objects should pass the document as their node, and they will then automatically be
    // marked as anonymous in the constructor.
    RenderObject(Node* node);
    virtual ~RenderObject();

    RenderObject *parent() const { return m_parent; }
    bool hasAncestor(const RenderObject *obj) const;

    RenderObject *previousSibling() const { return m_previous; }
    RenderObject *nextSibling() const { return m_next; }

    virtual RenderObject *firstChild() const { return 0; }
    virtual RenderObject *lastChild() const { return 0; }

    RenderObject* nextInPreOrder() const;
    RenderObject* nextInPreOrderAfterChildren() const;
    RenderObject* previousInPreOrder() const;
    RenderObject* childAt(unsigned) const;

    RenderObject *nextEditable() const; 
    RenderObject *previousEditable() const; 

    RenderObject *firstLeafChild() const;
    RenderObject *lastLeafChild() const;
    
    RenderObject *traverseNext(const RenderObject *stayWithin) const;     
    typedef bool (*TraverseNextInclusionFunction)(const RenderObject *); 
    RenderObject *traverseNext(const RenderObject *stayWithin, TraverseNextInclusionFunction inclusionFunction) const;
    void adjustComputedFontSizesOnBlocks(float size, float visibleWidth);
    virtual RenderLayer* layer() const { return 0; }
    RenderLayer* enclosingLayer();
    void addLayers(RenderLayer* parentLayer, RenderObject* newObject);
    void removeLayers(RenderLayer* parentLayer);
    void moveLayers(RenderLayer* oldParent, RenderLayer* newParent);
    RenderLayer* findNextLayer(RenderLayer* parentLayer, RenderObject* startPoint,
                               bool checkParent=true);
    virtual void positionChildLayers() { }
    virtual bool requiresLayer();
    
    virtual IntRect getOverflowClipRect(int tx, int ty) { return IntRect(0,0,0,0); }
    virtual IntRect getClipRect(int tx, int ty) { return IntRect(0,0,0,0); }
    bool hasClip() { return isPositioned() &&  style()->hasClip(); }
    
    virtual int getBaselineOfFirstLineBox() const { return -1; } 
    virtual int getBaselineOfLastLineBox() const { return -1; }
    virtual bool isEmpty() const { return firstChild() == 0; }
        
    virtual bool isEdited() const { return false; }
    virtual void setEdited(bool) { return; };
    
    // Obtains the nearest enclosing block (including this block) that contributes a first-line style to our inline
    // children.
    virtual RenderBlock* firstLineBlock() const;
    virtual void updateFirstLetter();
    
    // Called when an object that was floating or positioned becomes a normal flow object
    // again.  We have to make sure the render tree updates as needed to accommodate the new
    // normal flow object.
    void handleDynamicFloatPositionChange();

    // This function is a convenience helper for creating an anonymous block that inherits its
    // style from this RenderObject.
    RenderBlock* createAnonymousBlock();
    
    // Whether or not a positioned element requires normal flow x/y to be computed
    // to determine its position.
    bool hasStaticX() const;
    bool hasStaticY() const;
    virtual void setStaticX(int staticX) {};
    virtual void setStaticY(int staticY) {};
    virtual int staticX() const { return 0; }
    virtual int staticY() const { return 0; }
    
    // RenderObject tree manipulation
    //////////////////////////////////////////
    virtual bool canHaveChildren() const;
    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const { return true; }
    virtual void addChild(RenderObject *newChild, RenderObject *beforeChild = 0);
    virtual void removeChild(RenderObject *oldChild);
    virtual bool createsAnonymousWrapper() const { return false; }

    // raw tree manipulation
    virtual RenderObject* removeChildNode(RenderObject* child);
    virtual void appendChildNode(RenderObject* child);
    virtual void insertChildNode(RenderObject* child, RenderObject* before);
    //////////////////////////////////////////

protected:
    //////////////////////////////////////////
    // Helper functions. Dangerous to use!
    void setPreviousSibling(RenderObject *previous) { m_previous = previous; }
    void setNextSibling(RenderObject *next) { m_next = next; }
    void setParent(RenderObject *parent) { m_parent = parent; }
    //////////////////////////////////////////
private:
    void addAbsoluteRectForLayer(IntRect& result);

public:
    virtual const char *renderName() const { return "RenderObject"; }
#ifndef NDEBUG
    DeprecatedString information() const;
    virtual void dump(TextStream *stream, DeprecatedString ind = "") const;
    void showTreeForThis() const;
#endif

    static RenderObject *createObject(Node* node, RenderStyle* style);

    // Overloaded new operator.  Derived classes must override operator new
    // in order to allocate out of the RenderArena.
    void* operator new(size_t sz, RenderArena* renderArena) throw();    

    // Overridden to prevent the normal delete from being called.
    void operator delete(void* ptr, size_t sz);
        
private:
    // The normal operator new is disallowed on all render objects.
    void* operator new(size_t sz) throw();
    
public:
    RenderArena* renderArena() const;
    
    // some helper functions...
    virtual bool isRenderBlock() const { return false; }
    virtual bool isRenderInline() const { return false; }
    virtual bool isInlineFlow() const { return false; }
    virtual bool isBlockFlow() const { return false; }
    virtual bool isInlineBlockOrInlineTable() const { return false; }
    virtual bool childrenInline() const { return false; }
    virtual void setChildrenInline(bool b) { };

    virtual RenderFlow* continuation() const;
    virtual bool isInlineContinuation() const;
    
    virtual bool isListItem() const { return false; }
    virtual bool isListMarker() const { return false; }
    virtual bool isRenderView() const { return false; }
    inline bool isRoot() const
    {
        Node * node = element();
        
        return  node && node->renderer() == this && 
                node->document()->documentElement() == node; 
    }
    bool isBody() const;
    bool isHR() const;
    virtual bool isBR() const { return false; }
    virtual bool isTableCell() const { return false; }
    virtual bool isTableRow() const { return false; }
    virtual bool isTableSection() const { return false; }
    virtual bool isTableCol() const { return false; }
    virtual bool isTable() const { return false; }
    virtual bool isWidget() const { return false; }
    virtual bool isFormElement() const { return false; }
    virtual bool isImage() const { return false; }
    virtual bool isTextArea() const { return false; }
    virtual bool isTextField() const { return false; }
    virtual bool isFrame() const { return false; }
    virtual bool isFrameSet() const { return false; }
    virtual bool isApplet() const { return false; }
    
#if SVG_SUPPORT
    virtual bool isKCanvasContainer() const { return false; }
    virtual bool isRenderPath() const { return false; }
    virtual FloatRect relativeBBox(bool includeStroke = true) const;
    // We may eventually want to make these non-virtual
    virtual AffineTransform localTransform() const;
    virtual void setLocalTransform(const AffineTransform&);
    virtual AffineTransform absoluteTransform() const;
#endif
    
    virtual bool isEditable() const;

    bool isHTMLMarquee() const;
    
    bool isAnonymous() const { return m_isAnonymous; }
    void setIsAnonymous(bool b) { m_isAnonymous = b; }
    bool isAnonymousBlock() const { return m_isAnonymous && 
                                           style()->display() == BLOCK && 
                                           style()->styleType() == RenderStyle::NOPSEUDO &&
                                           !isListMarker(); }
    
    bool isFloating() const { return m_floating; }
    bool isPositioned() const { return m_positioned; } // absolute or fixed positioning
    bool isRelPositioned() const { return m_relPositioned; } // relative positioning
    bool isText() const  { return m_isText; }
    bool isInline() const { return m_inline; }  // inline object
    bool isCompact() const { return style()->display() == COMPACT; } // compact object
    bool isRunIn() const { return style()->display() == RUN_IN; } // run-in object
    bool isDragging() const;
    bool isReplaced() const { return m_replaced; } // a "replaced" element (see CSS)
    bool shouldPaintBackgroundOrBorder() const { return m_paintBackground; }
    bool mustRepaintBackgroundOrBorder() const;
    bool needsLayout() const   { return m_needsLayout || m_normalChildNeedsLayout || m_posChildNeedsLayout; }
    bool selfNeedsLayout() const { return m_needsLayout; }
    bool posChildNeedsLayout() const { return m_posChildNeedsLayout; }
    bool normalChildNeedsLayout() const { return m_normalChildNeedsLayout; }
    bool minMaxKnown() const{ return m_minMaxKnown; }
    bool recalcMinMax() const { return m_recalcMinMax; }
    bool isSelectionBorder() const;
    
    bool hasOverflowClip() const { return m_hasOverflowClip; }
    
    bool hasAutoVerticalScrollbar() const { return hasOverflowClip() && (style()->overflowY() == OAUTO || style()->overflowY() == OOVERLAY); }
    bool hasAutoHorizontalScrollbar() const { return hasOverflowClip() && (style()->overflowX() == OAUTO || style()->overflowX() == OOVERLAY); }

    bool scrollsOverflow() const { return scrollsOverflowX() || scrollsOverflowY(); }
    bool scrollsOverflowX() const { return hasOverflowClip() && (style()->overflowX() == OSCROLL || hasAutoHorizontalScrollbar()); }
    bool scrollsOverflowY() const { return hasOverflowClip() && (style()->overflowY() == OSCROLL || hasAutoVerticalScrollbar()); }
    
    bool includeVerticalScrollbarSize() const { return hasOverflowClip() && (style()->overflowY() == OSCROLL || style()->overflowY() == OAUTO); }
    bool includeHorizontalScrollbarSize() const { return hasOverflowClip() && (style()->overflowX() == OSCROLL || style()->overflowX() == OAUTO); }

    RenderStyle* getPseudoStyle(RenderStyle::PseudoId pseudo, RenderStyle* parentStyle = 0) const;
    
    void updateDragState(bool dragOn);

    RenderView* view() const;

    // don't even think about making this method virtual!
    Node* element() const { return m_isAnonymous ? 0 : m_node; }
    Document* document() const { return m_node->document(); }
    void setNode(Node* node) { m_node = node; }
    Node* node() const { return m_node; }
    
   /**
     * returns the object containing this one. can be different from parent for
     * positioned elements
     */
    RenderObject *container() const;
    RenderObject* hoverAncestor() const;

    virtual void markAllDescendantsWithFloatsForLayout(RenderObject* floatToRemove = 0);
    void markContainingBlocksForLayout(bool scheduleRelayout = true);
    void setNeedsLayout(bool b, bool markParents = true);
    void setChildNeedsLayout(bool b, bool markParents = true);
    void setMinMaxKnown(bool b=true) {
        m_minMaxKnown = b;
        if ( !b ) {
            RenderObject *o = this;
            RenderObject *root = this;
            while( o ) { // ### && !o->m_recalcMinMax ) {
                o->m_recalcMinMax = true;
                root = o;
                o = o->m_parent;
            }
        }
    }

    void setNeedsLayoutAndMinMaxRecalc() {
        setMinMaxKnown(false);
        setNeedsLayout(true);
    }
    
    void setPositioned(bool b=true)  { m_positioned = b;  }
    void setRelPositioned(bool b=true) { m_relPositioned = b; }
    void setFloating(bool b=true) { m_floating = b; }
    void setInline(bool b=true) { m_inline = b; }
    void setShouldPaintBackgroundOrBorder(bool b=true) { m_paintBackground = b; }
    void setRenderText() { m_isText = true; }
    void setReplaced(bool b=true) { m_replaced = b; }
    void setHasOverflowClip(bool b = true) { m_hasOverflowClip = b; }

    void scheduleRelayout();
    
    void updateBackgroundImages(RenderStyle* oldStyle);

    virtual InlineBox* createInlineBox(bool makePlaceHolderBox, bool isRootLineBox, bool isOnlyRun=false);
    virtual void dirtyLineBoxes(bool fullLayout, bool isRootLineBox=false);
    
    // For inline replaced elements, this function returns the inline box that owns us.  Enables
    // the replaced RenderObject to quickly determine what line it is contained on and to easily
    // iterate over structures on the line.
    virtual InlineBox* inlineBoxWrapper() const;
    virtual void setInlineBoxWrapper(InlineBox* b);
    virtual void deleteLineBoxWrapper();

    virtual InlineBox *inlineBox(int offset=0, EAffinity affinity = UPSTREAM);
    
    // for discussion of lineHeight see CSS2 spec
    virtual short lineHeight( bool firstLine, bool isRootLineBox=false ) const;
    // for the vertical-align property of inline elements
    // the difference between this objects baseline position and the lines baseline position.
    virtual short verticalPositionHint( bool firstLine ) const;
    // the offset of baseline from the top of the object.
    virtual short baselinePosition( bool firstLine, bool isRootLineBox=false ) const;
    // width of tab character
    int tabWidth() const;

    // Hook so that RenderTextControl can return the line height of its inner renderer.
    // For other renderers, the value is the same as lineHeight(false).
    virtual short innerLineHeight() const;

    /*
     * Paint the object and its children, clipped by (x|y|w|h).
     * (tx|ty) is the calculated position of the parent
     */
    struct PaintInfo {
        PaintInfo(GraphicsContext* _p, const IntRect& _r, PaintPhase _phase, bool _forceWhiteText, RenderObject* _paintingRoot, RenderFlowSequencedSet* _outlineObjects)
            : p(_p), r(_r), phase(_phase), forceWhiteText(_forceWhiteText), paintingRoot(_paintingRoot), outlineObjects(_outlineObjects) {}
        GraphicsContext* p;
        IntRect r;
        PaintPhase phase;
        bool forceWhiteText;
        RenderObject* paintingRoot; // used to draw just one element and its visual kids
        RenderFlowSequencedSet* outlineObjects; // used to list outlines that should be painted by a block with inline children
    };
    virtual void paint(PaintInfo&, int tx, int ty);
    void paintBorder(GraphicsContext*, int tx, int ty, int w, int h, const RenderStyle*, bool begin = true, bool end = true);
    bool paintBorderImage(GraphicsContext*, int tx, int ty, int w, int h, const RenderStyle* style);
    void paintOutline(GraphicsContext*, int tx, int ty, int w, int h, const RenderStyle* style);

    // RenderBox implements this.
    virtual void paintBoxDecorations(PaintInfo&, int tx, int ty) {}

    virtual void paintBackgroundExtended(GraphicsContext*, const Color&, const BackgroundLayer*, int clipy, int cliph,
        int tx, int ty, int w, int height, int bleft, int bright, int pleft, int pright) {}

    /*
     * This function calculates the minimum & maximum width that the object
     * can be set to.
     *
     * when the Element calls setMinMaxKnown(true), calcMinMaxWidth() will
     * be no longer called.
     *
     * when a element has a fixed size, m_minWidth and m_maxWidth should be
     * set to the same value. This has the special meaning that m_width,
     * contains the actual value.
     *
     * assumes calcMinMaxWidth has already been called for all children.
     */
    virtual void calcMinMaxWidth() { }

    /*
     * Does the min max width recalculations after changes.
     */
    void recalcMinMaxWidths();

    /*
     * Calculates the actual width of the object (only for non inline
     * objects)
     */
    virtual void calcWidth() {}

    /*
     * This function should cause the Element to calculate its
     * width and height and the layout of its content
     *
     * when the Element calls setNeedsLayout(false), layout() is no
     * longer called during relayouts, as long as there is no
     * style sheet change. When that occurs, m_needsLayout will be
     * set to true and the Element receives layout() calls
     * again.
     */
    virtual void layout() = 0;

    /* This function performs a layout only if one is needed. */
    void layoutIfNeeded() { if (needsLayout()) layout(); }

    // used for element state updates that can not be fixed with a
    // repaint and do not need a relayout
    virtual void updateFromElement() {};

    virtual int availableHeight() const { return 0; }

    virtual void updateWidgetPosition();
    
    DeprecatedValueList<DashboardRegionValue> computeDashboardRegions();
    void addDashboardRegions (DeprecatedValueList<DashboardRegionValue>& regions);
    void collectDashboardRegions (DeprecatedValueList<DashboardRegionValue>& regions);

    // does a query on the rendertree and finds the innernode
    // and overURL for the given position
    // if readonly == false, it will recalc hover styles accordingly
    class NodeInfo
    {
        friend class RenderLayer;
        friend class RenderImage;
        friend class RenderText;
        friend class RenderInline;
        friend class RenderObject;
        friend class RenderFrameSet;
        friend class HTMLAreaElement;
    public:
        NodeInfo(bool readonly, bool active, bool mouseMove = false)
            : m_innerNode(0), m_innerNonSharedNode(0), m_innerURLElement(0), m_readonly(readonly), m_active(active), m_mouseMove(mouseMove)
            { }

        Node* innerNode() const { return m_innerNode; }
        Node* innerNonSharedNode() const { return m_innerNonSharedNode; }
        Element* URLElement() const { return m_innerURLElement; }
        bool readonly() const { return m_readonly; }
        bool active() const { return m_active; }
        bool mouseMove() const { return m_mouseMove; }
        
        void setInnerNode(Node* n) { m_innerNode = n; }
        void setInnerNonSharedNode(Node* n) { m_innerNonSharedNode = n; }
        void setURLElement(Element* n) { m_innerURLElement = n; }

    private:
        Node* m_innerNode;
        Node* m_innerNonSharedNode;
        Element* m_innerURLElement;
        bool m_readonly;
        bool m_active;
        bool m_mouseMove;
    };

    // Used to signal a specific subrect within an object that must be repainted after
    // layout is complete.
    struct RepaintInfo {
        RenderObject* m_object;
        IntRect m_repaintRect;
    
        RepaintInfo(RenderObject* o, const IntRect& r) :m_object(o), m_repaintRect(r) {}
    };
    
    bool hitTest(NodeInfo& info, int x, int y, int tx, int ty, HitTestFilter hitTestFilter = HitTestAll);
    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty,
                             HitTestAction hitTestAction);
    void setInnerNode(NodeInfo& info);

    virtual VisiblePosition positionForCoordinates(int x, int y);
    VisiblePosition positionForPoint(const IntPoint& point) { return positionForCoordinates(point.x(), point.y()); }
    
    virtual void dirtyLinesFromChangedChild(RenderObject* child);
    
    // Set the style of the object and update the state of the object accordingly.
    virtual void setStyle(RenderStyle* style);

    // Updates only the local style ptr of the object.  Does not update the state of the object,
    // and so only should be called when the style is known not to have changed (or from setStyle).
    void setStyleInternal(RenderStyle* style);

    // returns the containing block level element for this element.
    RenderBlock *containingBlock() const;

    // return just the width of the containing block
    virtual int containingBlockWidth() const;
    // return just the height of the containing block
    virtual int containingBlockHeight() const;

    // size of the content area (box size minus padding/border)
    virtual int contentWidth() const { return 0; }
    virtual int contentHeight() const { return 0; }

    // intrinsic extend of replaced elements. undefined otherwise
    virtual int intrinsicWidth() const { return 0; }
    virtual int intrinsicHeight() const { return 0; }

    // used by flexible boxes to impose a flexed width/height override
    virtual int overrideSize() const { return 0; }
    virtual int overrideWidth() const { return 0; }
    virtual int overrideHeight() const { return 0; }
    virtual void setOverrideSize(int s) {}

    // relative to parent node
    virtual void setPos( int /*xPos*/, int /*yPos*/ ) { }
    virtual void setWidth( int /*width*/ ) { }
    virtual void setHeight( int /*height*/ ) { }

    virtual int xPos() const { return 0; }
    virtual int yPos() const { return 0; }

    // calculate client position of box
    virtual bool absolutePosition(int &/*xPos*/, int &/*yPos*/, bool fixed = false);
    
    // This function is used to deal with the extra top space that can occur in table cells (called borderTopExtra).
    // The children of the cell do not factor this space in, so we have to add it in.  Any code that wants to
    // accurately deal with the contents of a cell must call this function instad of absolutePosition.
    void absolutePositionForContent(int& xPos, int& yPos, bool fixed = false) {
        absolutePosition(xPos, yPos, fixed);
        yPos += borderTopExtra();
    }

    // width and height are without margins but include paddings and borders
    virtual int width() const { return 0; }
    virtual int height() const { return 0; }

    virtual IntRect borderBox() const { return IntRect(0, 0, width(), height()); }

    // The height of a block when you include normal flow overflow spillage out of the bottom
    // of the block (e.g., a <div style="height:25px"> that has a 100px tall image inside
    // it would have an overflow height of borderTop() + paddingTop() + 100px.
    virtual int overflowHeight(bool includeInterior=true) const { return height(); }
    virtual int overflowWidth(bool includeInterior=true) const { return width(); }
    virtual void setOverflowHeight(int) {}
    virtual void setOverflowWidth(int) {}
    virtual int overflowLeft(bool includeInterior=true) const { return 0; }
    virtual int overflowTop(bool includeInterior=true) const { return 0; }
    virtual IntRect overflowRect(bool includeInterior=true) const { return borderBox(); }

    // IE extensions. Used to calculate offsetWidth/Height.  Overridden by inlines (RenderFlow) 
    // to return the remaining width on a given line (and the height of a single line). -dwh
    virtual int offsetWidth() const { return width(); }
    virtual int offsetHeight() const { return height() + borderTopExtra() + borderBottomExtra(); }
    
    // IE exxtensions.  Also supported by Gecko.  We override in render flow to get the
    // left and top correct. -dwh
    virtual int offsetLeft() const;
    virtual int offsetTop() const;
    virtual RenderObject* offsetParent() const;

    // More IE extensions.  clientWidth and clientHeight represent the interior of an object
    // excluding border and scrollbar.  clientLeft/Top are just the borderLeftWidth and borderTopWidth.
    int clientLeft() const { return borderLeft(); }
    int clientTop() const { return borderTop(); }
    int clientWidth() const;
    int clientHeight() const;

    // scrollWidth/scrollHeight will be the same as clientWidth/clientHeight unless the
    // object has overflow:hidden/scroll/auto specified and also has overflow.
    // scrollLeft/Top return the current scroll position.  These methods are virtual so that objects like
    // textareas can scroll shadow content (but pretend that they are the objects that are
    // scrolling).
    virtual int scrollLeft() const;
    virtual int scrollTop() const;
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);
    
    virtual bool scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier=1.0);

    // The following seven functions are used to implement collapsing margins.
    // All objects know their maximal positive and negative margins.  The
    // formula for computing a collapsed margin is |maxPosMargin|-|maxNegmargin|.
    // For a non-collapsing, e.g., a leaf element, this formula will simply return
    // the margin of the element.  Blocks override the maxTopMargin and maxBottomMargin
    // methods.
    virtual bool isSelfCollapsingBlock() const { return false; }
    virtual int collapsedMarginTop() const 
        { return maxTopMargin(true)-maxTopMargin(false); }
    virtual int collapsedMarginBottom() const 
        { return maxBottomMargin(true)-maxBottomMargin(false); }
    virtual bool isTopMarginQuirk() const { return false; }
    virtual bool isBottomMarginQuirk() const { return false; }
    virtual int maxTopMargin(bool positive) const {
        if (positive)
            if (marginTop() > 0)
                return marginTop();
            else
                return 0;
        else
            if (marginTop() < 0)
                return 0 - marginTop();
            else
                return 0;
    }
    virtual int maxBottomMargin(bool positive) const {
        if (positive)
            if (marginBottom() > 0)
                return marginBottom();
            else
                return 0;
        else
            if (marginBottom() < 0)
                return 0 - marginBottom();
            else
                return 0;
    }

    virtual int marginTop() const { return 0; }
    virtual int marginBottom() const { return 0; }
    virtual int marginLeft() const { return 0; }
    virtual int marginRight() const { return 0; }

    // Virtual since table cells override 
    virtual int paddingTop() const;
    virtual int paddingBottom() const;
    virtual int paddingLeft() const;
    virtual int paddingRight() const;
    
    virtual int borderTop() const { return style()->borderTopWidth(); }
    virtual int borderBottom() const { return style()->borderBottomWidth(); }
    virtual int borderTopExtra() const { return 0; }
    virtual int borderBottomExtra() const { return 0; }
    virtual int borderLeft() const { return style()->borderLeftWidth(); }
    virtual int borderRight() const { return style()->borderRightWidth(); }

    virtual DeprecatedValueList<IntRect> lineBoxRects();

    virtual void absoluteRects(DeprecatedValueList<IntRect>& rects, int tx, int ty);
    IntRect absoluteBoundingBoxRect();
    
    // the rect that will be painted if this object is passed as the paintingRoot
    IntRect paintingRootRect(IntRect& topLevelRect);

    virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);

    virtual int minWidth() const { return 0; }
    virtual int maxWidth() const { return 0; }

    RenderStyle* style() const { return m_style; }
    RenderStyle* firstLineStyle() const;
    RenderStyle* style(bool firstLine) const { return firstLine ? firstLineStyle() : style(); }


    void getTextDecorationColors(int decorations, Color& underline, Color& overline,
                                 Color& linethrough, bool quirksMode=false);

    enum BorderSide {
        BSTop, BSBottom, BSLeft, BSRight
    };
    void drawBorderArc(GraphicsContext*, int x, int y, float thickness, IntSize radius, int angleStart,
        int angleSpan, BorderSide, Color, EBorderStyle, bool firstCorner);
    void drawBorder(GraphicsContext*, int x1, int y1, int x2, int y2, BorderSide,
        Color, const Color& textcolor, EBorderStyle, int adjbw1, int adjbw2, bool invalidisInvert = false);

    virtual void setTable(RenderTable*) {};

    // Used by collapsed border tables.
    virtual void collectBorders(DeprecatedValueList<CollapsedBorderValue>& borderStyles);

    // Repaint the entire object.  Called when, e.g., the color of a border changes, or when a border
    // style changes.
    void repaint(bool immediate = false);

    // Repaint a specific subrectangle within a given object.  The rect |r| is in the object's coordinate space.
    void repaintRectangle(const IntRect& r, bool immediate = false);
    
    // Repaint only if our old bounds and new bounds are different.
    bool repaintAfterLayoutIfNeeded(const IntRect& oldBounds, const IntRect& oldFullBounds);

    // Repaint only if the object moved.
    virtual void repaintDuringLayoutIfMoved(int oldX, int oldY);

    // Called to repaint a block's floats.
    virtual void repaintOverhangingFloats(bool paintAllDescendants = false);

    // Called before layout to repaint all dirty children (with selfNeedsLayout() set).
    virtual void repaintObjectsBeforeLayout();

    bool checkForRepaintDuringLayout() const;

    // Returns the rect that should be repainted whenever this object changes.  The rect is in the view's
    // coordinate space.  This method deals with outlines and overflow.
    virtual IntRect getAbsoluteRepaintRect();

    IntRect getAbsoluteRepaintRectWithOutline(int ow);

    virtual void getAbsoluteRepaintRectIncludingFloats(IntRect& bounds, IntRect& boundsWithChildren);

    // Given a rect in the object's coordinate space, this method converts the rectangle to the view's
    // coordinate space.
    virtual void computeAbsoluteRepaintRect(IntRect& r, bool f=false);
    
    virtual unsigned int length() const { return 1; }

    bool isFloatingOrPositioned() const { return (isFloating() || isPositioned()); };
    virtual bool containsFloats() { return false; }
    virtual bool containsFloat(RenderObject* o) { return false; }
    virtual bool hasOverhangingFloats() { return false; }
    virtual IntRect floatRect() const { return borderBox(); }

    virtual void removePositionedObjects(RenderBlock*) { }

    bool avoidsFloats() const;
    bool shrinkToAvoidFloats() const;

    // positioning of inline children (bidi)
    virtual void position(InlineBox*, int, int, bool, bool) {}

    bool isTransparent() const { return style()->opacity() < 1.0f; }
    float opacity() const { return style()->opacity(); }

    // Applied as a "slop" to dirty rect checks during the outline painting phase's dirty-rect checks.
    int maximalOutlineSize(PaintPhase p) const;

    enum SelectionState {
        SelectionNone, // The object is not selected.
        SelectionStart, // The object either contains the start of a selection run or is the start of a run
        SelectionInside, // The object is fully encompassed by a selection run
        SelectionEnd, // The object either contains the end of a selection run or is the end of a run
        SelectionBoth // The object contains an entire run or is the sole selected object in that run
    };

    // The current selection state for an object.  For blocks, the state refers to the state of the leaf
    // descendants (as described above in the SelectionState enum declaration).
    virtual SelectionState selectionState() const { return SelectionNone; }
    
    // Sets the selection state for an object.
    virtual void setSelectionState(SelectionState s) { if (parent()) parent()->setSelectionState(s); }

    // A single rectangle that encompasses all of the selected objects within this object.  Used to determine the tightest
    // possible bounding box for the selection.
    virtual IntRect selectionRect() { return IntRect(); }
    
    // Whether or not an object can be part of the leaf elements of the selection.
    virtual bool canBeSelectionLeaf() const { return false; }

    // Whether or not a block has selected children.
    virtual bool hasSelectedChildren() const { return false; }

    // Whether or not a selection can be attempted on this object.
    bool canSelect() const;

    // Whether or not a selection can be attempted on this object.  Should only be called right before actually beginning a selection,
    // since it fires the selectstart DOM event.
    bool shouldSelect() const;
    
    // Obtains the selection colors that should be used when painting a selection.
    Color selectionBackgroundColor() const;
    Color selectionForegroundColor() const;

    // Whether or not a given block needs to paint selection gaps.
    virtual bool shouldPaintSelectionGaps() const { return false; }

    // This struct is used when the selection changes to cache the old and new state of the selection for each RenderObject.
    struct SelectionInfo {
        RenderObject* m_object;
        IntRect m_rect;
        RenderObject::SelectionState m_state;

        RenderObject* object() const { return m_object; }
        IntRect rect() const { return m_rect; }
        SelectionState state() const { return m_state; }
        
        SelectionInfo() { m_object = 0; m_state = SelectionNone; }
        SelectionInfo(RenderObject* o) :m_object(o), m_rect(o->selectionRect()), m_state(o->selectionState()) {}
    };

    Node* draggableNode(bool dhtmlOK, bool uaOK, int x, int y, bool& dhtmlWillDrag) const;

    /**
     * Returns the content coordinates of the caret within this render object.
     * @param offset zero-based offset determining position within the render object.
     * @param override @p true if input overrides existing characters,
     * @p false if it inserts them. The width of the caret depends on this one.
     * @param extraWidthToEndOfLine optional out arg to give extra width to end of line -
     * useful for character range rect computations
     */
    virtual IntRect caretRect(int offset, EAffinity affinity = UPSTREAM, int *extraWidthToEndOfLine = 0);

    virtual int lowestPosition(bool includeOverflowInterior=true, bool includeSelf=true) const { return 0; }
    virtual int rightmostPosition(bool includeOverflowInterior=true, bool includeSelf=true) const { return 0; }
    virtual int leftmostPosition(bool includeOverflowInterior=true, bool includeSelf=true) const { return 0; }
    
    virtual void calcVerticalMargins() {}
    void removeFromObjectLists();

    // When performing a global document tear-down, the renderer of the document is cleared.  We use this
    // as a hook to detect the case of document destruction and don't waste time doing unnecessary work.
    bool documentBeingDestroyed() const;

    virtual void destroy();

    const Font& font(bool firstLine) const {
        return style(firstLine)->font();
    }

    // Virtual function helpers for CSS3 Flexible Box Layout
    virtual bool isFlexibleBox() const { return false; }
    virtual bool isFlexingChildren() const { return false; }
    virtual bool isStretchingChildren() const { return false; }

    // Convenience, to avoid repeating the code to dig down to get this.
    UChar backslashAsCurrencySymbol() const;

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;

    virtual int previousOffset (int current) const;
    virtual int nextOffset (int current) const;

    virtual void imageChanged(CachedImage*);
    virtual bool willRenderImage(CachedImage*);

    virtual void selectionStartEnd(int& spos, int& epos);

    RenderObject* paintingRootForChildren(PaintInfo &i) const {
        // if we're the painting root, kids draw normally, and see root of 0
        return (!i.paintingRoot || i.paintingRoot == this) ? 0 : i.paintingRoot;
    }

    bool shouldPaintWithinRoot(PaintInfo &i) const {
        return !i.paintingRoot || i.paintingRoot == this;
    }
    
protected:
    virtual void printBoxDecorations(GraphicsContext*, int /*x*/, int /*y*/,
        int /*w*/, int /*h*/, int /*tx*/, int /*ty*/) {}

    virtual IntRect viewRect() const;

    void remove() { if (parent()) parent()->removeChild(this); }

    void invalidateVerticalPositions();
    short getVerticalPosition(bool firstLine) const;

    virtual void removeLeftoverAnonymousBoxes();
    
    void arenaDelete(RenderArena*, void* objectBase);

private:
    RenderStyle* m_style;

    Node* m_node;

    RenderObject *m_parent;
    RenderObject *m_previous;
    RenderObject *m_next;

    mutable short m_verticalPosition;

    bool m_needsLayout               : 1;
    bool m_normalChildNeedsLayout    : 1;
    bool m_posChildNeedsLayout       : 1;
    bool m_minMaxKnown               : 1;
    bool m_floating                  : 1;

    bool m_positioned                : 1;
    bool m_relPositioned             : 1;
    bool m_paintBackground           : 1; // if the box has something to paint in the
                                          // background painting phase (background, border, etc)

    bool m_isAnonymous               : 1;
    bool m_recalcMinMax              : 1;
    bool m_isText                    : 1;
    bool m_inline                    : 1;
    bool m_replaced                  : 1;
    bool m_isDragging                : 1;
    
    bool m_hasOverflowClip           : 1;

    // note: do not add unnecessary bitflags, we have 32 bit already!
    friend class RenderListItem;
    friend class RenderContainer;
    friend class RenderView;
};


enum VerticalPositionHint {
    PositionTop = -0x4000,
    PositionBottom = 0x4000,
    PositionUndefined = 0x3fff
};

} //namespace

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::RenderObject*);
#endif


#endif
