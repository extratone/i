/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "NavigationState.h"

#if WK_API_ENABLED

#import "APIFrameInfo.h"
#import "APINavigationData.h"
#import "APINavigationResponse.h"
#import "APIString.h"
#import "APIURL.h"
#import "AuthenticationDecisionListener.h"
#import "CompletionHandlerCallChecker.h"
#import "NavigationActionData.h"
#import "PageLoadState.h"
#import "SecurityOriginData.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKHistoryDelegatePrivate.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKNSURLExtras.h"
#import "WKNSURLRequest.h"
#import "WKNavigationActionInternal.h"
#import "WKNavigationDataInternal.h"
#import "WKNavigationDelegatePrivate.h"
#import "WKNavigationInternal.h"
#import "WKNavigationResponseInternal.h"
#import "WKReloadFrameErrorRecoveryAttempter.h"
#import "WKWebViewInternal.h"
#import "WebCredential.h"
#import "WebFrameProxy.h"
#import "WebNavigationState.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "WebProtectionSpace.h"
#import "_WKErrorRecoveryAttempting.h"
#import "_WKFrameHandleInternal.h"
#import "_WKRenderingProgressEventsInternal.h"
#import "_WKSameDocumentNavigationTypeInternal.h"
#import <WebCore/Credential.h>
#import <WebCore/URL.h>
#import <wtf/NeverDestroyed.h>

#if HAVE(APP_LINKS)
#import <WebCore/LaunchServicesSPI.h>
#endif

#if USE(QUICK_LOOK)
#import "QuickLookDocumentData.h"
#endif

using namespace WebCore;

