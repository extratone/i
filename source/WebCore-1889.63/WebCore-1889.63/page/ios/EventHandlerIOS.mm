/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "EventHandler.h"

#include "AXObjectCache.h"
#include "BlockExceptions.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "KeyboardEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "Page.h"
#include "PlatformEventFactoryIOS.h"
#include "RenderWidget.h"
#include "WAKView.h"
#include "WAKWindow.h"
#include "WebEvent.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

static RetainPtr<WebEvent>& currentEventSlot()
{
    DEFINE_STATIC_LOCAL(RetainPtr<WebEvent>, event, ());
    return event;
}

WebEvent *EventHandler::currentEvent()
{
    return currentEventSlot().get();
}

class CurrentEventScope {
    WTF_MAKE_NONCOPYABLE(CurrentEventScope);
public:
    CurrentEventScope(WebEvent *);
    ~CurrentEventScope();

private:
    RetainPtr<WebEvent> m_savedCurrentEvent;
#ifndef NDEBUG
    RetainPtr<WebEvent> m_event;
#endif
};

inline CurrentEventScope::CurrentEventScope(WebEvent *event)
    : m_savedCurrentEvent(currentEventSlot())
#ifndef NDEBUG
    , m_event(event)
#endif
{
    currentEventSlot() = event;
}

inline CurrentEventScope::~CurrentEventScope()
{
    ASSERT(currentEventSlot() == m_event);
    currentEventSlot() = m_savedCurrentEvent;
}

bool EventHandler::wheelEvent(WebEvent *event)
{
    Page* page = m_frame->page();
    if (!page)
        return false;

    CurrentEventScope scope(event);

    bool eventWasHandled = handleWheelEvent(PlatformEventFactory::createPlatformWheelEvent(event));
    event.wasHandled = eventWasHandled;
    return eventWasHandled;
}

#if ENABLE(TOUCH_EVENTS)
void EventHandler::touchEvent(WebEvent *event)
{
    CurrentEventScope scope(event);

    event.wasHandled = handleTouchEvent(PlatformEventFactory::createPlatformTouchEvent(event));
}
#endif

static inline bool isKeyboardOptionTab(KeyboardEvent* event)
{
    return event
        && (event->type() == eventNames().keydownEvent || event->type() == eventNames().keypressEvent)
        && event->altKey()
        && event->keyIdentifier() == "U+0009";    
}

bool EventHandler::tabsToAllFormControls(KeyboardEvent* event) const
{
    Page* page = m_frame->page();
    if (!page)
        return false;

    KeyboardUIMode keyboardUIMode = page->chrome().client()->keyboardUIMode();
    bool handlingOptionTab = isKeyboardOptionTab(event);

    // If tab-to-links is off, option-tab always highlights all controls
    if ((keyboardUIMode & KeyboardAccessTabsToLinks) == 0 && handlingOptionTab)
        return true;
    
    // If system preferences say to include all controls, we always include all controls
    if (keyboardUIMode & KeyboardAccessFull)
        return true;
    
    // Otherwise tab-to-links includes all controls, unless the sense is flipped via option-tab.
    if (keyboardUIMode & KeyboardAccessTabsToLinks)
        return !handlingOptionTab;
    
    return handlingOptionTab;
}

bool EventHandler::keyEvent(WebEvent *event)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT(event.type == WebEventKeyDown || event.type == WebEventKeyUp);

    CurrentEventScope scope(event);
    bool eventWasHandled = keyEvent(PlatformEventFactory::createPlatformKeyboardEvent(event));
    event.wasHandled = eventWasHandled;
    return eventWasHandled;

    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

void EventHandler::focusDocumentView()
{
    Page* page = m_frame->page();
    if (!page)
        return;

    if (FrameView* frameView = m_frame->view()) {
        if (NSView *documentView = frameView->documentView())
            page->chrome().focusNSView(documentView);
    }

    page->focusController()->setFocusedFrame(m_frame);
}

bool EventHandler::passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults& event)
{
    // Figure out which view to send the event to.
    RenderObject* target = event.targetNode() ? event.targetNode()->renderer() : 0;
    if (!target || !target->isWidget())
        return false;
    
    // Double-click events don't exist in Cocoa. Since passWidgetMouseDownEventToWidget will
    // just pass currentEvent down to the widget, we don't want to call it for events that
    // don't correspond to Cocoa events.  The mousedown/ups will have already been passed on as
    // part of the pressed/released handling.
    return passMouseDownEventToWidget(static_cast<RenderWidget*>(target)->widget());
}

bool EventHandler::passWidgetMouseDownEventToWidget(RenderWidget* renderWidget)
{
    return passMouseDownEventToWidget(renderWidget->widget());
}

