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

#import <WebKit/WKWebView.h>

#if WK_API_ENABLED

#import <WebKit/_WKFindOptions.h>
#import <WebKit/_WKLayoutMode.h>
#import <WebKit/_WKRenderingProgressEvents.h>

typedef NS_ENUM(NSInteger, _WKPaginationMode) {
    _WKPaginationModeUnpaginated,
    _WKPaginationModeLeftToRight,
    _WKPaginationModeRightToLeft,
    _WKPaginationModeTopToBottom,
    _WKPaginationModeBottomToTop,
} WK_ENUM_AVAILABLE(10_10, 8_0);

WK_EXTERN NSString * const _WKShouldOpenExternalURLsKey WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

@class WKBrowsingContextHandle;
@class _WKRemoteObjectRegistry;
@class _WKSessionState;
@class _WKWebViewPrintFormatter;

@protocol WKHistoryDelegatePrivate;
@protocol _WKDiagnosticLoggingDelegate;
@protocol _WKFindDelegate;
@protocol _WKFormDelegate;

@interface WKWebView (WKPrivate)

// FIXME: This should return a _WKRemoteObjectRegistry *.
@property (nonatomic, readonly) id _remoteObjectRegistry;
@property (nonatomic, readonly) WKBrowsingContextHandle *_handle;

@property (nonatomic, setter=_setObservedRenderingProgressEvents:) _WKRenderingProgressEvents _observedRenderingProgressEvents;

@property (nonatomic, weak, setter=_setHistoryDelegate:) id <WKHistoryDelegatePrivate> _historyDelegate;

@property (nonatomic, readonly) NSURL *_unreachableURL;

- (void)_loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL;

@property (nonatomic, readonly) NSArray *_certificateChain WK_DEPRECATED(10_10, WK_MAC_TBA, 8_0, WK_IOS_TBA, "use -certificateChain");
@property (nonatomic, readonly) NSURL *_committedURL;
@property (nonatomic, readonly) NSString *_MIMEType;
@property (nonatomic, readonly) NSString *_userAgent WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

@property (copy, setter=_setApplicationNameForUserAgent:) NSString *_applicationNameForUserAgent;
@property (copy, setter=_setCustomUserAgent:) NSString *_customUserAgent;

@property (nonatomic, setter=_setUserContentExtensionsEnabled:) BOOL _userContentExtensionsEnabled WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

@property (nonatomic, readonly) pid_t _webProcessIdentifier;

@property (nonatomic, getter=_isEditable, setter=_setEditable:) BOOL _editable WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

// FIXME: Remove these once nobody is using them.
@property (nonatomic, readonly) NSData *_sessionStateData;
- (void)_restoreFromSessionStateData:(NSData *)sessionStateData;

@property (nonatomic, readonly) _WKSessionState *_sessionState;
- (WKNavigation *)_restoreSessionState:(_WKSessionState *)sessionState andNavigate:(BOOL)navigate;

@property (nonatomic, setter=_setAllowsRemoteInspection:) BOOL _allowsRemoteInspection;

@property (nonatomic, setter=_setAddsVisitedLinks:) BOOL _addsVisitedLinks;

@property (nonatomic, readonly) BOOL _networkRequestsInProgress;

@property (nonatomic, readonly, getter=_isShowingNavigationGestureSnapshot) BOOL _showingNavigationGestureSnapshot;

- (void)_close;

@property (nonatomic, setter=_setLayoutMode:) _WKLayoutMode _layoutMode;
// For use with _layoutMode = _WKLayoutModeFixedSize:
@property (nonatomic, setter=_setFixedLayoutSize:) CGSize _fixedLayoutSize;
// For use with _layoutMode = _WKLayoutModeDynamicSizeWithMinimumViewSize:
@property (nonatomic, setter=_setMinimumViewSize:) CGSize _minimumViewSize;

@property (nonatomic, setter=_setViewScale:) CGFloat _viewScale;

#if TARGET_OS_IPHONE
// DERECATED: The setters of the three following function are deprecated, please use overrideLayoutParameters.
// Define the smallest size a page take with a regular viewport.
@property (nonatomic, readonly) CGSize _minimumLayoutSizeOverride;
// Define the largest size the unobscured area can get for the current view bounds. This value is used to define viewport units.
@property (nonatomic, readonly) CGSize _maximumUnobscuredSizeOverride;

// Define the inset of the scrollview unusable by the web page.
@property (nonatomic, setter=_setObscuredInsets:) UIEdgeInsets _obscuredInsets;

// Override the interface orientation. Clients using _beginAnimatedResizeWithUpdates: must update the interface orientation
// in the update block.
@property (nonatomic, setter=_setInterfaceOrientationOverride:) UIInterfaceOrientation _interfaceOrientationOverride;

@property (nonatomic, setter=_setAllowsViewportShrinkToFit:) BOOL _allowsViewportShrinkToFit;

@property (nonatomic, setter=_setBackgroundExtendsBeyondPage:) BOOL _backgroundExtendsBeyondPage;

// FIXME: Remove these three properties once we expose WKWebViewContentProvider as API.
@property (nonatomic, readonly, getter=_isDisplayingPDF) BOOL _displayingPDF;
@property (nonatomic, readonly) NSData *_dataForDisplayedPDF;
// FIXME: This can be removed once WKNavigation's response property is implemented.
@property (nonatomic, readonly) NSString *_suggestedFilenameForDisplayedPDF;

@property (nonatomic, readonly) _WKWebViewPrintFormatter *_webViewPrintFormatter;

- (void)_beginInteractiveObscuredInsetsChange;
- (void)_endInteractiveObscuredInsetsChange;
- (void)_hideContentUntilNextUpdate;

