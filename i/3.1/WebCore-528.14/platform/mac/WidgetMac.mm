/*
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#import "Widget.h"

#ifdef BUILDING_ON_TIGER
#import "AutodrainedPool.h"
#endif

#import "BlockExceptions.h"
#import "Cursor.h"
#import "Document.h"
#import "Font.h"
#import "FoundationExtras.h"
#import "Frame.h"
#import "GraphicsContext.h"
#import "Page.h"
#import "PlatformMouseEvent.h"
#import "ScrollView.h"
#import "WebCoreFrameView.h"
#import "WebCoreView.h"
#import <wtf/RetainPtr.h>

@interface NSWindow (WebWindowDetails)
- (BOOL)_needsToResetDragMargins;
- (void)_setNeedsToResetDragMargins:(BOOL)needs;
@end

@interface NSView (WebSetSelectedMethods)
- (void)setIsSelected:(BOOL)isSelected;
- (void)webPlugInSetIsSelected:(BOOL)isSelected;
@end

namespace WebCore {

class WidgetPrivate {
public:
    bool mustStayInWindow;
    bool removeFromSuperviewSoon;
};

static void safeRemoveFromSuperview(NSView *view)
{
    // If the the view is the first responder, then set the window's first responder to nil so
    // we don't leave the window pointing to a view that's no longer in it.
    NSWindow *window = [view window];
    NSResponder *firstResponder = [window firstResponder];
    if ([firstResponder isKindOfClass:[NSView class]] && [(NSView *)firstResponder isDescendantOf:view])
        [window makeFirstResponder:nil];

    // Suppress the resetting of drag margins since we know we can't affect them.
    BOOL resetDragMargins = [window _needsToResetDragMargins];
    [window _setNeedsToResetDragMargins:NO];
    [view removeFromSuperview];
    [window _setNeedsToResetDragMargins:resetDragMargins];
}

Widget::Widget(NSView* view)
    : m_data(new WidgetPrivate)
{
    init(view);
    m_data->mustStayInWindow = false;
    m_data->removeFromSuperviewSoon = false;
}

Widget::~Widget()
{
    releasePlatformWidget();
    delete m_data;
}

// FIXME: Should move this to Chrome; bad layering that this knows about Frame.
void Widget::setFocus()
{
    Frame* frame = Frame::frameForWidget(this);
    if (!frame)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
 
    NSView *view = [platformWidget() _webcore_effectiveFirstResponder];
    if (Page* page = frame->page())
        page->chrome()->focusNSView(view);
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

 void Widget::setCursor(const Cursor& cursor)
 {
    if ([NSCursor currentCursor] == cursor.impl())
        return;
    [cursor.impl() set];
}

void Widget::show()
{
    if (isSelfVisible())
        return;

    setSelfVisible(true);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getOuterView() setHidden:NO];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::hide()
{
    if (!isSelfVisible())
        return;

    setSelfVisible(false);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getOuterView() setHidden:YES];
    END_BLOCK_OBJC_EXCEPTIONS;
}

IntRect Widget::frameRect() const
{
    if (!platformWidget())
        return m_frame;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return enclosingIntRect([getOuterView() frame]);
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return m_frame;
}

void Widget::setFrameRect(const IntRect& rect)
{
    m_frame = rect;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *v = getOuterView();
    NSRect f = rect;
    if (!NSEqualRects(f, [v frame])) {
        [v setFrame:f];
        [v setNeedsDisplay: NO];
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

NSView* Widget::getOuterView() const
{
    NSView* view = platformWidget();

    // If this widget's view is a WebCoreFrameScrollView then we
    // resize its containing view, a WebFrameView.
    if ([view conformsToProtocol:@protocol(WebCoreFrameScrollView)]) {
        view = [view superview];
        ASSERT(view);
    }

    return view;
}

void Widget::paint(GraphicsContext* p, const IntRect& r)
{
    if (p->paintingDisabled())
        return;
    NSView *view = getOuterView();
    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    if (currentContext == [[view window] graphicsContext] || ![currentContext isDrawingToScreen]) {
        // This is the common case of drawing into a window or printing.
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]]];
        END_BLOCK_OBJC_EXCEPTIONS;
    } else {
        // This is the case of drawing into a bitmap context other than a window backing store. It gets hit beneath
        // -cacheDisplayInRect:toBitmapImageRep:.

        // Transparent subframes are in fact implemented with scroll views that return YES from -drawsBackground (whenever the WebView
        // itself is in drawsBackground mode). In the normal drawing code path, the scroll views are never asked to draw the background,
        // so this is not an issue, but in this code path they are, so the following code temporarily turns background drwaing off.
        NSView *innerView = platformWidget();
        NSScrollView *scrollView = 0;
        if ([innerView conformsToProtocol:@protocol(WebCoreFrameScrollView)]) {
            ASSERT([innerView isKindOfClass:[NSScrollView class]]);
            NSScrollView *scrollView = static_cast<NSScrollView *>(innerView);
            // -copiesOnScroll will return NO whenever the content view is not fully opaque.
            if ([scrollView drawsBackground] && ![[scrollView contentView] copiesOnScroll])
                [scrollView setDrawsBackground:NO];
            else
                scrollView = 0;
        }

        CGContextRef cgContext = p->platformContext();
        ASSERT(cgContext == [currentContext graphicsPort]);
        CGContextSaveGState(cgContext);

        NSRect viewFrame = [view frame];
        NSRect viewBounds = [view bounds];
        // Set up the translation and (flipped) orientation of the graphics context. In normal drawing, AppKit does it as it descends down
        // the view hierarchy.
        CGContextTranslateCTM(cgContext, viewFrame.origin.x - viewBounds.origin.x, viewFrame.origin.y + viewFrame.size.height + viewBounds.origin.y);
        CGContextScaleCTM(cgContext, 1, -1);

        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        {
#ifdef BUILDING_ON_TIGER
            AutodrainedPool pool;
#endif
            NSGraphicsContext *nsContext = [NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:YES];
            [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]] inContext:nsContext];
        }
        END_BLOCK_OBJC_EXCEPTIONS;

        CGContextRestoreGState(cgContext);

        if (scrollView)
            [scrollView setDrawsBackground:YES];
    }
}

void Widget::setIsSelected(bool isSelected)
{
    NSView *view = platformWidget();
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view respondsToSelector:@selector(webPlugInSetIsSelected:)])
        [view webPlugInSetIsSelected:isSelected];
    else if ([view respondsToSelector:@selector(setIsSelected:)])
        [view setIsSelected:isSelected];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::removeFromSuperview()
{
    if (m_data->mustStayInWindow)
        m_data->removeFromSuperviewSoon = true;
    else {
        m_data->removeFromSuperviewSoon = false;
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        safeRemoveFromSuperview(getOuterView());
        END_BLOCK_OBJC_EXCEPTIONS;
    }
}

void Widget::beforeMouseDown(NSView *unusedView, Widget* widget)
{
    if (widget) {
        ASSERT_UNUSED(unusedView, unusedView == widget->getOuterView());
        ASSERT(!widget->m_data->mustStayInWindow);
        widget->m_data->mustStayInWindow = true;
    }
}

void Widget::afterMouseDown(NSView *view, Widget* widget)
{
    if (!widget) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        safeRemoveFromSuperview(view);
        END_BLOCK_OBJC_EXCEPTIONS;
    } else {
        ASSERT(widget->m_data->mustStayInWindow);
        widget->m_data->mustStayInWindow = false;
        if (widget->m_data->removeFromSuperviewSoon)
            widget->removeFromSuperview();
    }
}

IntPoint Widget::convertFromContainingWindow(const IntPoint& point) const
{
    if (!platformWidget() && parent()) {
        IntPoint result = parent()->convertFromContainingWindow(point);
        result.move(parent()->scrollX() - x(), parent()->scrollY() - y());
        return result;
    }
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return IntPoint([platformWidget() convertPoint:point fromView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return point;
}

IntRect Widget::convertFromContainingWindow(const IntRect& rect) const
{
    if (!platformWidget() && parent()) {
        IntRect result = parent()->convertFromContainingWindow(rect);
        result.move(parent()->scrollX() - x(), parent()->scrollY() - y());
        return result;
    }
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return enclosingIntRect([platformWidget() convertRect:rect fromView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return rect;
}

IntRect Widget::convertToContainingWindow(const IntRect& r) const
{
    if (!platformWidget()) {
        if (!parent())
            return r;
        IntRect result = r;
        result.move(parent()->scrollX() - x(), parent()->scrollY() - y());
        return parent()->convertToContainingWindow(result);
    }
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return IntRect([platformWidget() convertRect:r toView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return r;
}
 
IntPoint Widget::convertToContainingWindow(const IntPoint& p) const
{
    if (!platformWidget()) {
        if (!parent())
            return p;
        IntPoint result = p;
        result.move(parent()->scrollX() - x(), parent()->scrollY() - y());
        return parent()->convertToContainingWindow(result);
    }
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return IntPoint([platformWidget() convertPoint:p toView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return p;
}

void Widget::releasePlatformWidget()
{
    HardRelease(m_widget);
}

void Widget::retainPlatformWidget()
{
    HardRetain(m_widget);
}

}

