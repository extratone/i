/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#import "WKBrowsingContextControllerInternal.h"

#if WK_API_ENABLED

#import "APIData.h"
#import "ObjCObjectGraph.h"
#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKBrowsingContextGroupInternal.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKBrowsingContextLoadDelegatePrivate.h"
#import "WKBrowsingContextPolicyDelegate.h"
#import "WKFrame.h"
#import "WKFramePolicyListener.h"
#import "WKNSArray.h"
#import "WKNSData.h"
#import "WKNSError.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKNSURLExtras.h"
#import "WKPagePolicyClientInternal.h"
#import "WKProcessGroupPrivate.h"
#import "WKRetainPtr.h"
#import "WKURLRequestNS.h"
#import "WKURLResponseNS.h"
#import "WeakObjCPtr.h"
#import "WebCertificateInfo.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProtectionSpace.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import <wtf/NeverDestroyed.h>

using namespace WebCore;
using namespace WebKit;

class PageLoadStateObserver : public PageLoadState::Observer {
public:
    PageLoadStateObserver(WKBrowsingContextController *controller)
        : m_controller(controller)
    {
    }

private:
    virtual void willChangeIsLoading() override
    {
        [m_controller willChangeValueForKey:@"loading"];
    }

    virtual void didChangeIsLoading() override
    {
        [m_controller didChangeValueForKey:@"loading"];
    }

    virtual void willChangeTitle() override
    {
        [m_controller willChangeValueForKey:@"title"];
    }

    virtual void didChangeTitle() override
    {
        [m_controller didChangeValueForKey:@"title"];
    }

    virtual void willChangeActiveURL() override
    {
        [m_controller willChangeValueForKey:@"activeURL"];
    }

    virtual void didChangeActiveURL() override
    {
        [m_controller didChangeValueForKey:@"activeURL"];
    }

    virtual void willChangeHasOnlySecureContent() override
    {
        [m_controller willChangeValueForKey:@"hasOnlySecureContent"];
    }

    virtual void didChangeHasOnlySecureContent() override
    {
        [m_controller didChangeValueForKey:@"hasOnlySecureContent"];
    }

    virtual void willChangeEstimatedProgress() override
    {
        [m_controller willChangeValueForKey:@"estimatedProgress"];
    }

    virtual void didChangeEstimatedProgress() override
    {
        [m_controller didChangeValueForKey:@"estimatedProgress"];
    }

    virtual void willChangeCanGoBack() override { }
    virtual void didChangeCanGoBack() override { }
    virtual void willChangeCanGoForward() override { }
    virtual void didChangeCanGoForward() override { }
    virtual void willChangeNetworkRequestsInProgress() override { }
    virtual void didChangeNetworkRequestsInProgress() override { }
    virtual void willChangeCertificateInfo() override { }
    virtual void didChangeCertificateInfo() override { }

    WKBrowsingContextController *m_controller;
};

NSString * const WKActionIsMainFrameKey = @"WKActionIsMainFrameKey";
NSString * const WKActionNavigationTypeKey = @"WKActionNavigationTypeKey";
NSString * const WKActionMouseButtonKey = @"WKActionMouseButtonKey";
NSString * const WKActionModifierFlagsKey = @"WKActionModifierFlagsKey";
NSString * const WKActionOriginalURLRequestKey = @"WKActionOriginalURLRequestKey";
NSString * const WKActionURLRequestKey = @"WKActionURLRequestKey";
NSString * const WKActionURLResponseKey = @"WKActionURLResponseKey";
NSString * const WKActionFrameNameKey = @"WKActionFrameNameKey";
NSString * const WKActionOriginatingFrameURLKey = @"WKActionOriginatingFrameURLKey";
NSString * const WKActionCanShowMIMETypeKey = @"WKActionCanShowMIMETypeKey";

@implementation WKBrowsingContextController {
    RefPtr<WebPageProxy> _page;
    std::unique_ptr<PageLoadStateObserver> _pageLoadStateObserver;

    WeakObjCPtr<id <WKBrowsingContextLoadDelegate>> _loadDelegate;
    WeakObjCPtr<id <WKBrowsingContextPolicyDelegate>> _policyDelegate;
    
    RetainPtr<_WKRemoteObjectRegistry> _remoteObjectRegistry;
}

