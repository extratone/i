/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "WKViewPrivate.h"

#import "APIObject.h"
#import "PluginComplexTextInputState.h"
#import "SameDocumentNavigationType.h"
#import "WebFindOptions.h"
#import "WebHitTestResult.h"
#import <wtf/Forward.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

@class WKWebViewConfiguration;

namespace API {
class Object;
}

namespace IPC {
class DataReference;
}

namespace WebCore {
class Image;
class SharedBuffer;
class TextIndicator;
struct KeypressCommand;
}

namespace WebKit {
class DrawingAreaProxy;
class LayerTreeContext;
class ViewSnapshot;
class WebProcessPool;
struct ColorSpaceData;
struct EditorState;
struct WebPageConfiguration;
}

@class WKFullScreenWindowController;
@class WKWebView;
#if WK_API_ENABLED
@class _WKThumbnailView;
#endif

@interface WKView ()
#if WK_API_ENABLED
- (instancetype)initWithFrame:(CGRect)frame processPool:(WebKit::WebProcessPool&)processPool configuration:(WebKit::WebPageConfiguration)webPageConfiguration webView:(WKWebView *)webView;
#endif

- (std::unique_ptr<WebKit::DrawingAreaProxy>)_createDrawingAreaProxy;
- (BOOL)_isFocused;
- (void)_processDidExit;
- (void)_pageClosed;
- (void)_didRelaunchProcess;
- (void)_preferencesDidChange;
- (void)_toolTipChangedFrom:(NSString *)oldToolTip to:(NSString *)newToolTip;
- (void)_setUserInterfaceItemState:(NSString *)commandName enabled:(BOOL)isEnabled state:(int)newState;
- (void)_doneWithKeyEvent:(NSEvent *)event eventWasHandled:(BOOL)eventWasHandled;
- (bool)_executeSavedCommandBySelector:(SEL)selector;
- (void)_setIntrinsicContentSize:(NSSize)intrinsicContentSize;
- (NSRect)_convertToDeviceSpace:(NSRect)rect;
- (NSRect)_convertToUserSpace:(NSRect)rect;
- (void)_setTextIndicator:(WebCore::TextIndicator&)textIndicator;
- (void)_setTextIndicator:(WebCore::TextIndicator&)textIndicator withLifetime:(WebCore::TextIndicatorLifetime)lifetime;
- (void)_clearTextIndicatorWithAnimation:(WebCore::TextIndicatorDismissalAnimation)animation;
- (void)_setTextIndicatorAnimationProgress:(float)progress;
- (void)_selectionChanged;

- (void)_setAcceleratedCompositingModeRootLayer:(CALayer *)rootLayer;
- (CALayer *)_acceleratedCompositingModeRootLayer;

- (PassRefPtr<WebKit::ViewSnapshot>)_takeViewSnapshot;
- (void)_wheelEventWasNotHandledByWebCore:(NSEvent *)event;

- (void)_setAccessibilityWebProcessToken:(NSData *)data;

- (void)_pluginFocusOrWindowFocusChanged:(BOOL)pluginHasFocusAndWindowHasFocus pluginComplexTextInputIdentifier:(uint64_t)pluginComplexTextInputIdentifier;
- (void)_setPluginComplexTextInputState:(WebKit::PluginComplexTextInputState)pluginComplexTextInputState pluginComplexTextInputIdentifier:(uint64_t)pluginComplexTextInputIdentifier;
- (void)_dragImageForView:(NSView *)view withImage:(NSImage *)image at:(NSPoint)clientPoint linkDrag:(BOOL)linkDrag;
- (void)_setPromisedDataForImage:(WebCore::Image *)image withFileName:(NSString *)filename withExtension:(NSString *)extension withTitle:(NSString *)title withURL:(NSString *)url withVisibleURL:(NSString *)visibleUrl withArchive:(WebCore::SharedBuffer*) archiveBuffer forPasteboard:(NSString *)pasteboardName;
#if ENABLE(ATTACHMENT_ELEMENT)
- (void)_setPromisedDataForAttachment:(NSString *)filename withExtension:(NSString *)extension withTitle:(NSString *)title withURL:(NSString *)url withVisibleURL:(NSString *)visibleUrl forPasteboard:(NSString *)pasteboardName;
#endif
- (void)_updateSecureInputState;
- (void)_resetSecureInputState;
- (void)_notifyInputContextAboutDiscardedComposition;

- (WebKit::ColorSpaceData)_colorSpace;

- (NSInteger)spellCheckerDocumentTag;
- (void)handleAcceptedAlternativeText:(NSString*)text;

- (void)_setSuppressVisibilityUpdates:(BOOL)suppressVisibilityUpdates;
- (BOOL)_suppressVisibilityUpdates;

- (void)_didFirstVisuallyNonEmptyLayoutForMainFrame;
- (void)_didFinishLoadForMainFrame;
- (void)_didFailLoadForMainFrame;
- (void)_didSameDocumentNavigationForMainFrame:(WebKit::SameDocumentNavigationType)type;
- (void)_removeNavigationGestureSnapshot;

#if WK_API_ENABLED
@property (nonatomic, setter=_setThumbnailView:) _WKThumbnailView *_thumbnailView;
- (void)_reparentLayerTreeInThumbnailView;
#endif

- (void)_windowDidOrderOnScreen:(NSNotification *)notification;
- (void)_windowDidOrderOffScreen:(NSNotification *)notification;

- (void)_addFontPanelObserver;
// FullScreen

@property (readonly) BOOL _hasFullScreenWindowController;
@property (readonly) WKFullScreenWindowController *_fullScreenWindowController;
- (void)_closeFullScreenWindowController;

- (void)_prepareForDictionaryLookup;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
- (void)_didPerformImmediateActionHitTest:(const WebKit::WebHitTestResult::Data&)hitTestResult contentPreventsDefault:(BOOL)contentPreventsDefault userData:(API::Object*)userData;
#endif

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
- (void)_startWindowDrag;
#endif

@property (nonatomic, retain, setter=_setPrimaryTrackingArea:) NSTrackingArea *_primaryTrackingArea;

@property (readonly) NSWindow *_targetWindowForMovePreparation;

// For WKViewLayoutStrategy and subclasses:
- (void)_setDrawingAreaSize:(NSSize)size;
- (void)_updateViewExposedRect;
- (CALayer *)_rootLayer;

@end
