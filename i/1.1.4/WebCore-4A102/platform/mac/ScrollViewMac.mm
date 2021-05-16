/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "ScrollView.h"

#import "FloatRect.h"
#import "IntRect.h"
#import "BlockExceptions.h"
#import "Logging.h"
#import "WebCoreFrameView.h"

#import "WAKScrollView.h"
#import "WAKViewPrivate.h"
#import "WKScrollView.h"
#import "WKClipView.h"
#import "WKViewPrivate.h"
#import "WAKWindow.h"


/*
    This class implementation does NOT actually emulate the Qt ScrollView.
    It does provide an implementation that khtml will use to interact with
    WebKit's WebFrameView documentView and our NSScrollView subclass.

    ScrollView's view is a NSScrollView (or subclass of NSScrollView)
    in most cases. That scrollview is a subview of an
    WebCoreFrameView. The WebCoreFrameView's documentView will also be
    the scroll view's documentView.
    
    The WebCoreFrameView's size is the frame size.  The WebCoreFrameView's documentView
    corresponds to the frame content size.  The scrollview itself is autosized to the
    WebCoreFrameView's size (see Widget::resize).
*/

@interface NSView (KWQExtensions)
- (BOOL)_KWQ_isScrollView;
@end

@implementation NSView (KWQExtensions)

- (BOOL)_KWQ_isScrollView
{
    return [self isKindOfClass:[WAKScrollView class]];
}

@end
namespace WebCore {

int ScrollView::visibleWidth() const
{
    NSScrollView *view = (NSScrollView *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view isKindOfClass:[NSScrollView class]])
        return (int)[view documentVisibleRect].size.width;
    else
        return (int)[view bounds].size.width;
    END_BLOCK_OBJC_EXCEPTIONS;

    return 0;
}

int ScrollView::visibleHeight() const
{
    NSScrollView *view = (NSScrollView *)getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        return (int)[view documentVisibleRect].size.height;
    else
        return (int)[view bounds].size.height;
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return 0;
}

int ScrollView::actualVisibleWidth() const
{
    NSScrollView *view = (NSScrollView *)getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view isKindOfClass:[NSScrollView class]])
        return (int)[view actualDocumentVisibleRect].size.width;
    else
        return (int)[view bounds].size.width;
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return 0;
}

int ScrollView::actualVisibleHeight() const
{
    NSScrollView *view = (NSScrollView *)getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        return (int)[view actualDocumentVisibleRect].size.height;
    else
        return (int)[view bounds].size.height;
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return 0;
}

FloatRect ScrollView::visibleContentRect() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (NSView *docView = getDocumentView())
        return [docView visibleRect];
    END_BLOCK_OBJC_EXCEPTIONS;
    return FloatRect();
}

int ScrollView::contentsWidth() const
{
    NSView *docView, *view = getView();
    docView = getDocumentView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (docView)
        return (int)[docView bounds].size.width;
    else
        return (int)[view bounds].size.width;
    END_BLOCK_OBJC_EXCEPTIONS;

    return 0;
}

int ScrollView::contentsHeight() const
{
    NSView *docView, *view = getView();
    docView = getDocumentView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (docView)
        return (int)[docView bounds].size.height;
    else
        return (int)[view bounds].size.height;
    END_BLOCK_OBJC_EXCEPTIONS;

    return 0;
}

int ScrollView::actualContentsX() const
{
    NSView *view = getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        return (int)[(NSScrollView *)view contentsPoint].x;
    else
        return (int)[view visibleRect].origin.x;
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return 0;
}

int ScrollView::actualContentsY() const
{
    NSView *view = getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        return (int)[(NSScrollView *)view contentsPoint].y;
    else
        return (int)[view visibleRect].origin.y;
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return 0;
}

int ScrollView::contentsX() const
{
    NSView *view = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        return (int)[(NSScrollView *)view documentVisibleRect].origin.x;
    else
        return (int)[view visibleRect].origin.x;
    END_BLOCK_OBJC_EXCEPTIONS;

    return 0;
}

int ScrollView::contentsY() const
{
    NSView *view = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        return (int)[(NSScrollView *)view documentVisibleRect].origin.y;
    else
        return (int)[view visibleRect].origin.y;
    END_BLOCK_OBJC_EXCEPTIONS;

    return 0;
}

IntSize ScrollView::scrollOffset() const
{
    NSView *view = getView();
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view _KWQ_isScrollView]) 
        return IntPoint([[(NSScrollView *)view contentView] visibleRect].origin) - IntPoint();
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntSize();
}

void ScrollView::scrollBy(int dx, int dy)
{
    setContentsPos(actualContentsX() + dx, actualContentsY() + dy);
}

