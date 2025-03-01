/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#import "Frame.h"

#import "Document.h"
#import "FrameLoaderClient.h"
#import "FrameSelection.h"
#import "FrameSnapshottingMac.h"
#import "FrameView.h"
#import "RenderObject.h"

#if PLATFORM(IOS)
#import "EventListener.h"
#import <wtf/GetPtr.h>
#endif // PLATFORM(IOS)

namespace WebCore {

#if !PLATFORM(IOS)
DragImageRef Frame::nodeImage(Node* node)
{
    m_doc->updateLayout(); // forces style recalc

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nil;
    LayoutRect topLevelRect;
    NSRect paintingRect = pixelSnappedIntRect(renderer->paintingRootRect(topLevelRect));

    m_view->setNodeToDraw(node); // invoke special sub-tree drawing mode
    NSImage* result = imageFromRect(this, paintingRect);
    m_view->setNodeToDraw(0);

    return result;
}
#endif // !PLATFORM(IOS)

DragImageRef Frame::dragImageForSelection()
{
#if !PLATFORM(IOS)
    if (!selection()->isRange())
        return nil;
    return selectionImage(this);
#else
    return nil;
#endif
}

#if PLATFORM(IOS)
const ViewportArguments& Frame::viewportArguments() const
{
    return m_viewportArguments;
}

void Frame::setViewportArguments(const ViewportArguments& arguments)
{
    m_viewportArguments = arguments;
}
#endif

} // namespace WebCore
