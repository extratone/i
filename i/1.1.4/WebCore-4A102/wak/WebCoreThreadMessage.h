/*
 *  WebCoreThreadMessage.h
 *  WebCore
 *
 *  Copyright (C) 2006, 2007, Apple Inc.  All rights reserved.
 *
 */

#import <Foundation/Foundation.h>

#ifdef __OBJC__
#import <WebCore/WebCoreThread.h>
#define WebThreadAdoptAndReleaseIfNeeded \
    if (WebThreadIsEnabled() && WebThreadNotCurrent() && [self retainCount] == 1) \
        WebThreadAdoptAndRelease(self); \
    else
//      [super release];  // code using this macro should call this method.
#endif // __OBJC__

#if defined(__cplusplus)
extern "C" {
#endif    

// Asynchronous from main thread to web thread.
void WebThreadCallAPI(id target, SEL selector, ...);
void WebThreadAdoptAndRelease(id obj);

// Synchronous from web thread to main thread.
void WebThreadCallDelegate(id target, SEL selector, ...);
void WebThreadPostNotification(NSString *name, id object, id userInfo);

#if defined(__cplusplus)
}
#endif
