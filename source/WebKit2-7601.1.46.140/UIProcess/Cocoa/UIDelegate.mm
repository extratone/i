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
#import "UIDelegate.h"

#if WK_API_ENABLED

#import "CompletionHandlerCallChecker.h"
#import "NavigationActionData.h"
#import "SecurityOriginData.h"
#import "WKFrameInfoInternal.h"
#import "WKNavigationActionInternal.h"
#import "WKSecurityOriginInternal.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewInternal.h"
#import "WKWindowFeaturesInternal.h"
#import "WKUIDelegatePrivate.h"
#import "_WKFrameHandleInternal.h"
#import <WebCore/URL.h>

namespace WebKit {

UIDelegate::UIDelegate(WKWebView *webView)
    : m_webView(webView)
{
}

UIDelegate::~UIDelegate()
{
}

std::unique_ptr<API::UIClient> UIDelegate::createUIClient()
{
    return std::make_unique<UIClient>(*this);
}

RetainPtr<id <WKUIDelegate> > UIDelegate::delegate()
{
    return m_delegate.get();
}

void UIDelegate::setDelegate(id <WKUIDelegate> delegate)
{
    m_delegate = delegate;

    m_delegateMethods.webViewCreateWebViewWithConfigurationForNavigationActionWindowFeatures = [delegate respondsToSelector:@selector(webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures:)];
    m_delegateMethods.webViewRunJavaScriptAlertPanelWithMessageInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewRunJavaScriptConfirmPanelWithMessageInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewRunJavaScriptTextInputPanelWithPromptDefaultTextInitiatedByFrameCompletionHandler = [delegate respondsToSelector:@selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:completionHandler:)];
    m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler = [delegate respondsToSelector:@selector(_webView:decideDatabaseQuotaForSecurityOrigin:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:)];
    m_delegateMethods.webViewDecideWebApplicationCacheQuotaForSecurityOriginCurrentQuotaTotalBytesNeeded = [delegate respondsToSelector:@selector(_webView:decideWebApplicationCacheQuotaForSecurityOrigin:currentQuota:totalBytesNeeded:decisionHandler:)];
    m_delegateMethods.webViewPrintFrame = [delegate respondsToSelector:@selector(_webView:printFrame:)];
    m_delegateMethods.webViewDidClose = [delegate respondsToSelector:@selector(webViewDidClose:)];
    m_delegateMethods.webViewClose = [delegate respondsToSelector:@selector(_webViewClose:)];
    m_delegateMethods.webViewFullscreenMayReturnToInline = [delegate respondsToSelector:@selector(_webViewFullscreenMayReturnToInline:)];
    m_delegateMethods.webViewDidEnterFullscreen = [delegate respondsToSelector:@selector(_webViewDidEnterFullscreen:)];
    m_delegateMethods.webViewDidExitFullscreen = [delegate respondsToSelector:@selector(_webViewDidExitFullscreen:)];
#if PLATFORM(IOS)
#if HAVE(APP_LINKS)
    m_delegateMethods.webViewShouldIncludeAppLinkActionsForElement = [delegate respondsToSelector:@selector(_webView:shouldIncludeAppLinkActionsForElement:)];
#endif
    m_delegateMethods.webViewActionsForElementDefaultActions = [delegate respondsToSelector:@selector(_webView:actionsForElement:defaultActions:)];
    m_delegateMethods.webViewDidNotHandleTapAsClickAtPoint = [delegate respondsToSelector:@selector(_webView:didNotHandleTapAsClickAtPoint:)];
#endif
#if ENABLE(VIDEO)
    m_delegateMethods.webViewMediaDocumentNaturalSizeChanged = [delegate respondsToSelector:@selector(_webView:mediaDocumentNaturalSizeChanged:)];
#endif
}

UIDelegate::UIClient::UIClient(UIDelegate& uiDelegate)
    : m_uiDelegate(uiDelegate)
{
}

UIDelegate::UIClient::~UIClient()
{
}

