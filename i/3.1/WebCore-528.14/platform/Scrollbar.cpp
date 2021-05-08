/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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
#include "Scrollbar.h"

#include "EventHandler.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "ScrollbarClient.h"
#include "ScrollbarTheme.h"

#include <algorithm>

using std::max;
using std::min;

namespace WebCore {

#if !PLATFORM(GTK)
PassRefPtr<Scrollbar> Scrollbar::createNativeScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
{
    return adoptRef(new Scrollbar(client, orientation, size));
}
#endif

Scrollbar::Scrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize,
                     ScrollbarTheme* theme)
    : m_client(client)
    , m_orientation(orientation)
    , m_controlSize(controlSize)
    , m_theme(theme)
    , m_visibleSize(0)
    , m_totalSize(0)
    , m_currentPos(0)
    , m_lineStep(0)
    , m_pageStep(0)
    , m_pixelStep(1)
    , m_hoveredPart(NoPart)
    , m_pressedPart(NoPart)
    , m_pressedPos(0)
    , m_enabled(true)
    , m_scrollTimer(this, &Scrollbar::autoscrollTimerFired)
    , m_overlapsResizer(false)
    , m_suppressInvalidation(false)
{
    if (!m_theme)
        m_theme = ScrollbarTheme::nativeTheme();

    m_theme->registerScrollbar(this);

    // FIXME: This is ugly and would not be necessary if we fix cross-platform code to actually query for
    // scrollbar thickness and use it when sizing scrollbars (rather than leaving one dimension of the scrollbar
    // alone when sizing).
    int thickness = m_theme->scrollbarThickness(controlSize);
    Widget::setFrameRect(IntRect(0, 0, thickness, thickness));
}

Scrollbar::~Scrollbar()
{
    stopTimerIfNeeded();
    
    m_theme->unregisterScrollbar(this);
}

bool Scrollbar::setValue(int v)
{
    v = max(min(v, m_totalSize - m_visibleSize), 0);
    if (value() == v)
        return false; // Our value stayed the same.
    m_currentPos = v;

    updateThumbPosition();

    if (client())
        client()->valueChanged(this);
    
    return true;
}

void Scrollbar::setProportion(int visibleSize, int totalSize)
{
    if (visibleSize == m_visibleSize && totalSize == m_totalSize)
        return;

    m_visibleSize = visibleSize;
    m_totalSize = totalSize;

    updateThumbProportion();
}

void Scrollbar::setSteps(int lineStep, int pageStep, int pixelsPerStep)
{
    m_lineStep = lineStep;
    m_pageStep = pageStep;
    m_pixelStep = 1.0f / pixelsPerStep;
}

bool Scrollbar::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    float step = 0;
    if ((direction == ScrollUp && m_orientation == VerticalScrollbar) || (direction == ScrollLeft && m_orientation == HorizontalScrollbar))
        step = -1;
    else if ((direction == ScrollDown && m_orientation == VerticalScrollbar) || (direction == ScrollRight && m_orientation == HorizontalScrollbar)) 
        step = 1;
    
    if (granularity == ScrollByLine)
        step *= m_lineStep;
    else if (granularity == ScrollByPage)
        step *= m_pageStep;
    else if (granularity == ScrollByDocument)
        step *= m_totalSize;
    else if (granularity == ScrollByPixel)
        step *= m_pixelStep;
        
    float newPos = m_currentPos + step * multiplier;
    float maxPos = m_totalSize - m_visibleSize;
    newPos = max(min(newPos, maxPos), 0.0f);

    if (newPos == m_currentPos)
        return false;
    
    int oldValue = value();
    m_currentPos = newPos;
    updateThumbPosition();
    
    if (value() != oldValue && client())
        client()->valueChanged(this);
    
    // return true even if the integer value did not change so that scroll event gets eaten
    return true;
}

void Scrollbar::updateThumbPosition()
{
    theme()->invalidateParts(this, ForwardTrackPart | BackTrackPart | ThumbPart);
}

void Scrollbar::updateThumbProportion()
{
    theme()->invalidateParts(this, ForwardTrackPart | BackTrackPart | ThumbPart);
}