namespace WebKit {

static HashMap<WebPageProxy*, NavigationState*>& navigationStates()
{
    static NeverDestroyed<HashMap<WebPageProxy*, NavigationState*>> navigationStates;

    return navigationStates;
}

NavigationState::NavigationState(WKWebView *webView)
    : m_webView(webView)
    , m_navigationDelegateMethods()
    , m_historyDelegateMethods()
{
    ASSERT(m_webView->_page);
    ASSERT(!navigationStates().contains(m_webView->_page.get()));

    navigationStates().add(m_webView->_page.get(), this);
    m_webView->_page->pageLoadState().addObserver(*this);
}

NavigationState::~NavigationState()
{
    ASSERT(navigationStates().get(m_webView->_page.get()) == this);

    navigationStates().remove(m_webView->_page.get());
    m_webView->_page->pageLoadState().removeObserver(*this);
}

NavigationState& NavigationState::fromWebPage(WebPageProxy& webPageProxy)
{
    ASSERT(navigationStates().contains(&webPageProxy));

    return *navigationStates().get(&webPageProxy);
}

std::unique_ptr<API::NavigationClient> NavigationState::createNavigationClient()
{
    return std::make_unique<NavigationClient>(*this);
}
    
std::unique_ptr<API::HistoryClient> NavigationState::createHistoryClient()
{
    return std::make_unique<HistoryClient>(*this);
}

RetainPtr<id <WKNavigationDelegate> > NavigationState::navigationDelegate()
{
    return m_navigationDelegate.get();
}

void NavigationState::setNavigationDelegate(id <WKNavigationDelegate> delegate)
{
    m_navigationDelegate = delegate;

    m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandler = [delegate respondsToSelector:@selector(webView:decidePolicyForNavigationAction:decisionHandler:)];
    m_navigationDelegateMethods.webViewDecidePolicyForNavigationResponseDecisionHandler = [delegate respondsToSelector:@selector(webView:decidePolicyForNavigationResponse:decisionHandler:)];

    m_navigationDelegateMethods.webViewDidStartProvisionalNavigation = [delegate respondsToSelector:@selector(webView:didStartProvisionalNavigation:)];
    m_navigationDelegateMethods.webViewDidReceiveServerRedirectForProvisionalNavigation = [delegate respondsToSelector:@selector(webView:didReceiveServerRedirectForProvisionalNavigation:)];
    m_navigationDelegateMethods.webViewDidFailProvisionalNavigationWithError = [delegate respondsToSelector:@selector(webView:didFailProvisionalNavigation:withError:)];
    m_navigationDelegateMethods.webViewDidCommitNavigation = [delegate respondsToSelector:@selector(webView:didCommitNavigation:)];
    m_navigationDelegateMethods.webViewDidFinishNavigation = [delegate respondsToSelector:@selector(webView:didFinishNavigation:)];
    m_navigationDelegateMethods.webViewDidFailNavigationWithError = [delegate respondsToSelector:@selector(webView:didFailNavigation:withError:)];

    m_navigationDelegateMethods.webViewNavigationDidFailProvisionalLoadInSubframeWithError = [delegate respondsToSelector:@selector(_webView:navigation:didFailProvisionalLoadInSubframe:withError:)];
    m_navigationDelegateMethods.webViewNavigationDidFinishDocumentLoad = [delegate respondsToSelector:@selector(_webView:navigationDidFinishDocumentLoad:)];
    m_navigationDelegateMethods.webViewNavigationDidSameDocumentNavigation = [delegate respondsToSelector:@selector(_webView:navigation:didSameDocumentNavigation:)];
    m_navigationDelegateMethods.webViewRenderingProgressDidChange = [delegate respondsToSelector:@selector(_webView:renderingProgressDidChange:)];
    m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallengeCompletionHandler = [delegate respondsToSelector:@selector(webView:didReceiveAuthenticationChallenge:completionHandler:)];
    m_navigationDelegateMethods.webViewWebContentProcessDidTerminate = [delegate respondsToSelector:@selector(webViewWebContentProcessDidTerminate:)];
    m_navigationDelegateMethods.webViewCanAuthenticateAgainstProtectionSpace = [delegate respondsToSelector:@selector(_webView:canAuthenticateAgainstProtectionSpace:)];
    m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallenge = [delegate respondsToSelector:@selector(_webView:didReceiveAuthenticationChallenge:)];
    m_navigationDelegateMethods.webViewWebProcessDidCrash = [delegate respondsToSelector:@selector(_webViewWebProcessDidCrash:)];
    m_navigationDelegateMethods.webViewWebProcessDidBecomeResponsive = [delegate respondsToSelector:@selector(_webViewWebProcessDidBecomeResponsive:)];
    m_navigationDelegateMethods.webViewWebProcessDidBecomeUnresponsive = [delegate respondsToSelector:@selector(_webViewWebProcessDidBecomeUnresponsive:)];
    m_navigationDelegateMethods.webCryptoMasterKeyForWebView = [delegate respondsToSelector:@selector(_webCryptoMasterKeyForWebView:)];
    m_navigationDelegateMethods.webViewDidBeginNavigationGesture = [delegate respondsToSelector:@selector(_webViewDidBeginNavigationGesture:)];
    m_navigationDelegateMethods.webViewWillEndNavigationGestureWithNavigationToBackForwardListItem = [delegate respondsToSelector:@selector(_webViewWillEndNavigationGesture:withNavigationToBackForwardListItem:)];
    m_navigationDelegateMethods.webViewDidEndNavigationGestureWithNavigationToBackForwardListItem = [delegate respondsToSelector:@selector(_webViewDidEndNavigationGesture:withNavigationToBackForwardListItem:)];
    m_navigationDelegateMethods.webViewWillSnapshotBackForwardListItem = [delegate respondsToSelector:@selector(_webView:willSnapshotBackForwardListItem:)];
#if USE(QUICK_LOOK)
    m_navigationDelegateMethods.webViewDidStartLoadForQuickLookDocumentInMainFrame = [delegate respondsToSelector:@selector(_webView:didStartLoadForQuickLookDocumentInMainFrameWithFileName:uti:)];
    m_navigationDelegateMethods.webViewDidFinishLoadForQuickLookDocumentInMainFrame = [delegate respondsToSelector:@selector(_webView:didFinishLoadForQuickLookDocumentInMainFrame:)];
#endif
}

RetainPtr<id <WKHistoryDelegatePrivate> > NavigationState::historyDelegate()
{
    return m_historyDelegate.get();
}

void NavigationState::setHistoryDelegate(id <WKHistoryDelegatePrivate> historyDelegate)
{
    m_historyDelegate = historyDelegate;

    m_historyDelegateMethods.webViewDidNavigateWithNavigationData = [historyDelegate respondsToSelector:@selector(_webView:didNavigateWithNavigationData:)];
    m_historyDelegateMethods.webViewDidPerformClientRedirectFromURLToURL = [historyDelegate respondsToSelector:@selector(_webView:didPerformClientRedirectFromURL:toURL:)];
    m_historyDelegateMethods.webViewDidPerformServerRedirectFromURLToURL = [historyDelegate respondsToSelector:@selector(_webView:didPerformServerRedirectFromURL:toURL:)];
    m_historyDelegateMethods.webViewDidUpdateHistoryTitleForURL = [historyDelegate respondsToSelector:@selector(_webView:didUpdateHistoryTitle:forURL:)];
}

void NavigationState::navigationGestureDidBegin()
{
    if (!m_navigationDelegateMethods.webViewDidBeginNavigationGesture)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidBeginNavigationGesture:m_webView];
}