static HashMap<WebPageProxy*, WKBrowsingContextController *>& browsingContextControllerMap()
{
    static NeverDestroyed<HashMap<WebPageProxy*, WKBrowsingContextController *>> browsingContextControllerMap;
    return browsingContextControllerMap;
}

- (void)dealloc
{
    ASSERT(browsingContextControllerMap().get(_page.get()) == self);
    browsingContextControllerMap().remove(_page.get());

    _page->pageLoadState().removeObserver(*_pageLoadStateObserver);

    if (_remoteObjectRegistry) {
        _page->process().processPool().removeMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->pageID());
        [_remoteObjectRegistry _invalidate];
    }

    [super dealloc];
}

#pragma mark Loading

+ (void)registerSchemeForCustomProtocol:(NSString *)scheme
{
    WebProcessPool::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(scheme);
}

+ (void)unregisterSchemeForCustomProtocol:(NSString *)scheme
{
    WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(scheme);
}

- (void)loadRequest:(NSURLRequest *)request
{
    [self loadRequest:request userData:nil];
}

- (void)loadRequest:(NSURLRequest *)request userData:(id)userData
{
    RefPtr<ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = ObjCObjectGraph::create(userData);

    _page->loadRequest(request, ShouldOpenExternalURLsPolicy::ShouldNotAllow, wkUserData.get());
}

- (void)loadFileURL:(NSURL *)URL restrictToFilesWithin:(NSURL *)allowedDirectory
{
    [self loadFileURL:URL restrictToFilesWithin:allowedDirectory userData:nil];
}

- (void)loadFileURL:(NSURL *)URL restrictToFilesWithin:(NSURL *)allowedDirectory userData:(id)userData
{
    if (![URL isFileURL] || (allowedDirectory && ![allowedDirectory isFileURL]))
        [NSException raise:NSInvalidArgumentException format:@"Attempted to load a non-file URL"];

    RefPtr<ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = ObjCObjectGraph::create(userData);

    _page->loadFile([URL _web_originalDataAsWTFString], [allowedDirectory _web_originalDataAsWTFString], wkUserData.get());
}

- (void)loadHTMLString:(NSString *)HTMLString baseURL:(NSURL *)baseURL
{
    [self loadHTMLString:HTMLString baseURL:baseURL userData:nil];
}

- (void)loadHTMLString:(NSString *)HTMLString baseURL:(NSURL *)baseURL userData:(id)userData
{
    RefPtr<ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = ObjCObjectGraph::create(userData);

    _page->loadHTMLString(HTMLString, [baseURL _web_originalDataAsWTFString], wkUserData.get());
}

- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL
{
    _page->loadAlternateHTMLString(string, [baseURL _web_originalDataAsWTFString], [unreachableURL _web_originalDataAsWTFString]);
}

- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL
{
    [self loadData:data MIMEType:MIMEType textEncodingName:encodingName baseURL:baseURL userData:nil];
}

- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL userData:(id)userData
{
    RefPtr<API::Data> apiData;
    if (data) {
        // FIXME: This should copy the data.
        apiData = API::Data::createWithoutCopying(data);
    }

    RefPtr<ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = ObjCObjectGraph::create(userData);

    _page->loadData(apiData.get(), MIMEType, encodingName, [baseURL _web_originalDataAsWTFString], wkUserData.get());
}

- (void)stopLoading
{
    _page->stopLoading();
}

- (void)reload
{
    const bool reloadFromOrigin = false;
    const bool contentBlockersEnabled = true;
    _page->reload(reloadFromOrigin, contentBlockersEnabled);
}

- (void)reloadFromOrigin
{
    const bool reloadFromOrigin = true;
    const bool contentBlockersEnabled = true;
    _page->reload(reloadFromOrigin, contentBlockersEnabled);
}

