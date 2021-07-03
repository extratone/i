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

#import <WebKit/WKUIDelegate.h>

#if WK_API_ENABLED

#import <WebKit/WKSecurityOrigin.h>
#import <WebKit/_WKActivatedElementInfo.h>

@class UIScrollView;
@class UIViewController;
@class _WKActivatedElementInfo;
@class _WKElementAction;
@class _WKFrameHandle;

@protocol WKUIDelegatePrivate <WKUIDelegate>

struct UIEdgeInsets;

@optional

// FIXME: This should be handled by the WKWebsiteDataStore delegate.
- (void)_webView:(WKWebView *)webView decideDatabaseQuotaForSecurityOrigin:(WKSecurityOrigin *)securityOrigin currentQuota:(unsigned long long)currentQuota currentOriginUsage:(unsigned long long)currentOriginUsage currentDatabaseUsage:(unsigned long long)currentUsage expectedUsage:(unsigned long long)expectedUsage decisionHandler:(void (^)(unsigned long long newQuota))decisionHandler;

// FIXME: This should be handled by the WKWebsiteDataStore delegate.
- (void)_webView:(WKWebView *)webView decideWebApplicationCacheQuotaForSecurityOrigin:(WKSecurityOrigin *)securityOrigin currentQuota:(unsigned long long)currentQuota totalBytesNeeded:(unsigned long long)totalBytesNeeded decisionHandler:(void (^)(unsigned long long newQuota))decisionHandler;

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame;

- (void)_webViewClose:(WKWebView *)webView;
- (void)_webViewFullscreenMayReturnToInline:(WKWebView *)webView;
- (void)_webViewDidEnterFullscreen:(WKWebView *)webView WK_AVAILABLE(WK_MAC_TBA, 8_3);
- (void)_webViewDidExitFullscreen:(WKWebView *)webView WK_AVAILABLE(WK_MAC_TBA, 8_3);
- (void)_webView:(WKWebView *)webView mediaDocumentNaturalSizeChanged:(CGSize)size;

#if TARGET_OS_IPHONE
- (BOOL)_webView:(WKWebView *)webView shouldIncludeAppLinkActionsForElement:(_WKActivatedElementInfo *)element WK_AVAILABLE(NA, WK_IOS_TBA);
- (NSArray *)_webView:(WKWebView *)webView actionsForElement:(_WKActivatedElementInfo *)element defaultActions:(WK_ARRAY(_WKElementAction *) *)defaultActions;
- (void)_webView:(WKWebView *)webView didNotHandleTapAsClickAtPoint:(CGPoint)point;
- (BOOL)_webView:(WKWebView *)webView shouldRequestGeolocationAuthorizationForURL:(NSURL *)url isMainFrame:(BOOL)isMainFrame mainFrameURL:(NSURL *)mainFrameURL;
- (UIViewController *)_webView:(WKWebView *)webView previewViewControllerForURL:(NSURL *)url WK_AVAILABLE(NA, WK_IOS_TBA);
- (void)_webView:(WKWebView *)webView commitPreviewedViewController:(UIViewController *)previewedViewController WK_AVAILABLE(NA, WK_IOS_TBA);
- (void)_webView:(WKWebView *)webView willPreviewImageWithURL:(NSURL *)imageURL WK_AVAILABLE(NA, WK_IOS_TBA);
- (void)_webView:(WKWebView *)webView commitPreviewedImageWithURL:(NSURL *)imageURL WK_AVAILABLE(NA, WK_IOS_TBA);
- (void)_webView:(WKWebView *)webView didDismissPreviewViewController:(UIViewController *)previewedViewController committing:(BOOL)committing WK_AVAILABLE(NA, WK_IOS_TBA);
- (void)_webView:(WKWebView *)webView didDismissPreviewViewController:(UIViewController *)previewedViewController WK_AVAILABLE(NA, WK_IOS_TBA);
- (UIEdgeInsets)_webView:(WKWebView *)webView finalObscuredInsetsForScrollView:(UIScrollView *)scrollView withVelocity:(CGPoint)velocity targetContentOffset:(inout CGPoint *)targetContentOffset WK_AVAILABLE(NA, WK_IOS_TBA);
- (UIViewController *)_webView:(WKWebView *)webView previewViewControllerForURL:(NSURL *)url defaultActions:(WK_ARRAY(_WKElementAction *) *)actions elementInfo:(_WKActivatedElementInfo *)elementInfo WK_AVAILABLE(NA, WK_IOS_TBA);
- (UIViewController *)_webView:(WKWebView *)webView previewViewControllerForAnimatedImageAtURL:(NSURL *)url defaultActions:(WK_ARRAY(_WKElementAction *) *)actions elementInfo:(_WKActivatedElementInfo *)elementInfo imageSize:(CGSize)imageSize WK_AVAILABLE(NA, WK_IOS_TBA);
#endif

@end

#endif // WK_API_ENABLED