static bool lastEventIsMouseUp()
{
    // Many AppKit widgets run their own event loops and consume events while the mouse is down.
    // When they finish, currentEvent is the mouseUp that they exited on. We need to update
    // the WebCore state with this mouseUp, which we never saw. This method lets us detect
    // that state. Handling this was critical when we used AppKit widgets for form elements.
    // It's not clear in what cases this is helpful now -- it's possible it can be removed. 

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    WebEvent *currentEventAfterHandlingMouseDown = [WAKWindow currentEvent];
    return currentEventAfterHandlingMouseDown
        && EventHandler::currentEvent() != currentEventAfterHandlingMouseDown
        && currentEventAfterHandlingMouseDown.type == WebEventMouseUp
        && currentEventAfterHandlingMouseDown.timestamp >= EventHandler::currentEvent().timestamp;
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

bool EventHandler::passMouseDownEventToWidget(Widget* pWidget)
{
    // FIXME: This function always returns true. It should be changed either to return
    // false in some cases or the return value should be removed.
    
    RefPtr<Widget> widget = pWidget;

    if (!widget) {
        LOG_ERROR("hit a RenderWidget without a corresponding Widget, means a frame is half-constructed");
        return true;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    NSView *nodeView = widget->platformWidget();
    ASSERT(nodeView);
    ASSERT([nodeView superview]);
    NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:currentEvent().locationInWindow fromView:nil]];
    if (!view) {
        // We probably hit the border of a RenderWidget
        return true;
    }
    
    Page* page = m_frame->page();
    if (!page)
        return true;

    if (page->chrome().client()->firstResponder() != view) {
        // Normally [NSWindow sendEvent:] handles setting the first responder.
        // But in our case, the event was sent to the view representing the entire web page.
        if ([view acceptsFirstResponder] && [view needsPanelToBecomeKey])
            page->chrome().client()->makeFirstResponder(view);
    }

    // We need to "defer loading" while tracking the mouse, because tearing down the
    // page while an AppKit control is tracking the mouse can cause a crash.
    
    // FIXME: In theory, WebCore now tolerates tear-down while tracking the
    // mouse. We should confirm that, and then remove the deferrsLoading
    // hack entirely.
    
    bool wasDeferringLoading = page->defersLoading();
    if (!wasDeferringLoading)
        page->setDefersLoading(true);

    ASSERT(!m_sendingEventToSubview);
    m_sendingEventToSubview = true;
    [view mouseDown:currentEvent()];
    m_sendingEventToSubview = false;
    
    if (!wasDeferringLoading)
        page->setDefersLoading(false);

    // Remember which view we sent the event to, so we can direct the release event properly.
    m_mouseDownView = view;
    m_mouseDownWasInSubframe = false;
    
    // Many AppKit widgets run their own event loops and consume events while the mouse is down.
    // When they finish, currentEvent is the mouseUp that they exited on.  We need to update
    // the EventHandler state with this mouseUp, which we never saw.
    // If this event isn't a mouseUp, we assume that the mouseUp will be coming later.  There
    // is a hole here if the widget consumes both the mouseUp and subsequent events.
    if (lastEventIsMouseUp())
        m_mousePressed = false;

    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}
    
// Note that this does the same kind of check as [target isDescendantOf:superview].
// There are two differences: This is a lot slower because it has to walk the whole
// tree, and this works in cases where the target has already been deallocated.
static bool findViewInSubviews(NSView *superview, NSView *target)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSEnumerator *e = [[superview subviews] objectEnumerator];
    NSView *subview;
    while ((subview = [e nextObject])) {
        if (subview == target || findViewInSubviews(subview, target)) {
            return true;
        }
    }
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return false;
}

NSView *EventHandler::mouseDownViewIfStillGood()
{
    // Since we have no way of tracking the lifetime of m_mouseDownView, we have to assume that
    // it could be deallocated already. We search for it in our subview tree; if we don't find
    // it, we set it to nil.
    NSView *mouseDownView = m_mouseDownView;
    if (!mouseDownView) {
        return nil;
    }
    FrameView* topFrameView = m_frame->view();
    NSView *topView = topFrameView ? topFrameView->platformWidget() : nil;
    if (!topView || !findViewInSubviews(topView, mouseDownView)) {
        m_mouseDownView = nil;
        return nil;
    }
    return mouseDownView;
}

bool EventHandler::eventActivatedView(const PlatformMouseEvent& event) const
{
    UNUSED_PARAM(event);
    return false;
}
    
bool EventHandler::eventLoopHandleMouseUp(const MouseEventWithHitTestResults&)
{
    NSView *view = mouseDownViewIfStillGood();
    if (!view)
        return false;
    
    if (!m_mouseDownWasInSubframe) {
        ASSERT(!m_sendingEventToSubview);
        m_sendingEventToSubview = true;
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [view mouseUp:currentEvent()];
        END_BLOCK_OBJC_EXCEPTIONS;
        m_sendingEventToSubview = false;
    }
 
    return true;
}
    