void NavigationState::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (!m_navigationDelegateMethods.webViewWillEndNavigationGestureWithNavigationToBackForwardListItem)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webViewWillEndNavigationGesture:m_webView withNavigationToBackForwardListItem:willNavigate ? wrapper(item) : nil];
}

void NavigationState::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (!m_navigationDelegateMethods.webViewDidEndNavigationGestureWithNavigationToBackForwardListItem)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidEndNavigationGesture:m_webView withNavigationToBackForwardListItem:willNavigate ? wrapper(item) : nil];
}

void NavigationState::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    if (!m_navigationDelegateMethods.webViewWillSnapshotBackForwardListItem)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_webView willSnapshotBackForwardListItem:wrapper(item)];
}

void NavigationState::didFirstPaint()
{
    if (!m_navigationDelegateMethods.webViewRenderingProgressDidChange)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_webView renderingProgressDidChange:_WKRenderingProgressEventFirstPaint];
}

NavigationState::NavigationClient::NavigationClient(NavigationState& navigationState)
    : m_navigationState(navigationState)
{
}

NavigationState::NavigationClient::~NavigationClient()
{
}

static void tryAppLink(RefPtr<API::NavigationAction> navigationAction, const String& currentMainFrameURL, std::function<void (bool)> completionHandler)
{
#if HAVE(APP_LINKS)
    if (!navigationAction->shouldOpenAppLinks()) {
        completionHandler(false);
        return;
    }

    auto* localCompletionHandler = new std::function<void (bool)>(WTF::move(completionHandler));
    [LSAppLink openWithURL:navigationAction->request().url() completionHandler:[localCompletionHandler](BOOL success, NSError *) {
        dispatch_async(dispatch_get_main_queue(), [localCompletionHandler, success] {
            (*localCompletionHandler)(success);
            delete localCompletionHandler;
        });
    }];
#else
    completionHandler(false);
#endif
}

void NavigationState::NavigationClient::decidePolicyForNavigationAction(WebPageProxy& webPageProxy, API::NavigationAction& navigationAction, Ref<WebFramePolicyListenerProxy>&& listener, API::Object* userData)
{
    String mainFrameURLString = webPageProxy.mainFrame()->url();

    if (!m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandler) {
        RefPtr<API::NavigationAction> localNavigationAction = &navigationAction;
        RefPtr<WebFramePolicyListenerProxy> localListener = WTF::move(listener);

        tryAppLink(localNavigationAction, mainFrameURLString, [localListener, localNavigationAction] (bool followedLinkToApp) {
            if (followedLinkToApp) {
                localListener->ignore();
                return;
            }

            if (!localNavigationAction->targetFrame()) {
                localListener->use();
                return;
            }

            RetainPtr<NSURLRequest> nsURLRequest = adoptNS(wrapper(API::URLRequest::create(localNavigationAction->request()).leakRef()));
            if ([NSURLConnection canHandleRequest:nsURLRequest.get()]) {
                localListener->use();
                return;
            }

#if PLATFORM(MAC)
            // A file URL shouldn't fall through to here, but if it did,
            // it would be a security risk to open it.
            if (![[nsURLRequest URL] isFileURL])
                [[NSWorkspace sharedWorkspace] openURL:[nsURLRequest URL]];
#endif
            localListener->ignore();
        });

        return;
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    RefPtr<API::NavigationAction> localNavigationAction = &navigationAction;
    RefPtr<WebFramePolicyListenerProxy> localListener = WTF::move(listener);
    RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:decidePolicyForNavigationAction:decisionHandler:));
    [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationAction:wrapper(navigationAction) decisionHandler:[localListener, localNavigationAction, checker, mainFrameURLString](WKNavigationActionPolicy actionPolicy) {
        checker->didCallCompletionHandler();

        switch (actionPolicy) {
        case WKNavigationActionPolicyAllow:
            tryAppLink(localNavigationAction, mainFrameURLString, [localListener](bool followedLinkToApp) {
                if (followedLinkToApp) {
                    localListener->ignore();
                    return;
                }

                localListener->use();
            });
        
            break;

        case WKNavigationActionPolicyCancel:
            localListener->ignore();
            break;

// FIXME: Once we have a new enough compiler everywhere we don't need to ignore -Wswitch.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
        case _WKNavigationActionPolicyDownload:
#pragma clang diagnostic pop
            localListener->download();
            break;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
        case _WKNavigationActionPolicyAllowWithoutTryingAppLink:
#pragma clang diagnostic pop
            localListener->use();
            break;
        }
    }];
}

