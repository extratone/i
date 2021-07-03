/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PageViewportControllerClientEfl.h"

#include "EwkView.h"
#include <WebCore/FloatPoint.h>

using namespace WebCore;

namespace WebKit {

PageViewportControllerClientEfl::PageViewportControllerClientEfl(EwkView* view)
    : m_view(view)
{
    ASSERT(m_view);
}

void PageViewportControllerClientEfl::didChangeContentsSize(const WebCore::IntSize&)
{
    m_view->scheduleUpdateDisplay();
}

void PageViewportControllerClientEfl::setViewportPosition(const WebCore::FloatPoint& contentsPosition)
{
    m_view->setViewportPosition(contentsPosition);
}

void PageViewportControllerClientEfl::setPageScaleFactor(float newScale)
{
    WKViewSetContentScaleFactor(m_view->wkView(), newScale);
}

void PageViewportControllerClientEfl::didChangeVisibleContents()
{
    m_view->scheduleUpdateDisplay();
}

void PageViewportControllerClientEfl::didChangeViewportAttributes()
{
}

} // namespace WebKit
