/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameLoaderClient.h"

#include "Color.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "Page.h"
#include "RenderPart.h"

namespace WebCore {

FrameLoaderClient::~FrameLoaderClient()
{}

void FrameLoaderClient::transitionToCommittedForNewPage(Frame* frame,
                                                        const IntSize& viewportSize,
                                                        const Color& backgroundColor, bool transparent,
                                                        const IntSize& fixedLayoutSize, bool useFixedLayout,
                                                        ScrollbarMode horizontalScrollbarMode, ScrollbarMode verticalScrollbarMode)
{
    ASSERT(frame);

    Page* page = frame->page();
    ASSERT(page);

    bool isMainFrame = frame == page->mainFrame();

    if (isMainFrame && frame->view())
        frame->view()->setParentVisible(false);

    frame->setView(0);

    FrameView* frameView;
    if (isMainFrame) {
        frameView = new FrameView(frame, viewportSize);
        frameView->setFixedLayoutSize(fixedLayoutSize);
        frameView->setUseFixedLayout(useFixedLayout);
    } else
        frameView = new FrameView(frame);

    frameView->setScrollbarModes(horizontalScrollbarMode, verticalScrollbarMode);
    frameView->updateDefaultScrollbarState();

    frame->setView(frameView);
    // FrameViews are created with a ref count of 1. Release this ref since we've assigned it to frame.
    frameView->deref();

    if (backgroundColor.isValid())
        frameView->updateBackgroundRecursively(backgroundColor, transparent);

    if (isMainFrame)
        frameView->setParentVisible(true);

    if (frame->ownerRenderer())
        frame->ownerRenderer()->setWidget(frameView);

    if (HTMLFrameOwnerElement* owner = frame->ownerElement())
        frame->view()->setCanHaveScrollbars(owner->scrollingMode() != ScrollbarAlwaysOff);
}

}