PassRefPtr<WebKit::WebPageProxy> UIDelegate::UIClient::createNewPage(WebKit::WebPageProxy*, WebKit::WebFrameProxy* initiatingFrame, const WebKit::SecurityOriginData& securityOriginData, const WebCore::ResourceRequest& request, const WebCore::WindowFeatures& windowFeatures, const WebKit::NavigationActionData& navigationActionData)
{
    if (!m_uiDelegate.m_delegateMethods.webViewCreateWebViewWithConfigurationForNavigationActionWindowFeatures)
        return nullptr;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return nullptr;

    auto configuration = adoptNS([m_uiDelegate.m_webView->_configuration copy]);
    [configuration _setRelatedWebView:m_uiDelegate.m_webView];

    auto sourceFrameInfo = API::FrameInfo::create(*initiatingFrame, securityOriginData.securityOrigin());

    bool shouldOpenAppLinks = !hostsAreEqual(WebCore::URL(WebCore::ParsedURLString, initiatingFrame->url()), request.url());
    auto navigationAction = API::NavigationAction::create(navigationActionData, sourceFrameInfo.ptr(), nullptr, request, WebCore::URL(), shouldOpenAppLinks);

    RetainPtr<WKWebView> webView = [delegate.get() webView:m_uiDelegate.m_webView createWebViewWithConfiguration:configuration.get() forNavigationAction:wrapper(navigationAction) windowFeatures:adoptNS([[WKWindowFeatures alloc] _initWithWindowFeatures:windowFeatures]).get()];

    if (!webView)
        return nullptr;

    if ([webView->_configuration _relatedWebView] != m_uiDelegate.m_webView)
        [NSException raise:NSInternalInconsistencyException format:@"Returned WKWebView was not created with the given configuration."];

    return webView->_page.get();
}

void UIDelegate::UIClient::runJavaScriptAlert(WebKit::WebPageProxy*, const WTF::String& message, WebKit::WebFrameProxy* webFrameProxy, const WebKit::SecurityOriginData& securityOriginData, std::function<void ()> completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRunJavaScriptAlertPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler();
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler();
        return;
    }

    RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:completionHandler:));
    [delegate webView:m_uiDelegate.m_webView runJavaScriptAlertPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(*webFrameProxy, securityOriginData.securityOrigin())) completionHandler:[completionHandler, checker] {
        completionHandler();
        checker->didCallCompletionHandler();
    }];
}

void UIDelegate::UIClient::runJavaScriptConfirm(WebKit::WebPageProxy*, const WTF::String& message, WebKit::WebFrameProxy* webFrameProxy, const WebKit::SecurityOriginData& securityOriginData, std::function<void (bool)> completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRunJavaScriptConfirmPanelWithMessageInitiatedByFrameCompletionHandler) {
        completionHandler(false);
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(false);
        return;
    }

    RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:completionHandler:));
    [delegate webView:m_uiDelegate.m_webView runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:wrapper(API::FrameInfo::create(*webFrameProxy, securityOriginData.securityOrigin())) completionHandler:[completionHandler, checker](BOOL result) {
        completionHandler(result);
        checker->didCallCompletionHandler();
    }];
}

void UIDelegate::UIClient::runJavaScriptPrompt(WebKit::WebPageProxy*, const WTF::String& message, const WTF::String& defaultValue, WebKit::WebFrameProxy* webFrameProxy, const WebKit::SecurityOriginData& securityOriginData, std::function<void (const WTF::String&)> completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewRunJavaScriptTextInputPanelWithPromptDefaultTextInitiatedByFrameCompletionHandler) {
        completionHandler(String());
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(String());
        return;
    }

    RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:completionHandler:));
    [delegate webView:m_uiDelegate.m_webView runJavaScriptTextInputPanelWithPrompt:message defaultText:defaultValue initiatedByFrame:wrapper(API::FrameInfo::create(*webFrameProxy, securityOriginData.securityOrigin())) completionHandler:[completionHandler, checker](NSString *result) {
        completionHandler(result);
        checker->didCallCompletionHandler();
    }];
}

void UIDelegate::UIClient::exceededDatabaseQuota(WebPageProxy*, WebFrameProxy*, API::SecurityOrigin* securityOrigin, const WTF::String& databaseName, const WTF::String& displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentUsage, unsigned long long expectedUsage, std::function<void (unsigned long long)> completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDecideDatabaseQuotaForSecurityOriginCurrentQuotaCurrentOriginUsageCurrentDatabaseUsageExpectedUsageDecisionHandler) {
        completionHandler(currentQuota);
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(currentQuota);
        return;
    }

    ASSERT(securityOrigin);
    RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decideDatabaseQuotaForSecurityOrigin:currentQuota:currentOriginUsage:currentDatabaseUsage:expectedUsage:decisionHandler:));
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView decideDatabaseQuotaForSecurityOrigin:wrapper(*securityOrigin) currentQuota:currentQuota currentOriginUsage:currentOriginUsage currentDatabaseUsage:currentUsage expectedUsage:expectedUsage decisionHandler:[completionHandler, checker](unsigned long long newQuota) {
        checker->didCallCompletionHandler();
        completionHandler(newQuota);
    }];
}