void NavigationState::NavigationClient::decidePolicyForNavigationResponse(WebPageProxy&, API::NavigationResponse& navigationResponse, Ref<WebFramePolicyListenerProxy>&& listener, API::Object* userData)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationResponseDecisionHandler) {
        NSURL *url = navigationResponse.response().nsURLResponse().URL;
        if ([url isFileURL]) {
            BOOL isDirectory = NO;
            BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:url.path isDirectory:&isDirectory];

            if (exists && !isDirectory && navigationResponse.canShowMIMEType())
                listener->use();
            else
                listener->ignore();
            return;
        }

        if (navigationResponse.canShowMIMEType())
            listener->use();
        else
            listener->ignore();
        return;
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    RefPtr<WebFramePolicyListenerProxy> localListener = WTF::move(listener);
    RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:decidePolicyForNavigationResponse:decisionHandler:));
    [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationResponse:wrapper(navigationResponse) decisionHandler:[localListener, checker](WKNavigationResponsePolicy responsePolicy) {
        checker->didCallCompletionHandler();

        switch (responsePolicy) {
        case WKNavigationResponsePolicyAllow:
            localListener->use();
            break;

        case WKNavigationResponsePolicyCancel:
            localListener->ignore();
            break;

// FIXME: Once we have a new enough compiler everywhere we don't need to ignore -Wswitch.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
        case _WKNavigationResponsePolicyBecomeDownload:
#pragma clang diagnostic pop
            localListener->download();
            break;
        }
    }];
}

void NavigationState::NavigationClient::didStartProvisionalNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidStartProvisionalNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the page cache.
    WKNavigation *wkNavigation = nil;
    if (navigation)
        wkNavigation = wrapper(*navigation);

    [navigationDelegate webView:m_navigationState.m_webView didStartProvisionalNavigation:wkNavigation];
}

void NavigationState::NavigationClient::didReceiveServerRedirectForProvisionalNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidReceiveServerRedirectForProvisionalNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the page cache.
    WKNavigation *wkNavigation = nil;
    if (navigation)
        wkNavigation = wrapper(*navigation);

    [navigationDelegate webView:m_navigationState.m_webView didReceiveServerRedirectForProvisionalNavigation:wkNavigation];
}

static RetainPtr<NSError> createErrorWithRecoveryAttempter(WKWebView *webView, WebFrameProxy& webFrameProxy, NSError *originalError)
{
    RefPtr<API::FrameHandle> frameHandle = API::FrameHandle::create(webFrameProxy.frameID());

    auto recoveryAttempter = adoptNS([[WKReloadFrameErrorRecoveryAttempter alloc] initWithWebView:webView frameHandle:wrapper(*frameHandle) urlString:originalError.userInfo[NSURLErrorFailingURLStringErrorKey]]);

    auto userInfo = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:recoveryAttempter.get(), _WKRecoveryAttempterErrorKey, nil]);

    if (NSDictionary *originalUserInfo = originalError.userInfo)
        [userInfo addEntriesFromDictionary:originalUserInfo];

    return adoptNS([[NSError alloc] initWithDomain:originalError.domain code:originalError.code userInfo:userInfo.get()]);
}

