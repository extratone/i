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

#ifndef ScrollBar_h
#define ScrollBar_h

#include "Widget.h"

#ifdef __OBJC__
@class NSScroller;
#else
class NSScroller;
typedef int NSScrollerPart;
#endif

namespace WebCore {

enum ScrollDirection {
    ScrollUp,
    ScrollDown,
    ScrollLeft,
    ScrollRight
};

enum ScrollGranularity {
    ScrollByLine,
    ScrollByPage,
    ScrollByDocument,
    ScrollByWheel
};


// COPIED FROM NSScroller.h

enum {
    NSScrollerNoPart			= 0,
    NSScrollerDecrementPage		= 1,
    NSScrollerKnob			= 2,
    NSScrollerIncrementPage		= 3,
    NSScrollerDecrementLine    		= 4,
    NSScrollerIncrementLine	 	= 5,
    NSScrollerKnobSlot			= 6
};

typedef unsigned int NSScrollerPart;


enum ScrollBarOrientation { HorizontalScrollBar, VerticalScrollBar };

class ScrollBar : public Widget {
public:
    ScrollBar(ScrollBarOrientation);
    virtual ~ScrollBar();

    ScrollBarOrientation orientation() { return m_orientation; }

    int value() { return m_currentPos; }
    bool setValue(int v);

    void setSteps(int lineStep, int pageStep);
    void setKnobProportion(int visibleSize, int totalSize);
    
    bool scrollbarHit(NSScrollerPart);
    void valueChanged();
    
    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1.0);
    
private:
    ScrollBarOrientation m_orientation;
    int m_visibleSize;
    int m_totalSize;
    int m_currentPos;
    int m_lineStep;
    int m_pageStep;
};

}

#endif