- (NSString *)applicationNameForUserAgent
{
    const String& applicationName = _page->applicationNameForUserAgent();
    return !applicationName ? nil : (NSString *)applicationName;
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationNameForUserAgent
{
    _page->setApplicationNameForUserAgent(applicationNameForUserAgent);
}

- (NSString *)customUserAgent
{
    const String& customUserAgent = _page->customUserAgent();
    return !customUserAgent ? nil : (NSString *)customUserAgent;
}

- (void)setCustomUserAgent:(NSString *)customUserAgent
{
    _page->setCustomUserAgent(customUserAgent);
}

#pragma mark Back/Forward

- (void)goForward
{
    _page->goForward();
}

- (BOOL)canGoForward
{
    return !!_page->backForwardList().forwardItem();
}

- (void)goBack
{
    _page->goBack();
}

- (BOOL)canGoBack
{
    return !!_page->backForwardList().backItem();
}

- (void)goToBackForwardListItem:(WKBackForwardListItem *)item
{
    _page->goToBackForwardItem(&item._item);
}

- (WKBackForwardList *)backForwardList
{
    return wrapper(_page->backForwardList());
}

#pragma mark Active Load Introspection

- (BOOL)isLoading
{
    return _page->pageLoadState().isLoading();
}

- (NSURL *)activeURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().activeURL()];
}

- (NSURL *)provisionalURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().provisionalURL()];
}

- (NSURL *)committedURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().url()];
}

- (NSURL *)unreachableURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().unreachableURL()];
}

- (BOOL)hasOnlySecureContent
{
    return _page->pageLoadState().hasOnlySecureContent();
}

- (double)estimatedProgress
{
    return _page->estimatedProgress();
}

#pragma mark Active Document Introspection

- (NSString *)title
{
    return _page->pageLoadState().title();
}

- (NSArray *)certificateChain
{
    if (WebFrameProxy* mainFrame = _page->mainFrame())
        return mainFrame->certificateInfo() ? (NSArray *)mainFrame->certificateInfo()->certificateInfo().certificateChain() : nil;

    return nil;
}

#pragma mark Zoom

- (CGFloat)textZoom
{
    return _page->textZoomFactor();
}

- (void)setTextZoom:(CGFloat)textZoom
{
    _page->setTextZoomFactor(textZoom);
}

- (CGFloat)pageZoom
{
    return _page->pageZoomFactor();
}

- (void)setPageZoom:(CGFloat)pageZoom
{
    _page->setPageZoomFactor(pageZoom);
}

static void didStartProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerDidStartProvisionalLoad:)])
        [loadDelegate browsingContextControllerDidStartProvisionalLoad:browsingContext];
}

static void didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerDidReceiveServerRedirectForProvisionalLoad:)])
        [loadDelegate browsingContextControllerDidReceiveServerRedirectForProvisionalLoad:browsingContext];
}

static void didFailProvisionalLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextController:didFailProvisionalLoadWithError:)])
        [loadDelegate browsingContextController:browsingContext didFailProvisionalLoadWithError:wrapper(*toImpl(error))];
}

static void didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerDidCommitLoad:)])
        [loadDelegate browsingContextControllerDidCommitLoad:browsingContext];
}

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerDidFinishLoad:)])
        [loadDelegate browsingContextControllerDidFinishLoad:browsingContext];
}

static void didFailLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextController:didFailLoadWithError:)])
        [loadDelegate browsingContextController:browsingContext didFailLoadWithError:wrapper(*toImpl(error))];
}

static bool canAuthenticateAgainstProtectionSpaceInFrame(WKPageRef page, WKFrameRef frame, WKProtectionSpaceRef protectionSpace, const void *clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextController:canAuthenticateAgainstProtectionSpace:)])
        return [(id <WKBrowsingContextLoadDelegatePrivate>)loadDelegate browsingContextController:browsingContext canAuthenticateAgainstProtectionSpace:toImpl(protectionSpace)->protectionSpace().nsSpace()];

    return false;
}

static void didReceiveAuthenticationChallengeInFrame(WKPageRef page, WKFrameRef frame, WKAuthenticationChallengeRef authenticationChallenge, const void *clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextController:didReceiveAuthenticationChallenge:)])
        [(id <WKBrowsingContextLoadDelegatePrivate>)loadDelegate browsingContextController:browsingContext didReceiveAuthenticationChallenge:wrapper(*toImpl(authenticationChallenge))];
}

