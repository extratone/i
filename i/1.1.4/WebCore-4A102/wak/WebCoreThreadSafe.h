/*
 *  WebCoreThreadSafe.h
 *  WebCore
 *
 *  Copyright (C) 2006, 2007, Apple Inc.  All rights reserved.
 *
 */

#import <Foundation/Foundation.h>
#import <GraphicsServices/GraphicsServices.h>

// Listing of methods that are currently thread-safe.

@interface WAKView : NSObject
@end

@interface WebView : WAKView
- (void)reload:(id)sender;
- (void)stopLoading:(id)sender;
- (BOOL)canGoBack;
- (void)goBack;
- (BOOL)canGoForward;
- (void)goForward;
@end

@interface WebFrame : NSObject
- (void)loadRequest:(NSURLRequest *)request;
@end

@interface WAKResponder : NSObject
@end

@interface WAKWindow : WAKResponder
- (void)sendEvent:(GSEventRef)event;
@end

@protocol WebPolicyDecisionListener <NSObject>
- (void)use;
@end

@interface WebCoreFrameBridge : NSObject
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture;
- (void)insertText:(NSString *)aStringValue selectInsertedText:(BOOL)aFlag;
@end

@interface WebScriptObject : NSObject
@end

@interface DOMObject : WebScriptObject
@end

@interface DOMNode : DOMObject
- (void)simulateCompleteClick;
@end