- (void)_beginAnimatedResizeWithUpdates:(void (^)(void))updateBlock;
- (void)_endAnimatedResize;
- (void)_resizeWhileHidingContentWithUpdates:(void (^)(void))updateBlock;

- (void)_snapshotRect:(CGRect)rectInViewCoordinates intoImageOfWidth:(CGFloat)imageWidth completionHandler:(void(^)(CGImageRef))completionHandler;

// Deprecated: Use [_overrideLayoutParametersWithMinimumLayoutSize:maximumUnobscuredSizeOverride:] instead.
// This function is kept for binary compatibility with iOS 8.0, it can be removed after the bincompat window.
- (void)_overrideLayoutParametersWithMinimumLayoutSize:(CGSize)minimumLayoutSize minimumLayoutSizeForMinimalUI:(CGSize)minimumLayoutSizeForMinimalUI maximumUnobscuredSizeOverride:(CGSize)maximumUnobscuredSizeOverride WK_DEPRECATED(NA, NA, 8_0, WK_IOS_TBA, "use -_overrideLayoutParametersWithMinimumLayoutSize:maximumUnobscuredSizeOverride:");

- (void)_overrideLayoutParametersWithMinimumLayoutSize:(CGSize)minimumLayoutSize maximumUnobscuredSizeOverride:(CGSize)maximumUnobscuredSizeOverride WK_AVAILABLE(NA, WK_IOS_TBA);

- (UIView *)_viewForFindUI;

- (void)_setOverlaidAccessoryViewsInset:(CGSize)inset;

- (void)_killWebContentProcess;
- (void)_didRelaunchProcess;

// Puts the view into a state where being taken out of the view hierarchy and resigning first responder
// will not count as becoming inactive and unfocused. The returned block must be called to exit the state.
- (void (^)(void))_retainActiveFocusedState WK_AVAILABLE(NA, WK_IOS_TBA);

- (void)_becomeFirstResponderWithSelectionMovingForward:(BOOL)selectingForward completionHandler:(void (^)(BOOL didBecomeFirstResponder))completionHandler WK_AVAILABLE(NA, WK_IOS_TBA);

- (id)_snapshotLayerContentsForBackForwardListItem:(WKBackForwardListItem *)item WK_AVAILABLE(NA, WK_IOS_TBA);

#else
@property (readonly) NSColor *_pageExtendedBackgroundColor;
@property (nonatomic, setter=_setDrawsTransparentBackground:) BOOL _drawsTransparentBackground;
@property (nonatomic, setter=_setTopContentInset:) CGFloat _topContentInset;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
@property (nonatomic, setter=_setAutomaticallyAdjustsContentInsets:) BOOL _automaticallyAdjustsContentInsets;
#endif

// Default value is 0. A value of 0 means the window's backing scale factor will be used and automatically update when the window moves screens.
@property (nonatomic, setter=_setOverrideDeviceScaleFactor:) CGFloat _overrideDeviceScaleFactor WK_AVAILABLE(WK_MAC_TBA, NA);
#endif

- (WKNavigation *)_reloadWithoutContentBlockers WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

- (void)_killWebContentProcessAndResetState;

- (void)_getMainResourceDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler;
- (void)_getWebArchiveDataWithCompletionHandler:(void (^)(NSData *, NSError *))completionHandler;

@property (nonatomic, setter=_setPaginationMode:) _WKPaginationMode _paginationMode;
// Whether the column-break-{before,after} properties are respected instead of the
// page-break-{before,after} properties.
@property (nonatomic, setter=_setPaginationBehavesLikeColumns:) BOOL _paginationBehavesLikeColumns;
// Set to 0 to have the page length equal the view length.
@property (nonatomic, setter=_setPageLength:) CGFloat _pageLength;
@property (nonatomic, setter=_setGapBetweenPages:) CGFloat _gapBetweenPages;
@property (readonly) NSUInteger _pageCount;

@property (nonatomic, readonly) BOOL _supportsTextZoom;
@property (nonatomic, setter=_setTextZoomFactor:) double _textZoomFactor;
@property (nonatomic, setter=_setPageZoomFactor:) double _pageZoomFactor;

@property (nonatomic, weak, setter=_setDiagnosticLoggingDelegate:) id <_WKDiagnosticLoggingDelegate> _diagnosticLoggingDelegate WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, weak, setter=_setFindDelegate:) id <_WKFindDelegate> _findDelegate;
- (void)_findString:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount;
- (void)_countStringMatches:(NSString *)string options:(_WKFindOptions)options maxCount:(NSUInteger)maxCount;
- (void)_hideFindUI;

@property (nonatomic, weak, setter=_setFormDelegate:) id <_WKFormDelegate> _formDelegate;

@property (nonatomic, readonly, getter=_isDisplayingStandaloneImageDocument) BOOL _displayingStandaloneImageDocument;
@property (nonatomic, readonly, getter=_isDisplayingStandaloneMediaDocument) BOOL _displayingStandaloneMediaDocument;

@property (nonatomic, setter=_setScrollPerformanceDataCollectionEnabled:) BOOL _scrollPerformanceDataCollectionEnabled WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, readonly) NSArray *_scrollPerformanceData WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

- (WKNavigation *)_loadRequest:(NSURLRequest *)request withOptions:(WK_DICTIONARY(NSString *, id) *)loadOptions WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

- (void)_saveBackForwardSnapshotForItem:(WKBackForwardListItem *)item WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

@property (nonatomic, getter=_allowsMediaDocumentInlinePlayback, setter=_setAllowsMediaDocumentInlinePlayback:) BOOL _allowsMediaDocumentInlinePlayback;

@end

#endif