// FIXME: Shouldn't need to pass the WebFrameProxy in here. At most, a FrameHandle.
void NavigationState::NavigationClient::didFailProvisionalNavigationWithError(WebPageProxy& page, WebFrameProxy& webFrameProxy, API::Navigation* navigation, const WebCore::ResourceError& error, API::Object*)
{
    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the page cache.
    RetainPtr<WKNavigation> wkNavigation;
    if (navigation)
        wkNavigation = wrapper(*navigation);
    
    // FIXME: Set the error on the navigation object.

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFailProvisionalNavigationWithError)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState.m_webView, webFrameProxy, error);
    [navigationDelegate webView:m_navigationState.m_webView didFailProvisionalNavigation:wkNavigation.get() withError:errorWithRecoveryAttempter.get()];
}

// FIXME: Shouldn't need to pass the WebFrameProxy in here. At most, a FrameHandle.
void NavigationState::NavigationClient::didFailProvisionalLoadInSubframeWithError(WebPageProxy& page, WebFrameProxy& webFrameProxy, const SecurityOriginData& securityOrigin, API::Navigation* navigation, const WebCore::ResourceError& error, API::Object*)
{
    // FIXME: We should assert that navigation is not null here, but it's currently null because WebPageProxy::didFailProvisionalLoadForFrame passes null.
    RetainPtr<WKNavigation> wkNavigation;
    if (navigation)
        wkNavigation = wrapper(*navigation);

    if (!m_navigationState.m_navigationDelegateMethods.webViewNavigationDidFailProvisionalLoadInSubframeWithError)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState.m_webView, webFrameProxy, error);

    [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView navigation:nil didFailProvisionalLoadInSubframe:wrapper(API::FrameInfo::create(webFrameProxy, securityOrigin.securityOrigin())) withError:errorWithRecoveryAttempter.get()];
}

void NavigationState::NavigationClient::didCommitNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidCommitNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the page cache.
    WKNavigation *wkNavigation = nil;
    if (navigation)
        wkNavigation = wrapper(*navigation);

    [navigationDelegate webView:m_navigationState.m_webView didCommitNavigation:wkNavigation];
}

void NavigationState::NavigationClient::didFinishDocumentLoad(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewNavigationDidFinishDocumentLoad)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the page cache.
    WKNavigation *wkNavigation = nil;
    if (navigation)
        wkNavigation = wrapper(*navigation);

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView navigationDidFinishDocumentLoad:wkNavigation];
}

void NavigationState::NavigationClient::didFinishNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFinishNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the page cache.
    WKNavigation *wkNavigation = nil;
    if (navigation)
        wkNavigation = wrapper(*navigation);

    [navigationDelegate webView:m_navigationState.m_webView didFinishNavigation:wkNavigation];
}

// FIXME: Shouldn't need to pass the WebFrameProxy in here. At most, a FrameHandle.
void NavigationState::NavigationClient::didFailNavigationWithError(WebPageProxy& page, WebFrameProxy& webFrameProxy, API::Navigation* navigation, const WebCore::ResourceError& error, API::Object* userData)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFailNavigationWithError)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the page cache.
    WKNavigation *wkNavigation = nil;
    if (navigation)
        wkNavigation = wrapper(*navigation);

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState.m_webView, webFrameProxy, error);
    [navigationDelegate webView:m_navigationState.m_webView didFailNavigation:wkNavigation withError:errorWithRecoveryAttempter.get()];
}

void NavigationState::NavigationClient::didSameDocumentNavigation(WebPageProxy&, API::Navigation* navigation, SameDocumentNavigationType navigationType, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewNavigationDidSameDocumentNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    WKNavigation *wkNavigation = nil;
    if (navigation)
        wkNavigation = wrapper(*navigation);

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView navigation:wkNavigation didSameDocumentNavigation:toWKSameDocumentNavigationType(navigationType)];
}

void NavigationState::NavigationClient::renderingProgressDidChange(WebKit::WebPageProxy&, WebCore::LayoutMilestones layoutMilestones, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewRenderingProgressDidChange)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView renderingProgressDidChange:renderingProgressEvents(layoutMilestones)];
}

bool NavigationState::NavigationClient::canAuthenticateAgainstProtectionSpace(WebKit::WebPageProxy&, WebKit::WebProtectionSpace* protectionSpace)
{
    if (m_navigationState.m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallengeCompletionHandler)
        return true;

    if (!m_navigationState.m_navigationDelegateMethods.webViewCanAuthenticateAgainstProtectionSpace)
        return false;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return false;

    return [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView canAuthenticateAgainstProtectionSpace:protectionSpace->protectionSpace().nsSpace()];
}

