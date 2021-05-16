/*
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef ScrollView_h
#define ScrollView_h

#include "IntRect.h"
#include "Scrollbar.h"
#include "ScrollbarClient.h"
#include "ScrollTypes.h"
#include "Widget.h"

#include <wtf/HashSet.h>

#ifdef __OBJC__
@class WAKScrollView;
#else
class WAKScrollView;
#endif

#ifndef NSView
#define NSView WAKView
#endif

#if PLATFORM(MAC) && defined __OBJC__
@protocol WebCoreFrameScrollView;
#endif

#if PLATFORM(GTK)
typedef struct _GtkAdjustment GtkAdjustment;
#endif

#if PLATFORM(WX)
class wxScrollWinEvent;
#endif

// DANGER WILL ROBINSON! THIS FILE IS UNDERGOING HEAVY REFACTORING.
// Everything is changing!
// Port authors should wait until this refactoring is complete before attempting to implement this interface.
namespace WebCore {

class HostWindow;
class PlatformWheelEvent;
class Scrollbar;

class ScrollView : public Widget, public ScrollbarClient {
public:
    ScrollView();
    ~ScrollView();

    // ScrollbarClient method.  FrameView overrides the other two.
    virtual void valueChanged(Scrollbar*);
    
    // The window thats hosts the ScrollView.  The ScrollView will communicate scrolls and repaints to the
    // host window in the window's coordinate space.
    virtual HostWindow* hostWindow() const = 0;

    // Returns a clip rect in host window coordinates.  Used to clip the blit on a scroll.
    virtual IntRect windowClipRect(bool clipToContents = true) const = 0;

    // Methods for child manipulation and inspection.
    const HashSet<Widget*>* children() const { return &m_children; }
    void addChild(Widget*);
    void removeChild(Widget*);
    
    // If the scroll view does not use a native widget, then it will have cross-platform Scrollbars.  These methods
    // can be used to obtain those scrollbars.
    Scrollbar* horizontalScrollbar() const { return m_horizontalScrollbar.get(); }
    Scrollbar* verticalScrollbar() const { return m_verticalScrollbar.get(); }
    bool isScrollViewScrollbar(const Widget* child) const { return horizontalScrollbar() == child || verticalScrollbar() == child; }

    // Methods for setting and retrieving the scrolling mode in each axis (horizontal/vertical).  The mode has values of
    // AlwaysOff, AlwaysOn, and Auto.  AlwaysOff means never show a scrollbar, AlwaysOn means always show a scrollbar.
    // Auto means show a scrollbar only when one is needed.
    // Note that for platforms with native widgets, these modes are considered advisory.  In other words the underlying native
    // widget may choose not to honor the requested modes.
    void setScrollbarModes(ScrollbarMode horizontalMode, ScrollbarMode verticalMode);
    void setHorizontalScrollbarMode(ScrollbarMode mode) { setScrollbarModes(mode, verticalScrollbarMode()); }
    void setVerticalScrollbarMode(ScrollbarMode mode) { setScrollbarModes(horizontalScrollbarMode(), mode); }
    void scrollbarModes(ScrollbarMode& horizontalMode, ScrollbarMode& verticalMode) const;
    ScrollbarMode horizontalScrollbarMode() const { ScrollbarMode horizontal, vertical; scrollbarModes(horizontal, vertical); return horizontal; }
    ScrollbarMode verticalScrollbarMode() const { ScrollbarMode horizontal, vertical; scrollbarModes(horizontal, vertical); return vertical; }
    virtual void setCanHaveScrollbars(bool flag);
    bool canHaveScrollbars() const { return horizontalScrollbarMode() != ScrollbarAlwaysOff || verticalScrollbarMode() != ScrollbarAlwaysOff; }

    // Overridden by FrameView to create custom CSS scrollbars if applicable.
    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollbarOrientation);

    // If the prohibits scrolling flag is set, then all scrolling in the view (even programmatic scrolling) is turned off.
    void setProhibitsScrolling(bool b) { m_prohibitsScrolling = b; }
    bool prohibitsScrolling() const { return m_prohibitsScrolling; }

    // Whether or not a scroll view will blit visible contents when it is scrolled.  Blitting is disabled in situations
    // where it would cause rendering glitches (such as with fixed backgrounds or when the view is partially transparent).
    void setCanBlitOnScroll(bool);
    bool canBlitOnScroll() const;

    // The visible content rect has a location that is the scrolled offset of the document. The width and height are the viewport width
    // and height.  By default the scrollbars themselves are excluded from this rectangle, but an optional boolean argument allows them to be
    // included.
    IntRect visibleContentRect(bool includeScrollbars = false) const;
    int visibleWidth() const { return visibleContentRect().width(); }
    int visibleHeight() const { return visibleContentRect().height(); }
    IntRect actualVisibleContentRect() const;

    // Methods for getting/setting the size webkit should use to layout the contents.  By default this is the same as the visible
    // content size.  Explicitly setting a layout size value will cause webkit to layout the contents using this size instead.
    int layoutWidth() const;
    int layoutHeight() const;
    IntSize fixedLayoutSize() const;
    void setFixedLayoutSize(const IntSize&);
    bool useFixedLayout() const;
    void setUseFixedLayout(bool enable);
    
    // Methods for getting/setting the size of the document contained inside the ScrollView (as an IntSize or as individual width and height
    // values).
    IntSize contentsSize() const;
    int contentsWidth() const { return contentsSize().width(); }
    int contentsHeight() const { return contentsSize().height(); }
    virtual void setContentsSize(const IntSize&);
   
    // Methods for querying the current scrolled position (both as a point, a size, or as individual X and Y values).
    IntPoint scrollPosition() const { return visibleContentRect().location(); }
    IntSize scrollOffset() const { return visibleContentRect().location() - IntPoint(); } // Gets the scrolled position as an IntSize. Convenient for adding to other sizes.
    IntPoint maximumScrollPosition() const; // The maximum position we can be scrolled to.
    int scrollX() const { return scrollPosition().x(); }
    int scrollY() const { return scrollPosition().y(); }
    
    // Methods for scrolling the view.  setScrollPosition is the only method that really scrolls the view.  The other two methods are helper functions
    // that ultimately end up calling setScrollPosition.
    void setScrollPosition(const IntPoint&);
    void scrollBy(const IntSize& s) { return setScrollPosition(scrollPosition() + s); }
    void scrollRectIntoViewRecursively(const IntRect&);
    
    // This method scrolls by lines, pages or pixels.
    bool scroll(ScrollDirection, ScrollGranularity);
        
    // Scroll the actual contents of the view (either blitting or invalidating as needed).
    void scrollContents(const IntSize& scrollDelta);

    // This gives us a means of blocking painting on our scrollbars until the first layout has occurred.
    void setScrollbarsSuppressed(bool suppressed, bool repaintOnUnsuppress = false);
    bool scrollbarsSuppressed() const { return m_scrollbarsSuppressed; }

    // Event coordinates are assumed to be in the coordinate space of a window that contains
    // the entire widget hierarchy. It is up to the platform to decide what the precise definition
    // of containing window is. (For example on Mac it is the containing NSWindow.)
    IntPoint windowToContents(const IntPoint&) const;
    IntPoint contentsToWindow(const IntPoint&) const;
    IntRect windowToContents(const IntRect&) const;
    IntRect contentsToWindow(const IntRect&) const;

    // Methods for converting to and from screen coordinates.
    IntRect contentsToScreen(const IntRect&) const;
    IntPoint screenToContents(const IntPoint&) const;

    // The purpose of this method is to answer whether or not the scroll view is currently visible.  Animations and painting updates can be suspended if
    // we know that we are either not in a window right now or if that window is not visible.
    bool isOffscreen() const;
    
    // These methods are used to enable scrollbars to avoid window resizer controls that overlap the scroll view.  This happens on Mac
    // for example.
    virtual IntRect windowResizerRect() const { return IntRect(); }
    bool containsScrollbarsAvoidingResizer() const;
    void adjustScrollbarsAvoidingResizerCount(int overlapDelta);
    virtual void setParent(ScrollView*); // Overridden to update the overlapping scrollbar count.

    // Called when our frame rect changes (or the rect/scroll position of an ancestor changes).
    virtual void frameRectsChanged();
    
    // Widget override to update our scrollbars and notify our contents of the resize.
    virtual void setFrameRect(const IntRect&);

    // For platforms that need to hit test scrollbars from within the engine's event handlers (like Win32).
    Scrollbar* scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent);

    // This method exists for scrollviews that need to handle wheel events manually.
    // On Mac the underlying NSScrollView just does the scrolling, but on other platforms
    // (like Windows), we need this method in order to do the scroll ourselves.
    void wheelEvent(PlatformWheelEvent&);

    IntPoint convertChildToSelf(const Widget* child, const IntPoint& point) const
    {
        IntPoint newPoint = point;
        if (!isScrollViewScrollbar(child))
            newPoint = point - scrollOffset();
        newPoint.move(child->x(), child->y());
        return newPoint;
    }

    IntPoint convertSelfToChild(const Widget* child, const IntPoint& point) const
    {
        IntPoint newPoint = point;
        if (!isScrollViewScrollbar(child))
            newPoint = point + scrollOffset();
        newPoint.move(-child->x(), -child->y());
        return newPoint;
    }

    // Widget override.  Handles painting of the contents of the view as well as the scrollbars.
    virtual void paint(GraphicsContext*, const IntRect&);

    // Widget overrides to ensure that our children's visibility status is kept up to date when we get shown and hidden.
    virtual void show();
    virtual void hide();
    virtual void setParentVisible(bool);
    
    // Pan scrolling methods.
    void addPanScrollIcon(const IntPoint&);
    void removePanScrollIcon();

    virtual bool scrollbarCornerPresent() const;

protected:
    virtual void repaintContentRectangle(const IntRect&, bool now = false);
    virtual void paintContents(GraphicsContext*, const IntRect& damageRect) = 0;
    
    virtual void contentsResized() = 0;
    virtual void visibleContentsResized() = 0;
    
    // These methods are used to create/destroy scrollbars.
    void setHasHorizontalScrollbar(bool);
    void setHasVerticalScrollbar(bool);

private:
    RefPtr<Scrollbar> m_horizontalScrollbar;
    RefPtr<Scrollbar> m_verticalScrollbar;
    ScrollbarMode m_horizontalScrollbarMode;
    ScrollbarMode m_verticalScrollbarMode;
    bool m_prohibitsScrolling;

    HashSet<Widget*> m_children;

    // This bool is unused on Mac OS because we directly ask the platform widget
    // whether it is safe to blit on scroll.
    bool m_canBlitOnScroll;

    IntSize m_scrollOffset; // FIXME: Would rather store this as a position, but we will wait to make this change until more code is shared.
    IntSize m_fixedLayoutSize;
    IntSize m_contentsSize;

    int m_scrollbarsAvoidingResizer;
    bool m_scrollbarsSuppressed;

    bool m_inUpdateScrollbars;

    IntPoint m_panScrollIconPoint;
    bool m_drawPanScrollIcon;
    bool m_useFixedLayout;

    void init();
    void destroy();

    // Called to update the scrollbars to accurately reflect the state of the view.
    void updateScrollbars(const IntSize& desiredOffset);

    void platformInit();
    void platformDestroy();
    void platformAddChild(Widget*);
    void platformRemoveChild(Widget*);
    void platformSetScrollbarModes();
    void platformScrollbarModes(ScrollbarMode& horizontal, ScrollbarMode& vertical) const;
    void platformSetCanBlitOnScroll(bool);
    bool platformCanBlitOnScroll() const;
    IntRect platformVisibleContentRect(bool includeScrollbars) const;
    IntSize platformContentsSize() const;
    void platformSetContentsSize();
    IntRect platformContentsToScreen(const IntRect&) const;
    IntPoint platformScreenToContents(const IntPoint&) const;
    void platformSetScrollPosition(const IntPoint&);
    bool platformScroll(ScrollDirection, ScrollGranularity);
    void platformSetScrollbarsSuppressed(bool repaintOnUnsuppress);
    void platformRepaintContentRectangle(const IntRect&, bool now);
    bool platformIsOffscreen() const;
    bool platformHandleHorizontalAdjustment(const IntSize&);
    bool platformHandleVerticalAdjustment(const IntSize&);
    bool platformHasHorizontalAdjustment() const;
    bool platformHasVerticalAdjustment() const;

#if PLATFORM(MAC) && defined __OBJC__
public:
    NSView* documentView() const;

private:
    NSScrollView<WebCoreFrameScrollView>* scrollView() const;
#endif

#if PLATFORM(QT)
private:
    bool rootPreventsBlitting() const { return root()->m_widgetsThatPreventBlitting > 0; }
    unsigned m_widgetsThatPreventBlitting;
#else
    bool rootPreventsBlitting() const { return false; }
#endif

#if PLATFORM(GTK)
public:
    void setGtkAdjustments(GtkAdjustment* hadj, GtkAdjustment* vadj);
    GtkAdjustment* m_horizontalAdjustment;
    GtkAdjustment* m_verticalAdjustment;
    void setScrollOffset(const IntSize& offset) { m_scrollOffset = offset; }
#endif

#if PLATFORM(WX)
public:
    virtual void setPlatformWidget(wxWindow*);
    void adjustScrollbars(int x = -1, int y = -1, bool refresh = true);
private:
    class ScrollViewPrivate;
    ScrollViewPrivate* m_data;
#endif

}; // class ScrollView

} // namespace WebCore

#endif // ScrollView_h
