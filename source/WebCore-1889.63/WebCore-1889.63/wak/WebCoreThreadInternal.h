/*
 *  WebCoreThreadInternal.h
 *  WebCore
 *
 *  Copyright (C) 2011 Apple Inc.  All rights reserved.
 */

#ifndef WebCoreThreadInternal_h
#define WebCoreThreadInternal_h

#include "WebCoreThread.h"

#if defined(__cplusplus)
extern "C" {
#endif    

// Sometimes, like for the Inspector, we need to pause the execution of a current run
// loop iteration and resume it later. This handles pushing and popping the autorelease
// pools to keep the original pool unaffected by the run loop observers. The
// WebThreadLock is released when calling Enable, and acquired when calling Disable.
// NOTE: Does not expect arbitrary nesting, only 1 level of nesting.
void WebRunLoopEnableNested();
void WebRunLoopDisableNested();

void WebThreadInitRunQueue();

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // WebCoreThreadInternal_h
