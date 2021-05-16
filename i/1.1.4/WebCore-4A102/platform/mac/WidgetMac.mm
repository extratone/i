/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "Cursor.h"
#import "Font.h"
#import "FoundationExtras.h"
#import "GraphicsContext.h"
#import "BlockExceptions.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreFrameView.h"
#import "WebCoreView.h"
#import "WebCoreWidgetHolder.h"
#import "WidgetClient.h"
#import "WKGraphics.h"

namespace WebCore {

static bool deferFirstResponderChanges;
static Widget *deferredFirstResponder;

class WidgetPrivate
{
public:
    Font font;
    NSView* view;
    WidgetClient* client;
    bool visible;
    bool mustStayInWindow;
    bool removeFromSuperviewSoon;
};

Widget::Widget() : data(new WidgetPrivate)
{
    data->view = nil;
    data->client = 0;
    data->visible = true;
    data->mustStayInWindow = false;
    data->removeFromSuperviewSoon = false;
}

Widget::Widget(NSView* view) : data(new WidgetPrivate)
{
    data->view = HardRetain(view);
    data->client = 0;
    data->visible = true;
    data->mustStayInWindow = false;
    data->removeFromSuperviewSoon = false;
}

Widget::~Widget() 
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    HardRelease(data->view);
    END_BLOCK_OBJC_EXCEPTIONS;

    if (deferredFirstResponder == this)
        deferredFirstResponder = 0;

    delete data;
}

void Widget::setEnabled(bool enabled)
{
}

bool Widget::isEnabled() const
{
    return true;
}

IntRect Widget::frameGeometry() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *v = getOuterView();
    if (v != nil) return enclosingIntRect([v frame]);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntRect();
}

bool Widget::hasFocus() const
{
    return false;
}

void Widget::setFocus()
{
}

void Widget::clearFocus()
{
    if (!hasFocus())
        return;
    FrameMac::clearDocumentFocus(this);
}

Widget::FocusPolicy Widget::focusPolicy() const
{
    // This provides support for controlling the widgets that take 
    // part in tab navigation. Widgets must:
    // 1. not be hidden by css
    // 2. be enabled
    // 3. accept first responder

    if (!client())
        return NoFocus;
    if (!client()->isVisible(const_cast<Widget*>(this)))
        return NoFocus;
    if (!isEnabled())
        return NoFocus;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (![getView() acceptsFirstResponder])
        return NoFocus;
    END_BLOCK_OBJC_EXCEPTIONS;

    return TabFocus;
}

const Font& Widget::font() const
{
    return data->font;
}

void Widget::setFont(const Font& font)
{
    data->font = font;
}

void Widget::setCursor(const Cursor& cursor)
{
}

void Widget::show()
{
    if (!data || data->visible)
        return;

    data->visible = true;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getOuterView() setHidden: NO];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::hide()
{
    if (!data || !data->visible)
        return;

    data->visible = false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getOuterView() setHidden: YES];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::setFrameGeometry(const IntRect &rect)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *v = getOuterView();
    if (v != nil) {
        NSRect f = rect;
        if (!NSEqualRects(f, [v frame])) {
            [v setFrame:f];
            [v setNeedsDisplay: NO];
        }
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

IntPoint Widget::mapFromGlobal(const IntPoint &p) const
{
    NSPoint bp = {0,0};

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    bp = [[FrameMac::bridgeForWidget(this) window] convertScreenToBase:[data->view convertPoint:p toView:nil]];
    return IntPoint(bp);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntPoint();
}

NSView* Widget::getView() const
{
    return data->view;
}

void Widget::setView(NSView* view)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    HardRetain(view);
    HardRelease(data->view);
    data->view = view;
    END_BLOCK_OBJC_EXCEPTIONS;
}

float Widget::scaleFactor() const
{
	return 1.0;
}

NSView* Widget::getOuterView() const
{
    // If this widget's view is a WebCoreFrameView the we resize its containing view, a WebFrameView.

    NSView* view = data->view;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        view = [view superview];
        ASSERT(view);
    }

    return view;
}

// FIXME: Get rid of the single use of these next two functions (frame resizing), and remove them.

GraphicsContext* Widget::lockDrawingFocus()
{
    PlatformGraphicsContext* platformContext = static_cast<PlatformGraphicsContext*>(WKGetCurrentGraphicsContext());
    return new GraphicsContext(platformContext);
}

void Widget::unlockDrawingFocus(GraphicsContext* context)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getView() unlockFocus];
    END_BLOCK_OBJC_EXCEPTIONS;
    delete context;
}

void Widget::disableFlushDrawing()
{
}

void Widget::enableFlushDrawing()
{
}

void Widget::paint(GraphicsContext* p, const IntRect& r)
{
    if (p->paintingDisabled())
        return;
    // WebCoreTextArea and WebCoreTextField both rely on the fact that we use this particular
    // NSView display method. If you change this, be sure to update them as well.
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    //[view setNeedsDisplayInRect:[view convertRect:r fromView:[view superview]]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::sendConsumedMouseUp()
{
    if (client())
        client()->sendConsumedMouseUp(this);
}

void Widget::setIsSelected(bool isSelected)
{
    [FrameMac::bridgeForWidget(this) setIsSelected:isSelected forView:getView()];
}

void Widget::addToSuperview(NSView *superview)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT(superview);
    NSView *subview = getOuterView();
    if (!subview)
        return;
    ASSERT(![superview isDescendantOf:subview]);
    if ([subview superview] != superview)
        [superview addSubview:subview];
    data->removeFromSuperviewSoon = false;

    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::removeFromSuperview()
{
    if (data->mustStayInWindow)
        data->removeFromSuperviewSoon = true;
    else {
        data->removeFromSuperviewSoon = false;
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [getOuterView() removeFromSuperview];
        END_BLOCK_OBJC_EXCEPTIONS;
    }
}

void Widget::beforeMouseDown(NSView *view)
{
    ASSERT([view conformsToProtocol:@protocol(WebCoreWidgetHolder)]);
    Widget* widget = [(NSView <WebCoreWidgetHolder> *)view widget];
    if (widget) {
        ASSERT(view == widget->getOuterView());
        ASSERT(!widget->data->mustStayInWindow);
        widget->data->mustStayInWindow = true;
    }
}

void Widget::afterMouseDown(NSView *view)
{
    ASSERT([view conformsToProtocol:@protocol(WebCoreWidgetHolder)]);
    Widget* widget = [(NSView <WebCoreWidgetHolder>*)view widget];
    if (!widget) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [view removeFromSuperview];
        END_BLOCK_OBJC_EXCEPTIONS;
    } else {
        ASSERT(widget->data->mustStayInWindow);
        widget->data->mustStayInWindow = false;
        if (widget->data->removeFromSuperviewSoon)
            widget->removeFromSuperview();
    }
}

void Widget::setDeferFirstResponderChanges(bool defer)
{
    deferFirstResponderChanges = defer;
    if (!defer) {
        Widget* r = deferredFirstResponder;
        deferredFirstResponder = 0;
        if (r) {
            r->setFocus();
        }
    }
}

void Widget::setClient(WidgetClient* c)
{
    data->client = c;
}

WidgetClient* Widget::client() const
{
    return data->client;
}

}