void NavigationState::NavigationClient::didReceiveAuthenticationChallenge(WebPageProxy&, AuthenticationChallengeProxy* authenticationChallenge)
{
    if (m_navigationState.m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallengeCompletionHandler) {
        auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
        if (!navigationDelegate) {
            authenticationChallenge->listener()->performDefaultHandling();
            return;
        }

        RefPtr<AuthenticationChallengeProxy> challenge = authenticationChallenge;
        RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:didReceiveAuthenticationChallenge:completionHandler:));
        [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) webView:m_navigationState.m_webView didReceiveAuthenticationChallenge:wrapper(*authenticationChallenge)
            completionHandler:[challenge, checker](NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) {
                checker->didCallCompletionHandler();

                switch (disposition) {
                case NSURLSessionAuthChallengeUseCredential: {
                    RefPtr<WebCredential> webCredential;
                    if (credential)
                        webCredential = WebCredential::create(WebCore::Credential(credential));

                    challenge->listener()->useCredential(webCredential.get());
                    break;
                }

                case NSURLSessionAuthChallengePerformDefaultHandling:
                    challenge->listener()->performDefaultHandling();
                    break;

                case NSURLSessionAuthChallengeCancelAuthenticationChallenge:
                    challenge->listener()->cancel();
                    break;

                case NSURLSessionAuthChallengeRejectProtectionSpace:
                    challenge->listener()->rejectProtectionSpaceAndContinue();
                    break;

                default:
                    [NSException raise:NSInvalidArgumentException format:@"Invalid NSURLSessionAuthChallengeDisposition (%ld)", (long)disposition];
                }
            }
        ];
        return;
    }

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallenge)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didReceiveAuthenticationChallenge:wrapper(*authenticationChallenge)];
}

void NavigationState::NavigationClient::processDidCrash(WebKit::WebPageProxy& page)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewWebContentProcessDidTerminate && !m_navigationState.m_navigationDelegateMethods.webViewWebProcessDidCrash)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // We prefer webViewWebContentProcessDidTerminate: over _webViewWebProcessDidCrash:.
    if (m_navigationState.m_navigationDelegateMethods.webViewWebContentProcessDidTerminate) {
        [navigationDelegate webViewWebContentProcessDidTerminate:m_navigationState.m_webView];
        return;
    }

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webViewWebProcessDidCrash:m_navigationState.m_webView];
}

void NavigationState::NavigationClient::processDidBecomeResponsive(WebKit::WebPageProxy& page)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewWebProcessDidBecomeResponsive)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webViewWebProcessDidBecomeResponsive:m_navigationState.m_webView];
}

void NavigationState::NavigationClient::processDidBecomeUnresponsive(WebKit::WebPageProxy& page)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewWebProcessDidBecomeUnresponsive)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webViewWebProcessDidBecomeUnresponsive:m_navigationState.m_webView];
}

PassRefPtr<API::Data> NavigationState::NavigationClient::webCryptoMasterKey(WebKit::WebPageProxy&)
{
    if (!m_navigationState.m_navigationDelegateMethods.webCryptoMasterKeyForWebView)
        return nullptr;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return nullptr;

    RetainPtr<NSData> data = [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webCryptoMasterKeyForWebView:m_navigationState.m_webView];

    return API::Data::createWithoutCopying((const unsigned char*)[data bytes], [data length], [] (unsigned char*, const void* data) {
        [(NSData *)data release];
    }, data.leakRef());
}

#if USE(QUICK_LOOK)
void NavigationState::NavigationClient::didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidStartLoadForQuickLookDocumentInMainFrame)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didStartLoadForQuickLookDocumentInMainFrameWithFileName:fileName uti:uti];
}

void NavigationState::NavigationClient::didFinishLoadForQuickLookDocumentInMainFrame(const WebKit::QuickLookDocumentData& data)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFinishLoadForQuickLookDocumentInMainFrame)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didFinishLoadForQuickLookDocumentInMainFrame:(NSData *)data.decodedData()];
}
#endif

// HistoryDelegatePrivate support
    
NavigationState::HistoryClient::HistoryClient(NavigationState& navigationState)
    : m_navigationState(navigationState)
{
}

NavigationState::HistoryClient::~HistoryClient()
{
}

