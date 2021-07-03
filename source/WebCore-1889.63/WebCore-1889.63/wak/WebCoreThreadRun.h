/*
 *  WebCoreThreadRun.h
 *  WebCore
 *
 *  Copyright (C) 2010 Apple Inc.  All rights reserved.
 *
 */

#ifndef WebCoreThreadRun_h
#define WebCoreThreadRun_h

#if defined(__cplusplus)
extern "C" {
#endif


// On the web thread, both
//   WebThreadRun(^{ code; });
// and
//   WebThreadRunSync(^{ code; });
// just run the block immediately.

// On any other thread,
//   WebThreadRun(^{ code; });
// will queue the block for asynchronous execution on the web thread, and
//   WebThreadRunSync(^{ code; });
// will queue the block and wait for its execution to finish.

void WebThreadRun(void (^block)());
void WebThreadRunSync(void (^block)());

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // WebCoreThreadRun_h
