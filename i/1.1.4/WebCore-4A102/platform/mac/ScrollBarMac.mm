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
#import "ScrollBar.h"

#import "WidgetClient.h"

using namespace WebCore;

ScrollBar::ScrollBar(ScrollBarOrientation orientation)
    : m_orientation(orientation)
    , m_visibleSize(0)
    , m_totalSize(0)
    , m_currentPos(0)
    , m_lineStep(0)
    , m_pageStep(0)
{
    // Not implemented.
}

ScrollBar::~ScrollBar()
{
    // Not implemented.
}

bool ScrollBar::setValue(int v)
{
    if (v < 0)
        v = 0;

    int maxPos = m_totalSize - m_visibleSize;
    if (v > maxPos)
        v = maxPos;

    if (m_currentPos == v)
        return false; // Our value stayed the same.

    m_currentPos = v;

    // Not implemented.

    valueChanged();
    
    return true;
}

void ScrollBar::setSteps(int lineStep, int pageStep)
{
    m_lineStep = 1;
    m_pageStep = 1;

    // Not implemented.
}

void ScrollBar::setKnobProportion(int visibleArea, int totalArea)
{
    m_visibleSize = visibleArea;
    m_totalSize = totalArea;

    // Not implemented.
    //float val = (float)m_visibleSize/m_totalSize;
}

void ScrollBar::valueChanged()
{
    if (client())
        client()->valueChanged(this);
}

bool ScrollBar::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    float delta = 0.0;
    if ((direction == ScrollUp && m_orientation == VerticalScrollBar) || (direction == ScrollLeft && m_orientation == HorizontalScrollBar)) {
        if (granularity == ScrollByLine) {
            delta = -m_lineStep;
        } else if (granularity == ScrollByPage) {
            delta = -m_pageStep;
        } else if (granularity == ScrollByDocument) {
            delta = -m_currentPos;
        } else if (granularity == ScrollByWheel) {
            delta = -m_lineStep;
        }
    } else if ((direction == ScrollDown && m_orientation == VerticalScrollBar) || (direction == ScrollRight && m_orientation == HorizontalScrollBar)) {
        if (granularity == ScrollByLine) {
            delta = m_lineStep;
        } else if (granularity == ScrollByPage) {
            delta = m_pageStep;
        } else if (granularity == ScrollByDocument) {
            delta = m_totalSize - m_visibleSize - m_currentPos;
        } else if (granularity == ScrollByWheel) {
            delta = m_lineStep;
        }
    }

    int newPos = (int)(m_currentPos + (delta * multiplier));
    return setValue(newPos);
}