static void didStartProgress(WKPageRef page, const void* clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerDidStartProgress:)])
        [loadDelegate browsingContextControllerDidStartProgress:browsingContext];
}

static void didChangeProgress(WKPageRef page, const void* clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextController:estimatedProgressChangedTo:)])
        [loadDelegate browsingContextController:browsingContext estimatedProgressChangedTo:toImpl(page)->estimatedProgress()];
}

static void didFinishProgress(WKPageRef page, const void* clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerDidFinishProgress:)])
        [loadDelegate browsingContextControllerDidFinishProgress:browsingContext];
}

static void didChangeBackForwardList(WKPageRef page, WKBackForwardListItemRef addedItem, WKArrayRef removedItems, const void *clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if (![loadDelegate respondsToSelector:@selector(browsingContextControllerDidChangeBackForwardList:addedItem:removedItems:)])
        return;

    WKBackForwardListItem *added = addedItem ? wrapper(*toImpl(addedItem)) : nil;
    NSArray *removed = removedItems ? wrapper(*toImpl(removedItems)) : nil;
    [loadDelegate browsingContextControllerDidChangeBackForwardList:browsingContext addedItem:added removedItems:removed];
}

static void processDidCrash(WKPageRef page, const void* clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerWebProcessDidCrash:)])
        [(id <WKBrowsingContextLoadDelegatePrivate>)loadDelegate browsingContextControllerWebProcessDidCrash:browsingContext];
}

static void setUpPageLoaderClient(WKBrowsingContextController *browsingContext, WebPageProxy& page)
{
    WKPageLoaderClientV4 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 4;
    loaderClient.base.clientInfo = browsingContext;
    loaderClient.didStartProvisionalLoadForFrame = didStartProvisionalLoadForFrame;
    loaderClient.didReceiveServerRedirectForProvisionalLoadForFrame = didReceiveServerRedirectForProvisionalLoadForFrame;
    loaderClient.didFailProvisionalLoadWithErrorForFrame = didFailProvisionalLoadWithErrorForFrame;
    loaderClient.didCommitLoadForFrame = didCommitLoadForFrame;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;
    loaderClient.didFailLoadWithErrorForFrame = didFailLoadWithErrorForFrame;

    loaderClient.canAuthenticateAgainstProtectionSpaceInFrame = canAuthenticateAgainstProtectionSpaceInFrame;
    loaderClient.didReceiveAuthenticationChallengeInFrame = didReceiveAuthenticationChallengeInFrame;

    loaderClient.didStartProgress = didStartProgress;
    loaderClient.didChangeProgress = didChangeProgress;
    loaderClient.didFinishProgress = didFinishProgress;
    loaderClient.didChangeBackForwardList = didChangeBackForwardList;

    loaderClient.processDidCrash = processDidCrash;

    WKPageSetPageLoaderClient(toAPI(&page), &loaderClient.base);
}

static WKPolicyDecisionHandler makePolicyDecisionBlock(WKFramePolicyListenerRef listener)
{
    WKRetain(listener); // Released in the decision handler below.

    return [[^(WKPolicyDecision decision) {
        switch (decision) {
        case WKPolicyDecisionCancel:
            WKFramePolicyListenerIgnore(listener);                    
            break;
        
        case WKPolicyDecisionAllow:
            WKFramePolicyListenerUse(listener);
            break;
        
        case WKPolicyDecisionBecomeDownload:
            WKFramePolicyListenerDownload(listener);
            break;
        };

        WKRelease(listener); // Retained in the context above.
    } copy] autorelease];
}

