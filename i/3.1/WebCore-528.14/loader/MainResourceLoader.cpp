/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "MainResourceLoader.h"

#include "DocumentLoader.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLFormElement.h"
#include "Page.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "Settings.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "ApplicationCache.h"
#include "ApplicationCacheGroup.h"
#include "ApplicationCacheResource.h"
#endif

// FIXME: More that is in common with SubresourceLoader should move up into ResourceLoader.

namespace WebCore {

MainResourceLoader::MainResourceLoader(Frame* frame)
    : ResourceLoader(frame, true, true)
    , m_dataLoadTimer(this, &MainResourceLoader::handleDataLoadNow)
    , m_loadingMultipartContent(false)
    , m_waitingForContentPolicy(false)
{
}

MainResourceLoader::~MainResourceLoader()
{
}

PassRefPtr<MainResourceLoader> MainResourceLoader::create(Frame* frame)
{
    return adoptRef(new MainResourceLoader(frame));
}

void MainResourceLoader::receivedError(const ResourceError& error)
{
    // Calling receivedMainResourceError will likely result in the last reference to this object to go away.
    RefPtr<MainResourceLoader> protect(this);
    RefPtr<Frame> protectFrame(m_frame);

    // It is important that we call FrameLoader::receivedMainResourceError before calling 
    // FrameLoader::didFailToLoad because receivedMainResourceError clears out the relevant
    // document loaders. Also, receivedMainResourceError ends up calling a FrameLoadDelegate method
    // and didFailToLoad calls a ResourceLoadDelegate method and they need to be in the correct order.
    frameLoader()->receivedMainResourceError(error, true);

    if (!cancelled()) {
        ASSERT(!reachedTerminalState());
        frameLoader()->didFailToLoad(this, error);
        
        releaseResources();
    }

    ASSERT(reachedTerminalState());
}

void MainResourceLoader::didCancel(const ResourceError& error)
{
    m_dataLoadTimer.stop();

    // Calling receivedMainResourceError will likely result in the last reference to this object to go away.
    RefPtr<MainResourceLoader> protect(this);

    if (m_waitingForContentPolicy) {
        frameLoader()->cancelContentPolicyCheck();
        ASSERT(m_waitingForContentPolicy);
        m_waitingForContentPolicy = false;
        deref(); // balances ref in didReceiveResponse
    }
    frameLoader()->receivedMainResourceError(error, true);
    ResourceLoader::didCancel(error);
}

ResourceError MainResourceLoader::interruptionForPolicyChangeError() const
{
    return frameLoader()->interruptionForPolicyChangeError(request());
}

void MainResourceLoader::stopLoadingForPolicyChange()
{
    cancel(interruptionForPolicyChangeError());
}

void MainResourceLoader::callContinueAfterNavigationPolicy(void* argument, const ResourceRequest& request, PassRefPtr<FormState>, bool shouldContinue)
{
    static_cast<MainResourceLoader*>(argument)->continueAfterNavigationPolicy(request, shouldContinue);
}

void MainResourceLoader::continueAfterNavigationPolicy(const ResourceRequest&, bool shouldContinue)
{
    if (!shouldContinue)
        stopLoadingForPolicyChange();
    deref(); // balances ref in willSendRequest
}

bool MainResourceLoader::isPostOrRedirectAfterPost(const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (newRequest.httpMethod() == "POST")
        return true;

    int status = redirectResponse.httpStatusCode();
    if (((status >= 301 && status <= 303) || status == 307)
        && frameLoader()->initialRequest().httpMethod() == "POST")
        return true;
    
    return false;
}

void MainResourceLoader::addData(const char* data, int length, bool allAtOnce)
{
    ResourceLoader::addData(data, length, allAtOnce);
    frameLoader()->receivedData(data, length);
}

void MainResourceLoader::willSendRequest(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(!newRequest.isNull());
    
    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);
    
    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if (frameLoader()->isLoadingMainFrame())
        newRequest.setMainDocumentURL(newRequest.url());
    
    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if (newRequest.cachePolicy() == UseProtocolCachePolicy && isPostOrRedirectAfterPost(newRequest, redirectResponse))
        newRequest.setCachePolicy(ReloadIgnoringCacheData);