void Scrollbar::paint(GraphicsContext* context, const IntRect& damageRect)
{
    if (context->updatingControlTints() && theme()->supportsControlTints()) {
        invalidate();
        return;
    }
        
    if (context->paintingDisabled() || !frameRect().intersects(damageRect))
        return;

    if (!theme()->paint(this, context, damageRect))
        Widget::paint(context, damageRect);
}

void Scrollbar::autoscrollTimerFired(Timer<Scrollbar>*)
{
    autoscrollPressedPart(theme()->autoscrollTimerDelay());
}

static bool thumbUnderMouse(Scrollbar* scrollbar)
{
    int thumbPos = scrollbar->theme()->trackPosition(scrollbar) + scrollbar->theme()->thumbPosition(scrollbar);
    int thumbLength = scrollbar->theme()->thumbLength(scrollbar);
    return scrollbar->pressedPos() >= thumbPos && scrollbar->pressedPos() < thumbPos + thumbLength;
}

void Scrollbar::autoscrollPressedPart(double delay)
{
    // Don't do anything for the thumb or if nothing was pressed.
    if (m_pressedPart == ThumbPart || m_pressedPart == NoPart)
        return;

    // Handle the track.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse(this)) {
        theme()->invalidatePart(this, m_pressedPart);
        setHoveredPart(ThumbPart);
        return;
    }

    // Handle the arrows and track.
    if (scroll(pressedPartScrollDirection(), pressedPartScrollGranularity()))
        startTimerIfNeeded(delay);
}

void Scrollbar::startTimerIfNeeded(double delay)
{
    // Don't do anything for the thumb.
    if (m_pressedPart == ThumbPart)
        return;

    // Handle the track.  We halt track scrolling once the thumb is level
    // with us.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse(this)) {
        theme()->invalidatePart(this, m_pressedPart);
        setHoveredPart(ThumbPart);
        return;
    }

    // We can't scroll if we've hit the beginning or end.
    ScrollDirection dir = pressedPartScrollDirection();
    if (dir == ScrollUp || dir == ScrollLeft) {
        if (m_currentPos == 0)
            return;
    } else {
        if (m_currentPos == maximum())
            return;
    }

    m_scrollTimer.startOneShot(delay);
}

void Scrollbar::stopTimerIfNeeded()
{
    if (m_scrollTimer.isActive())
        m_scrollTimer.stop();
}

ScrollDirection Scrollbar::pressedPartScrollDirection()
{
    if (m_orientation == HorizontalScrollbar) {
        if (m_pressedPart == BackButtonStartPart || m_pressedPart == BackButtonEndPart || m_pressedPart == BackTrackPart)
            return ScrollLeft;
        return ScrollRight;
    } else {
        if (m_pressedPart == BackButtonStartPart || m_pressedPart == BackButtonEndPart || m_pressedPart == BackTrackPart)
            return ScrollUp;
        return ScrollDown;
    }
}

ScrollGranularity Scrollbar::pressedPartScrollGranularity()
{
    if (m_pressedPart == BackButtonStartPart || m_pressedPart == BackButtonEndPart ||  m_pressedPart == ForwardButtonStartPart || m_pressedPart == ForwardButtonEndPart)
        return ScrollByLine;
    return ScrollByPage;
}

void Scrollbar::moveThumb(int pos)
{
    // Drag the thumb.
    int thumbPos = theme()->thumbPosition(this);
    int thumbLen = theme()->thumbLength(this);
    int trackLen = theme()->trackLength(this);
    int maxPos = trackLen - thumbLen;
    int delta = pos - pressedPos();
    if (delta > 0)
        delta = min(maxPos - thumbPos, delta);
    else if (delta < 0)
        delta = max(-thumbPos, delta);
    if (delta) {
        setValue(static_cast<int>(static_cast<float>(thumbPos + delta) * maximum() / (trackLen - thumbLen)));
        setPressedPos(pressedPos() + theme()->thumbPosition(this) - thumbPos);
    }
}

void Scrollbar::setHoveredPart(ScrollbarPart part)
{
    if (part == m_hoveredPart)
        return;

    if ((m_hoveredPart == NoPart || part == NoPart) && theme()->invalidateOnMouseEnterExit())
        invalidate();  // Just invalidate the whole scrollbar, since the buttons at either end change anyway.
    else if (m_pressedPart == NoPart) {
        theme()->invalidatePart(this, part);
        theme()->invalidatePart(this, m_hoveredPart);
    }
    m_hoveredPart = part;
}