static void setUpPagePolicyClient(WKBrowsingContextController *browsingContext, WebPageProxy& page)
{
    WKPagePolicyClientInternal policyClient;
    memset(&policyClient, 0, sizeof(policyClient));

    policyClient.base.version = 2;
    policyClient.base.clientInfo = browsingContext;

    policyClient.decidePolicyForNavigationAction = [](WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKFrameRef originatingFrame, WKURLRequestRef originalRequest, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
    {
        WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
        auto policyDelegate = browsingContext->_policyDelegate.get();

        if ([policyDelegate respondsToSelector:@selector(browsingContextController:decidePolicyForNavigationAction:decisionHandler:)]) {
            NSDictionary *actionDictionary = @{
                WKActionIsMainFrameKey: @(WKFrameIsMainFrame(frame)),
                WKActionNavigationTypeKey: @(navigationType),
                WKActionModifierFlagsKey: @(modifiers),
                WKActionMouseButtonKey: @(mouseButton),
                WKActionOriginalURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(originalRequest)).get(),
                WKActionURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(request)).get()
            };

            if (originatingFrame) {
                actionDictionary = [[actionDictionary mutableCopy] autorelease];
                [(NSMutableDictionary *)actionDictionary setObject:[NSURL _web_URLWithWTFString:toImpl(originatingFrame)->url()] forKey:WKActionOriginatingFrameURLKey];
            }
            
            [policyDelegate browsingContextController:browsingContext decidePolicyForNavigationAction:actionDictionary decisionHandler:makePolicyDecisionBlock(listener)];
        } else
            WKFramePolicyListenerUse(listener);
    };

    policyClient.decidePolicyForNewWindowAction = [](WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKStringRef frameName, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
    {
        WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
        auto policyDelegate = browsingContext->_policyDelegate.get();

        if ([policyDelegate respondsToSelector:@selector(browsingContextController:decidePolicyForNewWindowAction:decisionHandler:)]) {
            NSDictionary *actionDictionary = @{
                WKActionIsMainFrameKey: @(WKFrameIsMainFrame(frame)),
                WKActionNavigationTypeKey: @(navigationType),
                WKActionModifierFlagsKey: @(modifiers),
                WKActionMouseButtonKey: @(mouseButton),
                WKActionURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(request)).get(),
                WKActionFrameNameKey: toImpl(frameName)->wrapper()
            };
            
            [policyDelegate browsingContextController:browsingContext decidePolicyForNewWindowAction:actionDictionary decisionHandler:makePolicyDecisionBlock(listener)];
        } else
            WKFramePolicyListenerUse(listener);
    };

    policyClient.decidePolicyForResponse = [](WKPageRef page, WKFrameRef frame, WKURLResponseRef response, WKURLRequestRef request, bool canShowMIMEType, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
    {
        WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
        auto policyDelegate = browsingContext->_policyDelegate.get();

        if ([policyDelegate respondsToSelector:@selector(browsingContextController:decidePolicyForResponseAction:decisionHandler:)]) {
            NSDictionary *actionDictionary = @{
                WKActionIsMainFrameKey: @(WKFrameIsMainFrame(frame)),
                WKActionURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(request)).get(),
                WKActionURLResponseKey: adoptNS(WKURLResponseCopyNSURLResponse(response)).get(),
                WKActionCanShowMIMETypeKey: @(canShowMIMEType),
            };

            [policyDelegate browsingContextController:browsingContext decidePolicyForResponseAction:actionDictionary decisionHandler:makePolicyDecisionBlock(listener)];
        } else
            WKFramePolicyListenerUse(listener);
    };

    WKPageSetPagePolicyClient(toAPI(&page), &policyClient.base);
}

- (id <WKBrowsingContextLoadDelegate>)loadDelegate
{
    return _loadDelegate.getAutoreleased();
}

- (void)setLoadDelegate:(id <WKBrowsingContextLoadDelegate>)loadDelegate
{
    _loadDelegate = loadDelegate;

    if (loadDelegate)
        setUpPageLoaderClient(self, *_page);
    else
        WKPageSetPageLoaderClient(toAPI(_page.get()), nullptr);
}

- (id <WKBrowsingContextPolicyDelegate>)policyDelegate
{
    return _policyDelegate.getAutoreleased();
}

- (void)setPolicyDelegate:(id <WKBrowsingContextPolicyDelegate>)policyDelegate
{
    _policyDelegate = policyDelegate;

    if (policyDelegate)
        setUpPagePolicyClient(self, *_page);
    else
        WKPageSetPagePolicyClient(toAPI(_page.get()), nullptr);
}

