/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebChromeClient.h"

#if PLATFORM(IOS)

#import "DrawingArea.h"
#import "WebCoreArgumentCoders.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/NotImplemented.h>

namespace WebKit {

#if ENABLE(IOS_TOUCH_EVENTS)
void WebChromeClient::didPreventDefaultForEvent()
{
    notImplemented();
}
#endif

void WebChromeClient::elementDidFocus(const WebCore::Node* node)
{
    m_page->elementDidFocus(const_cast<WebCore::Node*>(node));
}

void WebChromeClient::elementDidBlur(const WebCore::Node* node)
{
    m_page->elementDidBlur(const_cast<WebCore::Node*>(node));
}

void WebChromeClient::elementDidRefocus(const WebCore::Node* node)
{
    elementDidFocus(node);
}

void WebChromeClient::didReceiveMobileDocType(bool isMobileDoctype)
{
    m_page->didReceiveMobileDocType(isMobileDoctype);
}

void WebChromeClient::setNeedsScrollNotifications(WebCore::Frame*, bool)
{
    notImplemented();
}

void WebChromeClient::observedContentChange(WebCore::Frame*)
{
    m_page->completePendingSyntheticClickForContentChangeObserver();
}

void WebChromeClient::clearContentChangeObservers(WebCore::Frame*)
{
    notImplemented();
}

void WebChromeClient::notifyRevealedSelectionByScrollingFrame(WebCore::Frame*)
{
    m_page->didChangeSelection();
}

bool WebChromeClient::isStopping()
{
    notImplemented();
    return false;
}

void WebChromeClient::didLayout(LayoutType type)
{
    if (type == Scroll)
        m_page->didChangeSelection();
}

void WebChromeClient::didStartOverflowScroll()
{
    m_page->send(Messages::WebPageProxy::OverflowScrollWillStartScroll());
}

void WebChromeClient::didEndOverflowScroll()
{
    m_page->send(Messages::WebPageProxy::OverflowScrollDidEndScroll());
}

bool WebChromeClient::hasStablePageScaleFactor() const
{
    return m_page->hasStablePageScaleFactor();
}

void WebChromeClient::suppressFormNotifications()
{
    notImplemented();
}

void WebChromeClient::restoreFormNotifications()
{
    notImplemented();
}

void WebChromeClient::addOrUpdateScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*, const WebCore::IntSize&, bool, bool)
{
    notImplemented();
}

void WebChromeClient::removeScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*)
{
    notImplemented();
}

void WebChromeClient::webAppOrientationsUpdated()
{
    notImplemented();
}

void WebChromeClient::showPlaybackTargetPicker(bool hasVideo)
{
    m_page->send(Messages::WebPageProxy::ShowPlaybackTargetPicker(hasVideo, m_page->rectForElementAtInteractionLocation()));
}

std::chrono::milliseconds WebChromeClient::eventThrottlingDelay()
{
    return m_page->eventThrottlingDelay();
}

int WebChromeClient::deviceOrientation() const
{
    return m_page->deviceOrientation();
}

} // namespace WebKit

#endif // PLATFORM(IOS)