void Scrollbar::setPressedPart(ScrollbarPart part)
{
    if (m_pressedPart != NoPart)
        theme()->invalidatePart(this, m_pressedPart);
    m_pressedPart = part;
    if (m_pressedPart != NoPart)
        theme()->invalidatePart(this, m_pressedPart);
}


bool Scrollbar::mouseExited()
{
    setHoveredPart(NoPart);
    return true;
}

bool Scrollbar::mouseUp()
{
    setPressedPart(NoPart);
    m_pressedPos = 0;
    stopTimerIfNeeded();

    if (parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->frame()->eventHandler()->setMousePressed(false);

    return true;
}

bool Scrollbar::mouseDown(const PlatformMouseEvent& evt)
{
    // Early exit for right click
    if (evt.button() == RightButton)
        return true; // FIXME: Handled as context menu by Qt right now.  Should just avoid even calling this method on a right click though.

    setPressedPart(theme()->hitTest(this, evt));
    int pressedPos = (orientation() == HorizontalScrollbar ? convertFromContainingWindow(evt.pos()).x() : convertFromContainingWindow(evt.pos()).y());
    
    if ((pressedPart() == BackTrackPart || pressedPart() == ForwardTrackPart) && theme()->shouldCenterOnThumb(this, evt)) {
        setHoveredPart(ThumbPart);
        setPressedPart(ThumbPart);
        int thumbLen = theme()->thumbLength(this);
        int desiredPos = pressedPos;
        // Set the pressed position to the middle of the thumb so that when we do the move, the delta
        // will be from the current pixel position of the thumb to the new desired position for the thumb.
        m_pressedPos = theme()->trackPosition(this) + theme()->thumbPosition(this) + thumbLen / 2;
        moveThumb(desiredPos);
        return true;
    }
    
    m_pressedPos = pressedPos;

    autoscrollPressedPart(theme()->initialAutoscrollTimerDelay());
    return true;
}

void Scrollbar::setFrameRect(const IntRect& rect)
{
    // Get our window resizer rect and see if we overlap.  Adjust to avoid the overlap
    // if necessary.
    IntRect adjustedRect(rect);
    if (parent()) {
        bool overlapsResizer = false;
        ScrollView* view = parent();
        IntRect resizerRect = view->convertFromContainingWindow(view->windowResizerRect());
        if (rect.intersects(resizerRect)) {
            if (orientation() == HorizontalScrollbar) {
                int overlap = rect.right() - resizerRect.x();
                if (overlap > 0 && resizerRect.right() >= rect.right()) {
                    adjustedRect.setWidth(rect.width() - overlap);
                    overlapsResizer = true;
                }
            } else {
                int overlap = rect.bottom() - resizerRect.y();
                if (overlap > 0 && resizerRect.bottom() >= rect.bottom()) {
                    adjustedRect.setHeight(rect.height() - overlap);
                    overlapsResizer = true;
                }
            }
        }

        if (overlapsResizer != m_overlapsResizer) {
            m_overlapsResizer = overlapsResizer;
            view->adjustScrollbarsAvoidingResizerCount(m_overlapsResizer ? 1 : -1);
        }
    }

    Widget::setFrameRect(adjustedRect);
}

void Scrollbar::setParent(ScrollView* parentView)
{
    if (!parentView && m_overlapsResizer && parent())
        parent()->adjustScrollbarsAvoidingResizerCount(-1);
    Widget::setParent(parentView);
}

void Scrollbar::setEnabled(bool e)
{ 
    if (m_enabled == e)
        return;
    m_enabled = e;
    invalidate();
}

bool Scrollbar::isWindowActive() const
{
    return m_client && m_client->isActive();
}
 
void Scrollbar::invalidateRect(const IntRect& rect)
{
    if (suppressInvalidation())
        return;
    if (m_client)
        m_client->invalidateScrollbarRect(this, rect);
}

PlatformMouseEvent Scrollbar::transformEvent(const PlatformMouseEvent& event)
{
    return event;
}

}
