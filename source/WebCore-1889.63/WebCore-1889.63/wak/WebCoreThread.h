/*
 *  WebCoreThread.h
 *  WebCore
 *
 *  Copyright (C) 2006, 2007, 2008, 2009 Apple Inc.  All rights reserved.
 */

#ifndef WebCoreThread_h
#define WebCoreThread_h

#import <CoreGraphics/CoreGraphics.h>
#import <fenv.h>

#if defined(__cplusplus)
extern "C" {
#endif    
        
typedef struct {
    CGContextRef currentCGContext;
} WebThreadContext;
    
extern volatile bool webThreadShouldYield;

extern fenv_t mainThreadFEnv;

#ifdef __OBJC__
@class NSRunLoop;
#else
class NSRunLoop;
#endif

// The lock is automatically freed at the bottom of the runloop. No need to unlock.
// Note that calling this function may hang your UI for several seconds. Don't use
// unless you have to.
void WebThreadLock(void);
    
// This is a no-op for compatibility only. It will go away. Please don't use.
void WebThreadUnlock(void);
    
// Please don't use anything below this line unless you know what you are doing. If unsure, ask.
// ---------------------------------------------------------------------------------------------
bool WebThreadIsLocked(void);
bool WebThreadIsLockedOrDisabled(void);
    
void WebThreadLockPushModal(void);
void WebThreadLockPopModal(void);

void WebThreadEnable(void);
bool WebThreadIsEnabled(void);
bool WebThreadIsCurrent(void);
bool WebThreadNotCurrent(void);
    
// These are for <rdar://problem/6817341> Many apps crashing calling -[UIFieldEditor text] in secondary thread
// Don't use them to solve any random problems you might have.
void WebThreadLockFromAnyThread();
void WebThreadLockFromAnyThreadNoLog();
void WebThreadUnlockFromAnyThread();

// This is for <rdar://problem/8005192> Mail entered a state where message subject and content isn't displayed.
// It should only be used for MobileMail to work around <rdar://problem/8005192>.
void WebThreadUnlockGuardForMail();

static inline bool WebThreadShouldYield(void) { return webThreadShouldYield; }
static inline void WebThreadSetShouldYield() { webThreadShouldYield = true; }

CFRunLoopRef WebThreadRunLoop(void);
NSRunLoop* WebThreadNSRunLoop(void);
WebThreadContext *WebThreadCurrentContext(void);
bool WebThreadContextIsCurrent(void);

void WebThreadSetDelegateSourceRunLoopMode(CFStringRef mode);

#if defined(__cplusplus)
}
#endif

#endif // WebCoreThread_h