void NavigationState::HistoryClient::didNavigateWithNavigationData(WebKit::WebPageProxy&, const WebKit::WebNavigationDataStore& navigationDataStore)
{
    if (!m_navigationState.m_historyDelegateMethods.webViewDidNavigateWithNavigationData)
        return;

    auto historyDelegate = m_navigationState.m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState.m_webView didNavigateWithNavigationData:wrapper(API::NavigationData::create(navigationDataStore))];
}

void NavigationState::HistoryClient::didPerformClientRedirect(WebKit::WebPageProxy&, const WTF::String& sourceURL, const WTF::String& destinationURL)
{
    if (!m_navigationState.m_historyDelegateMethods.webViewDidPerformClientRedirectFromURLToURL)
        return;

    auto historyDelegate = m_navigationState.m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState.m_webView didPerformClientRedirectFromURL:[NSURL _web_URLWithWTFString:sourceURL] toURL:[NSURL _web_URLWithWTFString:destinationURL]];
}

void NavigationState::HistoryClient::didPerformServerRedirect(WebKit::WebPageProxy&, const WTF::String& sourceURL, const WTF::String& destinationURL)
{
    if (!m_navigationState.m_historyDelegateMethods.webViewDidPerformServerRedirectFromURLToURL)
        return;

    auto historyDelegate = m_navigationState.m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState.m_webView didPerformServerRedirectFromURL:[NSURL _web_URLWithWTFString:sourceURL] toURL:[NSURL _web_URLWithWTFString:destinationURL]];
}

void NavigationState::HistoryClient::didUpdateHistoryTitle(WebKit::WebPageProxy&, const WTF::String& title, const WTF::String& url)
{
    if (!m_navigationState.m_historyDelegateMethods.webViewDidUpdateHistoryTitleForURL)
        return;

    auto historyDelegate = m_navigationState.m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState.m_webView didUpdateHistoryTitle:title forURL:[NSURL _web_URLWithWTFString:url]];
}

void NavigationState::willChangeIsLoading()
{
    [m_webView willChangeValueForKey:@"loading"];
}

void NavigationState::didChangeIsLoading()
{
#if PLATFORM(IOS)
    if (m_webView->_page->pageLoadState().isLoading())
        m_activityToken = m_webView->_page->process().throttler().backgroundActivityToken();
    else
        m_activityToken = nullptr;
#endif

    [m_webView didChangeValueForKey:@"loading"];
}

void NavigationState::willChangeTitle()
{
    [m_webView willChangeValueForKey:@"title"];
}

void NavigationState::didChangeTitle()
{
    [m_webView didChangeValueForKey:@"title"];
}

void NavigationState::willChangeActiveURL()
{
    [m_webView willChangeValueForKey:@"URL"];
}

void NavigationState::didChangeActiveURL()
{
    [m_webView didChangeValueForKey:@"URL"];
}

void NavigationState::willChangeHasOnlySecureContent()
{
    [m_webView willChangeValueForKey:@"hasOnlySecureContent"];
}

void NavigationState::didChangeHasOnlySecureContent()
{
    [m_webView didChangeValueForKey:@"hasOnlySecureContent"];
}

void NavigationState::willChangeEstimatedProgress()
{
    [m_webView willChangeValueForKey:@"estimatedProgress"];
}

void NavigationState::didChangeEstimatedProgress()
{
    [m_webView didChangeValueForKey:@"estimatedProgress"];
}

void NavigationState::willChangeCanGoBack()
{
    [m_webView willChangeValueForKey:@"canGoBack"];
}

void NavigationState::didChangeCanGoBack()
{
    [m_webView didChangeValueForKey:@"canGoBack"];
}

void NavigationState::willChangeCanGoForward()
{
    [m_webView willChangeValueForKey:@"canGoForward"];
}

void NavigationState::didChangeCanGoForward()
{
    [m_webView didChangeValueForKey:@"canGoForward"];
}

void NavigationState::willChangeNetworkRequestsInProgress()
{
    [m_webView willChangeValueForKey:@"_networkRequestsInProgress"];
}

void NavigationState::didChangeNetworkRequestsInProgress()
{
    [m_webView didChangeValueForKey:@"_networkRequestsInProgress"];
}

void NavigationState::willChangeCertificateInfo()
{
    [m_webView willChangeValueForKey:@"certificateChain"];
}

void NavigationState::didChangeCertificateInfo()
{
    [m_webView didChangeValueForKey:@"certificateChain"];
}

} // namespace WebKit

#endif