    ResourceLoader::willSendRequest(newRequest, redirectResponse);
    
    // Don't set this on the first request. It is set when the main load was started.
    m_documentLoader->setRequest(newRequest);

    // FIXME: Ideally we'd stop the I/O until we hear back from the navigation policy delegate
    // listener. But there's no way to do that in practice. So instead we cancel later if the
    // listener tells us to. In practice that means the navigation policy needs to be decided
    // synchronously for these redirect cases.

    ref(); // balanced by deref in continueAfterNavigationPolicy
    frameLoader()->checkNavigationPolicy(newRequest, callContinueAfterNavigationPolicy, this);
}

static bool shouldLoadAsEmptyDocument(const KURL& url)
{
    return url.isEmpty() || equalIgnoringCase(String(url.protocol()), "about");
}

void MainResourceLoader::continueAfterContentPolicy(PolicyAction contentPolicy, const ResourceResponse& r)
{
    KURL url = request().url();
    const String& mimeType = r.mimeType();
    
    switch (contentPolicy) {
    case PolicyUse: {
        // Prevent remote web archives from loading because they can claim to be from any domain and thus avoid cross-domain security checks (4120255).
        bool isRemoteWebArchive = equalIgnoringCase("application/x-webarchive", mimeType) && !m_substituteData.isValid() && !url.isLocalFile();
        if (!frameLoader()->canShowMIMEType(mimeType) || isRemoteWebArchive) {
            frameLoader()->cannotShowMIMEType(r);
            // Check reachedTerminalState since the load may have already been cancelled inside of _handleUnimplementablePolicyWithErrorCode::.
            if (!reachedTerminalState())
                stopLoadingForPolicyChange();
            return;
        }
        break;
    }

    case PolicyDownload:
        // m_handle can be null, e.g. when loading a substitute resource from application cache.
        if (!m_handle) {
            receivedError(cannotShowURLError());
            return;
        }
        frameLoader()->client()->download(m_handle.get(), request(), m_handle.get()->request(), r);
        // It might have gone missing
        if (frameLoader())
            receivedError(interruptionForPolicyChangeError());
        return;

    case PolicyIgnore:
        stopLoadingForPolicyChange();
        return;
    
    default:
        ASSERT_NOT_REACHED();
    }

    RefPtr<MainResourceLoader> protect(this);

    if (r.isHTTP()) {
        int status = r.httpStatusCode();
        if (status < 200 || status >= 300) {
            bool hostedByObject = frameLoader()->isHostedByObjectElement();

            frameLoader()->handleFallbackContent();
            // object elements are no longer rendered after we fallback, so don't
            // keep trying to process data from their load

            if (hostedByObject)
                cancel();
        }
    }

    // we may have cancelled this load as part of switching to fallback content
    if (!reachedTerminalState())
        ResourceLoader::didReceiveResponse(r);

    if (frameLoader() && !frameLoader()->isStopping())
        if (m_substituteData.isValid()) {
            if (m_substituteData.content()->size())
                didReceiveData(m_substituteData.content()->data(), m_substituteData.content()->size(), m_substituteData.content()->size(), true);
            if (frameLoader() && !frameLoader()->isStopping()) 
                didFinishLoading();
        } else if (shouldLoadAsEmptyDocument(url) || frameLoader()->representationExistsForURLScheme(url.protocol()))
            didFinishLoading();
}

void MainResourceLoader::callContinueAfterContentPolicy(void* argument, PolicyAction policy)
{
    static_cast<MainResourceLoader*>(argument)->continueAfterContentPolicy(policy);
}

void MainResourceLoader::continueAfterContentPolicy(PolicyAction policy)
{
    ASSERT(m_waitingForContentPolicy);
    m_waitingForContentPolicy = false;
    if (frameLoader() && !frameLoader()->isStopping())
        continueAfterContentPolicy(policy, m_response);
    deref(); // balances ref in didReceiveResponse
}

