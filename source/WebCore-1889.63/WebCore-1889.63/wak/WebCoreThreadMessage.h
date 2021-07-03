/*
 *  WebCoreThreadMessage.h
 *  WebCore
 *
 *  Copyright (C) 2006, 2007, 2008, 2009 Apple Inc.  All rights reserved.
 *
 */

#ifndef WebCoreThreadMessage_h
#define WebCoreThreadMessage_h

#import <Foundation/Foundation.h>

#ifdef __OBJC__
#import "WebCoreThread.h"
#endif // __OBJC__

#if defined(__cplusplus)
extern "C" {
#endif    

//
// Release an object on the main thread.
//
@interface NSObject(WebCoreThreadAdditions)
- (void)releaseOnMainThread;
@end

// Register a class for deallocation on the WebThread
void WebCoreObjCDeallocOnWebThread(Class cls);
void WebCoreObjCDeallocWithWebThreadLock(Class cls);

// Asynchronous from main thread to web thread.
void WebThreadAdoptAndRelease(id obj);

// Synchronous from web thread to main thread, or main thread to main thread.
void WebThreadCallDelegate(NSInvocation *invocation);
void WebThreadRunOnMainThread(void (^)(void));

// Asynchronous from web thread to main thread, but synchronous when called on the main thread.
void WebThreadCallDelegateAsync(NSInvocation *invocation);

// Asynchronous from web thread to main thread, but synchronous when called on the main thread.
void WebThreadPostNotification(NSString *name, id object, id userInfo);

// Convenience method for making an NSInvocation object
NSInvocation *WebThreadMakeNSInvocation(id target, SEL selector);

#if defined(__cplusplus)
}
#endif

#endif // WebCoreThreadMessage_h
