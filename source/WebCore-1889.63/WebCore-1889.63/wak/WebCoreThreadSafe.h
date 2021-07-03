/*
 *  WebCoreThreadSafe.h
 *  WebCore
 *
 *  Copyright (C) 2006, 2007, 2008, 2009 Apple Inc.  All rights reserved.
 */

#ifndef WebCoreThreadSafe_h
#define WebCoreThreadSafe_h

#import <Foundation/Foundation.h>

@class WebEvent;

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
- (void)sendEvent:(WebEvent *)event;
@end

@protocol WebPolicyDecisionListener <NSObject>
- (void)use;
@end

@interface WebScriptObject : NSObject
@end

@interface DOMObject : WebScriptObject
@end

#endif // WebCoreThreadSafe_h