void MainResourceLoader::didReceiveResponse(const ResourceResponse& r)
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (r.httpStatusCode() / 100 == 4 || r.httpStatusCode() / 100 == 5) {
        ASSERT(!m_applicationCache);
        if (m_frame->settings() && m_frame->settings()->offlineWebApplicationCacheEnabled()) {
            m_applicationCache = ApplicationCacheGroup::fallbackCacheForMainRequest(request(), documentLoader());

            if (scheduleLoadFallbackResourceFromApplicationCache(m_applicationCache.get()))
                return;
        }
    }
#endif

    HTTPHeaderMap::const_iterator it = r.httpHeaderFields().find(AtomicString("x-frame-options"));
    if (it != r.httpHeaderFields().end()) {
        String content = it->second;
        if (m_frame->loader()->shouldInterruptLoadForXFrameOptions(content, r.url())) {
            cancel();
            return;
        }
    }

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !PLATFORM(CF)
    ASSERT(shouldLoadAsEmptyDocument(r.url()) || !defersLoading());
#endif

    if (m_loadingMultipartContent) {
        frameLoader()->setupForReplaceByMIMEType(r.mimeType());
        clearResourceData();
    }
    
    if (r.isMultipart())
        m_loadingMultipartContent = true;
        
    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    m_documentLoader->setResponse(r);

    m_response = r;

    ASSERT(!m_waitingForContentPolicy);
    m_waitingForContentPolicy = true;
    ref(); // balanced by deref in continueAfterContentPolicy and didCancel
    frameLoader()->checkContentPolicy(m_response.mimeType(), callContinueAfterContentPolicy, this);
}

void MainResourceLoader::didReceiveData(const char* data, int length, long long lengthReceived, bool allAtOnce)
{
    ASSERT(data);
    ASSERT(length != 0);

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !PLATFORM(CF)
    ASSERT(!defersLoading());
#endif
 
    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    ResourceLoader::didReceiveData(data, length, lengthReceived, allAtOnce);
}

void MainResourceLoader::didFinishLoading()
{
    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !PLATFORM(CF)
    ASSERT(shouldLoadAsEmptyDocument(frameLoader()->activeDocumentLoader()->url()) || !defersLoading());
#endif
    
    // The additional processing can do anything including possibly removing the last
    // reference to this object.
    RefPtr<MainResourceLoader> protect(this);
    
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    RefPtr<DocumentLoader> dl = documentLoader();
#endif

    frameLoader()->finishedLoading();
    ResourceLoader::didFinishLoading();
    
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    ApplicationCacheGroup* group = dl->candidateApplicationCacheGroup();
    if (!group && dl->applicationCache() && !dl->mainResourceApplicationCache())
        group = dl->applicationCache()->group();
    
    if (group)
        group->finishedLoadingMainResource(dl.get());
#endif
}

void MainResourceLoader::didFail(const ResourceError& error)
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (!error.isCancellation()) {
        ASSERT(!m_applicationCache);
        if (m_frame->settings() && m_frame->settings()->offlineWebApplicationCacheEnabled()) {
            m_applicationCache = ApplicationCacheGroup::fallbackCacheForMainRequest(request(), documentLoader());

            if (scheduleLoadFallbackResourceFromApplicationCache(m_applicationCache.get()))
                return;
        }
    }
#endif

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !PLATFORM(CF)
    ASSERT(!defersLoading());
#endif
    
    receivedError(error);
}

void MainResourceLoader::handleEmptyLoad(const KURL& url, bool forURLScheme)
{
    String mimeType;
    if (forURLScheme)
        mimeType = frameLoader()->generatedMIMETypeForURLScheme(url.protocol());
    else
        mimeType = "text/html";
    
    ResourceResponse response(url, mimeType, 0, String(), String());
    didReceiveResponse(response);
}