void UIDelegate::UIClient::reachedApplicationCacheOriginQuota(WebPageProxy*, const WebCore::SecurityOrigin& securityOrigin, uint64_t currentQuota, uint64_t totalBytesNeeded, std::function<void (unsigned long long)> completionHandler)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDecideWebApplicationCacheQuotaForSecurityOriginCurrentQuotaTotalBytesNeeded) {
        completionHandler(currentQuota);
        return;
    }

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(currentQuota);
        return;
    }

    RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(delegate.get(), @selector(_webView:decideWebApplicationCacheQuotaForSecurityOrigin:currentQuota:totalBytesNeeded:decisionHandler:));
    RefPtr<API::SecurityOrigin> apiOrigin = API::SecurityOrigin::create(securityOrigin);
    
    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView decideWebApplicationCacheQuotaForSecurityOrigin:wrapper(*apiOrigin) currentQuota:currentQuota totalBytesNeeded:totalBytesNeeded decisionHandler:[completionHandler, checker](unsigned long long newQuota) {
        checker->didCallCompletionHandler();
        completionHandler(newQuota);
    }];
}

void UIDelegate::UIClient::printFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy* webFrameProxy)
{
    ASSERT_ARG(webFrameProxy, webFrameProxy);

    if (!m_uiDelegate.m_delegateMethods.webViewPrintFrame)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView printFrame:wrapper(API::FrameHandle::create(webFrameProxy->frameID()))];
}

void UIDelegate::UIClient::close(WebKit::WebPageProxy*)
{
    if (m_uiDelegate.m_delegateMethods.webViewClose) {
        auto delegate = m_uiDelegate.m_delegate.get();
        if (!delegate)
            return;

        [(id <WKUIDelegatePrivate>)delegate _webViewClose:m_uiDelegate.m_webView];
        return;
    }

    if (!m_uiDelegate.m_delegateMethods.webViewDidClose)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [delegate webViewDidClose:m_uiDelegate.m_webView];
}

void UIDelegate::UIClient::fullscreenMayReturnToInline(WebKit::WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.webViewFullscreenMayReturnToInline)
        return;
    
    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;
    
    [(id <WKUIDelegatePrivate>)delegate _webViewFullscreenMayReturnToInline:m_uiDelegate.m_webView];
}

void UIDelegate::UIClient::didEnterFullscreen(WebKit::WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidEnterFullscreen)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewDidEnterFullscreen:m_uiDelegate.m_webView];
}

void UIDelegate::UIClient::didExitFullscreen(WebKit::WebPageProxy*)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidExitFullscreen)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [(id <WKUIDelegatePrivate>)delegate _webViewDidExitFullscreen:m_uiDelegate.m_webView];
}
    
#if PLATFORM(IOS)
#if HAVE(APP_LINKS)
bool UIDelegate::UIClient::shouldIncludeAppLinkActionsForElement(_WKActivatedElementInfo *elementInfo)
{
    if (!m_uiDelegate.m_delegateMethods.webViewShouldIncludeAppLinkActionsForElement)
        return true;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return true;

    return [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView shouldIncludeAppLinkActionsForElement:elementInfo];
}
#endif

RetainPtr<NSArray> UIDelegate::UIClient::actionsForElement(_WKActivatedElementInfo *elementInfo, RetainPtr<NSArray> defaultActions)
{
    if (!m_uiDelegate.m_delegateMethods.webViewActionsForElementDefaultActions)
        return WTF::move(defaultActions);

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return defaultActions;

    return [(id <WKUIDelegatePrivate>)delegate _webView:m_uiDelegate.m_webView actionsForElement:elementInfo defaultActions:defaultActions.get()];
}

void UIDelegate::UIClient::didNotHandleTapAsClick(const WebCore::IntPoint& point)
{
    if (!m_uiDelegate.m_delegateMethods.webViewDidNotHandleTapAsClickAtPoint)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:m_uiDelegate.m_webView didNotHandleTapAsClickAtPoint:point];
}
#endif

#if ENABLE(VIDEO)
void UIDelegate::UIClient::mediaDocumentNaturalSizeChanged(const WebCore::IntSize& newSize)
{
    if (!m_uiDelegate.m_delegateMethods.webViewMediaDocumentNaturalSizeChanged)
        return;

    auto delegate = m_uiDelegate.m_delegate.get();
    if (!delegate)
        return;

    [static_cast<id <WKUIDelegatePrivate>>(delegate) _webView:m_uiDelegate.m_webView mediaDocumentNaturalSizeChanged:newSize];
}
#endif

} // namespace WebKit

#endif // WK_API_ENABLED