void ScrollView::scrollPointRecursively(int x, int y)
{ 
    x = (x < 0) ? 0 : x;
    y = (y < 0) ? 0 : y;
    NSPoint p = NSMakePoint(x,y);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *docView;
    NSView *view = getView();    
    docView = getDocumentView();
    if (docView)
        view = docView;
    
    NSView *originalView = view;
    while (view) {
        if ([view isKindOfClass:[NSClipView class]]) {
            NSPoint viewPoint = [view convertPoint:p fromView:originalView];
            [view scrollPoint:viewPoint];
        }
        view = [view superview];
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::setContentsPos(int x, int y)
{
    x = (x < 0) ? 0 : x;
    y = (y < 0) ? 0 : y;
    NSPoint p =  NSMakePoint(x,y);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *docView;
    NSView *view = getView();    
    docView = getDocumentView();
    if (docView)
    {
        p = [view convertPoint:p fromView:docView];
        view = docView;
    }

    [view scrollPoint:p];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::setVScrollBarMode(ScrollBarMode vMode)
{
    NSView* view = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setVerticalScrollingMode: (WebCoreScrollBarMode)vMode];
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::setHScrollBarMode(ScrollBarMode hMode)
{
    NSView* view = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setHorizontalScrollingMode: (WebCoreScrollBarMode)hMode];
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::setScrollBarsMode(ScrollBarMode mode)
{
    NSView* view = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setScrollingMode: (WebCoreScrollBarMode)mode];
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

ScrollBarMode ScrollView::vScrollBarMode() const
{
    NSView* view = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        return (ScrollBarMode)[frameView verticalScrollingMode];
    }
    END_BLOCK_OBJC_EXCEPTIONS;

    return ScrollBarAuto;
}

ScrollBarMode ScrollView::hScrollBarMode() const
{
    NSView* view = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        return (ScrollBarMode)[frameView horizontalScrollingMode];
    }
    END_BLOCK_OBJC_EXCEPTIONS;

    return ScrollBarAuto;
}

void ScrollView::suppressScrollBars(bool suppressed,  bool repaintOnUnsuppress)
{
    NSView* view = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setScrollBarsSuppressed: suppressed
                       repaintOnUnsuppress: repaintOnUnsuppress];
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::addChild(Widget* child, int x, int y)
{
    ASSERT(child != this);
    
    // we don't need to do the offscreen position initialization that KDE needs
    if (x != -500000)
        child->move(x, y);

    NSView *thisView = getView();
    NSView *thisDocView = getDocumentView();
    if (thisDocView)
        thisView = thisDocView;

    child->addToSuperview(thisView);
}

void ScrollView::removeChild(Widget* child)
{
    child->removeFromSuperview();
}

void ScrollView::resizeContents(int w, int h)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    int _w = w;
    int _h = h;

    NSView *view = getView();
    if ([view _KWQ_isScrollView]){
        view = getDocumentView();
        
        if (_w < 0)
            _w = 0;
        if (_h < 0)
            _h = 0;
            
        NSSize tempSize = { _w, _h }; // workaround for 4213314
        [view setBoundsSize:tempSize];
    } else {
        resize (_w, _h);
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::updateContents(const IntRect &rect, bool now)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *view = getView();

    if ([view _KWQ_isScrollView])
        view = getDocumentView();

    [view setNeedsDisplayInRect:rect];    
    // FIXME: Handle "now".

    END_BLOCK_OBJC_EXCEPTIONS;
}

// NB, for us "viewport" means the NSWindow's coord system, which is origin lower left

IntPoint ScrollView::contentsToViewport(const IntPoint& contentsPoint)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *docView;
    NSView *view = getView();
     
    docView = getDocumentView();
    if (docView)
        view = docView;
    
    NSPoint tempPoint = { contentsPoint.x(), contentsPoint.y() }; // workaround for 4213314
    NSPoint np = [view convertPoint:tempPoint toView: nil];
    return IntPoint(roundf(np.x), roundf(np.y));

    END_BLOCK_OBJC_EXCEPTIONS;
    
    return IntPoint();
}

IntPoint ScrollView::viewportToContents(const IntPoint& viewportPoint)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *docView;
    NSView *view = getView();    

    docView = getDocumentView();
    if (docView)
        view = docView;
    
    NSPoint tempPoint = { viewportPoint.x(), viewportPoint.y() }; // workaround for 4213314
    NSPoint np = [view convertPoint:tempPoint fromView: nil];

    return IntPoint(round(np.x), round(np.y));

    END_BLOCK_OBJC_EXCEPTIONS;

    return IntPoint();
}

void ScrollView::setStaticBackground(bool b)
{
    NSScrollView *view = (NSScrollView *)getView();
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view _KWQ_isScrollView]) {
        WKClipViewRef clipView = WKScrollViewGetContentView ((WKScrollViewRef)[view _viewRef]);
        if (clipView)
            WKClipViewSetCopiesOnScroll (clipView, true);
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

NSView *ScrollView::getDocumentView() const
{
    id view = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view respondsToSelector:@selector(documentView)]) 
        return [view documentView];
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return nil;
}

bool ScrollView::inWindow() const
{
    NSView* view = getView();
    return [view window];
}

bool ScrollView::inSuspendedWindow()
{
    WAKView* view = getView();
    return WKWindowIsSuspendedWindow([[view window] _windowRef]);
}

void ScrollView::setVisibleSizeOverrideEnabled(bool flag)
{
    WAKView *documentView = getDocumentView();
    if (documentView)
        _WKViewSetVisibleSizeOverrideEnabled([documentView _viewRef], flag);
}
}