void MainResourceLoader::handleDataLoadNow(Timer<MainResourceLoader>*)
{
    RefPtr<MainResourceLoader> protect(this);

    KURL url = m_substituteData.responseURL();
    if (url.isEmpty())
        url = m_initialRequest.url();
        
    ResourceResponse response(url, m_substituteData.mimeType(), m_substituteData.content()->size(), m_substituteData.textEncoding(), "");
    didReceiveResponse(response);
}

void MainResourceLoader::handleDataLoadSoon(ResourceRequest& r)
{
    if (frameLoader()->loadsSynchronously()) {
        handleDataLoadNow(0);
        return;
    }

    m_initialRequest = r;
    
    if (m_documentLoader->deferMainResourceDataLoad())
        m_dataLoadTimer.startOneShot(0);
    else
        handleDataLoadNow(0);
}

bool MainResourceLoader::loadNow(ResourceRequest& r)
{
    bool shouldLoadEmptyBeforeRedirect = shouldLoadAsEmptyDocument(r.url());

    ASSERT(!m_handle);
    ASSERT(shouldLoadEmptyBeforeRedirect || !defersLoading());

    // Send this synthetic delegate callback since clients expect it, and
    // we no longer send the callback from within NSURLConnection for
    // initial requests.
    willSendRequest(r, ResourceResponse());

    // <rdar://problem/4801066>
    // willSendRequest() is liable to make the call to frameLoader() return NULL, so we need to check that here
    if (!frameLoader())
        return false;
    
    const KURL& url = r.url();
    bool shouldLoadEmpty = shouldLoadAsEmptyDocument(url) && !m_substituteData.isValid();

    if (shouldLoadEmptyBeforeRedirect && !shouldLoadEmpty && defersLoading())
        return true;

    if (m_substituteData.isValid()) 
        handleDataLoadSoon(r);
    else if (shouldLoadEmpty || frameLoader()->representationExistsForURLScheme(url.protocol()))
        handleEmptyLoad(url, !shouldLoadEmpty);
    else
        m_handle = ResourceHandle::create(r, this, m_frame.get(), false, true, true);

    return false;
}

bool MainResourceLoader::load(const ResourceRequest& r, const SubstituteData& substituteData)
{
    ASSERT(!m_handle);

    m_substituteData = substituteData;
    
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    // Check if this request should be loaded from the application cache
    if (!m_substituteData.isValid() && frameLoader()->frame()->settings() && frameLoader()->frame()->settings()->offlineWebApplicationCacheEnabled()) {
        ASSERT(!m_applicationCache);

        m_applicationCache = ApplicationCacheGroup::cacheForMainRequest(r, m_documentLoader.get());

        if (m_applicationCache) {
            // Get the resource from the application cache. By definition, cacheForMainRequest() returns a cache that contains the resource.
            ApplicationCacheResource* resource = m_applicationCache->resourceForRequest(r);
            m_substituteData = SubstituteData(resource->data(), 
                                              resource->response().mimeType(),
                                              resource->response().textEncodingName(), KURL());
        }
    }
#endif

    ResourceRequest request(r);
    bool defer = defersLoading();
    if (defer) {
        bool shouldLoadEmpty = shouldLoadAsEmptyDocument(r.url());
        if (shouldLoadEmpty)
            defer = false;
    }
    if (!defer) {
        if (loadNow(request)) {
            // Started as an empty document, but was redirected to something non-empty.
            ASSERT(defersLoading());
            defer = true;
        }
    }
    if (defer)
        m_initialRequest = request;

    return true;
}

void MainResourceLoader::setDefersLoading(bool defers)
{
    ResourceLoader::setDefersLoading(defers);
    
    if (defers) {
        if (m_dataLoadTimer.isActive())
            m_dataLoadTimer.stop();
    } else {
        if (m_initialRequest.isNull())
            return;
        
        if (m_substituteData.isValid() &&
            m_documentLoader->deferMainResourceDataLoad())
                m_dataLoadTimer.startOneShot(0);
        else {
            ResourceRequest r(m_initialRequest);
            m_initialRequest = ResourceRequest();
            loadNow(r);
        }
    }
}

}
