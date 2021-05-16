/*
 *  WebCoreThreadMessage.h
 *  WebCore
 *
 *  Copyright (C) 2006, 2007, 2008, Apple Inc.  All rights reserved.
 *
 */

#import <Foundation/Foundation.h>

#ifdef __OBJC__
#import <WebCore/WebCoreThread.h>
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

// Asynchronous from main thread to web thread.
void WebThreadCallAPI(NSInvocation *invocation);
void WebThreadAdoptAndRelease(id obj);

// Synchronous from web thread to main thread, or main thread to main thread.
void WebThreadCallDelegate(NSInvocation *invocation);
void WebThreadPostNotification(NSString *name, id object, id userInfo);

// Asynchronous from web thread to main thread, but synchronous when called on the main thread.
void WebThreadCallDelegateAsync(NSInvocation *invocation);
void WebThreadPostNotificationAsync(NSString *name, id object, id userInfo);

// Convenience method for creating an NSInvocation object
NSInvocation *WebThreadCreateNSInvocation(id target, SEL selector);

#if defined(__cplusplus)
}
#endif
