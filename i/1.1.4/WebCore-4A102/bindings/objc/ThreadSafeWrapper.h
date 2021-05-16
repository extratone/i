//
//  WebThreadSafeWrapper.h
//  WebCore
//
//  Copyright (C) 2006, 2007, Apple Inc.  All rights reserved.
//

#define USE_THREADSADE_NODES 0

#if USE_THREADSAFE_NODES

struct WKWindow;

@interface ThreadSafeWrapper : NSProxy
{
    id              _object;
}

+ (ThreadSafeWrapper *)threadSafeWrapperWithObject:(id)anObject;
- (id)initWithObject:(id)anObject;
- (BOOL)isKindOfClass:(Class)aClass;

@end

#endif