bool EventHandler::passSubframeEventToSubframe(MouseEventWithHitTestResults& event, Frame* subframe, HitTestResult* hoveredNode)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebEventType currentEventType = currentEvent().type;
    switch (currentEventType) {
        case WebEventMouseMoved:
            // Since we're passing in currentNSEvent() here, we can call
            // handleMouseMoveEvent() directly, since the save/restore of
            // currentNSEvent() that mouseMoved() does would have no effect.
            ASSERT(!m_sendingEventToSubview);
            m_sendingEventToSubview = true;
            subframe->eventHandler()->handleMouseMoveEvent(currentPlatformMouseEvent(), hoveredNode);
            m_sendingEventToSubview = false;
            return true;
        
        case WebEventMouseDown: {
            Node* node = event.targetNode();
            if (!node)
                return false;
            RenderObject* renderer = node->renderer();
            if (!renderer || !renderer->isWidget())
                return false;
            Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
            if (!widget || !widget->isFrameView())
                return false;
            if (!passWidgetMouseDownEventToWidget(static_cast<RenderWidget*>(renderer)))
                return false;
            m_mouseDownWasInSubframe = true;
            return true;
        }
        case WebEventMouseUp: {
            if (!m_mouseDownWasInSubframe)
                return false;
            ASSERT(!m_sendingEventToSubview);
            m_sendingEventToSubview = true;
            subframe->eventHandler()->handleMouseReleaseEvent(currentPlatformMouseEvent());
            m_sendingEventToSubview = false;
            return true;
        }
        default:
            return false;
    }
    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

bool EventHandler::passWheelEventToWidget(const PlatformWheelEvent&, Widget* widget)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
        
    if (currentEvent().type != WebEventScrollWheel || m_sendingEventToSubview || !widget)
        return false;

    NSView* nodeView = widget->platformWidget();
    ASSERT(nodeView);
    ASSERT([nodeView superview]);
    NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:currentEvent().locationInWindow fromView:nil]];
    if (!view)
        // We probably hit the border of a RenderWidget
        return false;

    ASSERT(!m_sendingEventToSubview);
    m_sendingEventToSubview = true;
    [view scrollWheel:currentEvent()];
    m_sendingEventToSubview = false;
    return true;
            
    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

void EventHandler::mouseDown(WebEvent *event)
{
    FrameView* v = m_frame->view();
    if (!v || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    m_frame->loader()->resetMultipleFormSubmissionProtection();

    m_mouseDownView = nil;
    
    CurrentEventScope scope(event);

    event.wasHandled = handleMousePressEvent(currentPlatformMouseEvent());

    END_BLOCK_OBJC_EXCEPTIONS;
}

void EventHandler::mouseUp(WebEvent *event)
{
    FrameView* v = m_frame->view();
    if (!v || m_sendingEventToSubview)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    CurrentEventScope scope(event);

    event.wasHandled = handleMouseReleaseEvent(currentPlatformMouseEvent());
    
    m_mouseDownView = nil;

    END_BLOCK_OBJC_EXCEPTIONS;
}

void EventHandler::mouseMoved(WebEvent *event)
{
    // Reject a mouse moved if the button is down - screws up tracking during autoscroll
    // These happen because WebKit sometimes has to fake up moved events.
    if (!m_frame->view() || m_mousePressed || m_sendingEventToSubview)
        return;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    m_frame->document()->updateStyleIfNeeded();

    WKBeginObservingContentChanges(true);
    CurrentEventScope scope(event);
    event.wasHandled = mouseMoved(currentPlatformMouseEvent());
    m_frame->document()->updateStyleIfNeeded();
    WKStopObservingContentChanges();

    END_BLOCK_OBJC_EXCEPTIONS;
}

bool EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return passSubframeEventToSubframe(mev, subframe);
}

bool EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe, HitTestResult* hoveredNode)
{
    return passSubframeEventToSubframe(mev, subframe, hoveredNode);
}

bool EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return passSubframeEventToSubframe(mev, subframe);
}

unsigned EventHandler::accessKeyModifiers()
{
    // Control+Option key combinations are usually unused on Mac OS X, but not when VoiceOver is enabled.
    // So, we use Control in this case, even though it conflicts with Emacs-style key bindings.
    // See <https://bugs.webkit.org/show_bug.cgi?id=21107> for more detail.
    if (AXObjectCache::accessibilityEnhancedUserInterfaceEnabled())
        return PlatformKeyboardEvent::CtrlKey;

    return PlatformKeyboardEvent::CtrlKey | PlatformKeyboardEvent::AltKey;
}

PlatformMouseEvent EventHandler::currentPlatformMouseEvent() const
{
    return PlatformEventFactory::createPlatformMouseEvent(currentEvent());
}

}