- (id <WKBrowsingContextHistoryDelegate>)historyDelegate
{
    return _historyDelegate.getAutoreleased();
}

- (void)setHistoryDelegate:(id <WKBrowsingContextHistoryDelegate>)historyDelegate
{
    _historyDelegate = historyDelegate;
}

+ (NSMutableSet *)customSchemes
{
    static NSMutableSet *customSchemes = [[NSMutableSet alloc] init];
    return customSchemes;
}

- (instancetype)_initWithPageRef:(WKPageRef)pageRef
{
    if (!(self = [super init]))
        return nil;

    _page = toImpl(pageRef);

    _pageLoadStateObserver = std::make_unique<PageLoadStateObserver>(self);
    _page->pageLoadState().addObserver(*_pageLoadStateObserver);

    ASSERT(!browsingContextControllerMap().contains(_page.get()));
    browsingContextControllerMap().set(_page.get(), self);

    return self;
}

+ (WKBrowsingContextController *)_browsingContextControllerForPageRef:(WKPageRef)pageRef
{
    return browsingContextControllerMap().get(toImpl(pageRef));
}

@end

@implementation WKBrowsingContextController (Private)

- (WKPageRef)_pageRef
{
    return toAPI(_page.get());
}

- (void)setPaginationMode:(WKBrowsingContextPaginationMode)paginationMode
{
    Pagination::Mode mode;
    switch (paginationMode) {
    case WKPaginationModeUnpaginated:
        mode = Pagination::Unpaginated;
        break;
    case WKPaginationModeLeftToRight:
        mode = Pagination::LeftToRightPaginated;
        break;
    case WKPaginationModeRightToLeft:
        mode = Pagination::RightToLeftPaginated;
        break;
    case WKPaginationModeTopToBottom:
        mode = Pagination::TopToBottomPaginated;
        break;
    case WKPaginationModeBottomToTop:
        mode = Pagination::BottomToTopPaginated;
        break;
    default:
        return;
    }

    _page->setPaginationMode(mode);
}

- (WKBrowsingContextPaginationMode)paginationMode
{
    switch (_page->paginationMode()) {
    case Pagination::Unpaginated:
        return WKPaginationModeUnpaginated;
    case Pagination::LeftToRightPaginated:
        return WKPaginationModeLeftToRight;
    case Pagination::RightToLeftPaginated:
        return WKPaginationModeRightToLeft;
    case Pagination::TopToBottomPaginated:
        return WKPaginationModeTopToBottom;
    case Pagination::BottomToTopPaginated:
        return WKPaginationModeBottomToTop;
    }

    ASSERT_NOT_REACHED();
    return WKPaginationModeUnpaginated;
}

- (void)setPaginationBehavesLikeColumns:(BOOL)behavesLikeColumns
{
    _page->setPaginationBehavesLikeColumns(behavesLikeColumns);
}

- (BOOL)paginationBehavesLikeColumns
{
    return _page->paginationBehavesLikeColumns();
}

- (void)setPageLength:(CGFloat)pageLength
{
    _page->setPageLength(pageLength);
}

- (CGFloat)pageLength
{
    return _page->pageLength();
}

- (void)setGapBetweenPages:(CGFloat)gapBetweenPages
{
    _page->setGapBetweenPages(gapBetweenPages);
}

- (CGFloat)gapBetweenPages
{
    return _page->gapBetweenPages();
}

- (NSUInteger)pageCount
{
    return _page->pageCount();
}

- (WKBrowsingContextHandle *)handle
{
    return [[[WKBrowsingContextHandle alloc] _initWithPageID:_page->pageID()] autorelease];
}

- (_WKRemoteObjectRegistry *)_remoteObjectRegistry
{
    if (!_remoteObjectRegistry) {
        _remoteObjectRegistry = adoptNS([[_WKRemoteObjectRegistry alloc] _initWithMessageSender:*_page]);
        _page->process().processPool().addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->pageID(), [_remoteObjectRegistry remoteObjectRegistry]);
    }

    return _remoteObjectRegistry.get();
}

- (pid_t)processIdentifier
{
    return _page->processIdentifier();
}

@end

#endif // WK_API_ENABLED
