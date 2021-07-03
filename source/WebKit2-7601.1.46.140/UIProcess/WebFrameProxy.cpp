/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "WebFrameProxy.h"

#include "WebCertificateInfo.h"
#include "WebFormSubmissionListenerProxy.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <WebCore/DOMImplementation.h>
#include <WebCore/Image.h>
#include <WebCore/MIMETypeRegistry.h>
#include <stdio.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

WebFrameProxy::WebFrameProxy(WebPageProxy* page, uint64_t frameID)
    : m_page(page)
    , m_isFrameSet(false)
    , m_frameID(frameID)
{
    WebProcessPool::statistics().wkFrameCount++;
}

WebFrameProxy::~WebFrameProxy()
{
    WebProcessPool::statistics().wkFrameCount--;
}

void WebFrameProxy::webProcessWillShutDown()
{
    m_page = nullptr;

    if (m_activeListener) {
        m_activeListener->invalidate();
        m_activeListener = nullptr;
    }
}

bool WebFrameProxy::isMainFrame() const
{
    if (!m_page)
        return false;

    return this == m_page->mainFrame();
}

void WebFrameProxy::loadURL(const String& url)
{
    if (!m_page)
        return;

    m_page->process().send(Messages::WebPage::LoadURLInFrame(url, m_frameID), m_page->pageID());
}

void WebFrameProxy::stopLoading() const
{
    if (!m_page)
        return;

    if (!m_page->isValid())
        return;

    m_page->process().send(Messages::WebPage::StopLoadingFrame(m_frameID), m_page->pageID());
}
    
bool WebFrameProxy::canProvideSource() const
{
    return isDisplayingMarkupDocument();
}

bool WebFrameProxy::canShowMIMEType(const String& mimeType) const
{
    if (!m_page)
        return false;

    return m_page->canShowMIMEType(mimeType);
}

bool WebFrameProxy::isDisplayingStandaloneImageDocument() const
{
    return Image::supportsType(m_MIMEType);
}

bool WebFrameProxy::isDisplayingStandaloneMediaDocument() const
{
    return MIMETypeRegistry::isSupportedMediaMIMEType(m_MIMEType);
}

bool WebFrameProxy::isDisplayingMarkupDocument() const
{
    // FIXME: This check should be moved to somewhere in WebCore.
    return m_MIMEType == "text/html" || m_MIMEType == "image/svg+xml" || m_MIMEType == "application/x-webarchive" || DOMImplementation::isXMLMIMEType(m_MIMEType);
}

bool WebFrameProxy::isDisplayingPDFDocument() const
{
    if (m_MIMEType.isEmpty())
        return false;

    return MIMETypeRegistry::isPDFOrPostScriptMIMEType(m_MIMEType);
}

void WebFrameProxy::didStartProvisionalLoad(const String& url)
{
    m_frameLoadState.didStartProvisionalLoad(url);
}

void WebFrameProxy::didReceiveServerRedirectForProvisionalLoad(const String& url)
{
    m_frameLoadState.didReceiveServerRedirectForProvisionalLoad(url);
}

void WebFrameProxy::didFailProvisionalLoad()
{
    m_frameLoadState.didFailProvisionalLoad();
}

void WebFrameProxy::didCommitLoad(const String& contentType, WebCertificateInfo& certificateInfo)
{
    m_frameLoadState.didCommitLoad();

    m_title = String();
    m_MIMEType = contentType;
    m_isFrameSet = false;
    m_certificateInfo = &certificateInfo;
}

void WebFrameProxy::didFinishLoad()
{
    m_frameLoadState.didFinishLoad();
}

void WebFrameProxy::didFailLoad()
{
    m_frameLoadState.didFailLoad();
}

void WebFrameProxy::didSameDocumentNavigation(const String& url)
{
    m_frameLoadState.didSameDocumentNotification(url);
}

void WebFrameProxy::didChangeTitle(const String& title)
{
    m_title = title;
}

void WebFrameProxy::receivedPolicyDecision(WebCore::PolicyAction action, uint64_t listenerID, API::Navigation* navigation)
{
    if (!m_page)
        return;

    ASSERT(m_activeListener);
    ASSERT(m_activeListener->listenerID() == listenerID);
    m_page->receivedPolicyDecision(action, this, listenerID, navigation);
}

WebFramePolicyListenerProxy& WebFrameProxy::setUpPolicyListenerProxy(uint64_t listenerID)
{
    if (m_activeListener)
        m_activeListener->invalidate();
    m_activeListener = WebFramePolicyListenerProxy::create(this, listenerID);
    return *static_cast<WebFramePolicyListenerProxy*>(m_activeListener.get());
}

WebFormSubmissionListenerProxy& WebFrameProxy::setUpFormSubmissionListenerProxy(uint64_t listenerID)
{
    if (m_activeListener)
        m_activeListener->invalidate();
    m_activeListener = WebFormSubmissionListenerProxy::create(this, listenerID);
    return *static_cast<WebFormSubmissionListenerProxy*>(m_activeListener.get());
}

void WebFrameProxy::getWebArchive(std::function<void (API::Data*, CallbackBase::Error)> callbackFunction)
{
    if (!m_page) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    m_page->getWebArchiveOfFrame(this, callbackFunction);
}

void WebFrameProxy::getMainResourceData(std::function<void (API::Data*, CallbackBase::Error)> callbackFunction)
{
    if (!m_page) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    m_page->getMainResourceDataOfFrame(this, callbackFunction);
}

void WebFrameProxy::getResourceData(API::URL* resourceURL, std::function<void (API::Data*, CallbackBase::Error)> callbackFunction)
{
    if (!m_page) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    m_page->getResourceDataFromFrame(this, resourceURL, callbackFunction);
}

void WebFrameProxy::setUnreachableURL(const String& unreachableURL)
{
    m_frameLoadState.setUnreachableURL(unreachableURL);
}

#if ENABLE(CONTENT_FILTERING)
bool WebFrameProxy::didHandleContentFilterUnblockNavigation(const WebCore::ResourceRequest& request)
{
    if (!m_contentFilterUnblockHandler.canHandleRequest(request)) {
        m_contentFilterUnblockHandler = { };
        return false;
    }

    RefPtr<WebPageProxy> page { m_page };
    ASSERT(page);
    m_contentFilterUnblockHandler.requestUnblockAsync([page](bool unblocked) {
        if (unblocked) {
            const bool reloadFromOrigin = false;
            const bool contentBlockersEnabled = true;
            page->reload(reloadFromOrigin, contentBlockersEnabled);
        }
    });
    return true;
}
#endif

} // namespace WebKit
