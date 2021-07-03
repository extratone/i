//
//  WKUtilities.c
//
//  Copyright (C) 2005, 2006, 2007, 2008 Apple Inc.  All rights reserved.
//

#import "config.h"
#import "WKUtilities.h"

#import <wtf/Assertions.h>

const CFArrayCallBacks WKCollectionArrayCallBacks = { 0, WKCollectionRetain, WKCollectionRelease, NULL, NULL };
const CFSetCallBacks WKCollectionSetCallBacks = { 0, WKCollectionRetain, WKCollectionRelease, NULL, NULL, NULL };

#define LISTEN_FOR_VISIBILITY_CHANGES   1
#define VISIBILITY_DID_CHANGE           2

const void *WKCollectionRetain (CFAllocatorRef allocator, const void *value)
{
    UNUSED_PARAM(allocator);
    return WKRetain (value);
}

const void *WKRetain(const void *o)
{
    WKObjectRef object = (WKObjectRef)o;
    
    object->referenceCount++;
    
    return object;
}

void WKCollectionRelease (CFAllocatorRef allocator, const void *value)
{
    UNUSED_PARAM(allocator);
    WKRelease (value);
}

void WKRelease(const void *o)
{
    WKObjectRef object = (WKObjectRef)o;

    if (object->referenceCount == 0) {
        WKError ("attempt to release invalid object");
        return;
    }
    
    object->referenceCount--;

    if (object->referenceCount == 0) {
        const WKClassInfo *info = object->classInfo;
        while (info) {
            if (info->dealloc)
                info->dealloc ((void *)object);
            info = info->parent;
        }
    }
}

static void _WKObjectDealloc (WKObjectRef v)
{
    free (v);
}

WKClassInfo WKObjectClass = { 0, "WKObject", _WKObjectDealloc };

const void *WKCreateObjectWithSize (size_t size, WKClassInfo *info)
{
    WKObjectRef object = (WKObjectRef)calloc (size, 1);
    if (!object)
        return 0;

    object->classInfo = info;
    
    WKRetain(object);
    
    return object;
}

WTF_ATTRIBUTE_PRINTF(4, 5)
void WKReportError(const char *file, int line, const char *function, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:%d %s:  ", file, line, function);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

CFIndex WKArrayIndexOfValue (CFArrayRef array, const void *value)
{
    CFIndex i, count, index = -1;

    count = CFArrayGetCount (array);
    for (i = 0; i < count; i++) {
        if (CFArrayGetValueAtIndex (array, i) == value) {
            index = i;
            break;
        }
    }
    
    return index;
}

WKClassInfo *WKGetClassInfo (WKObjectRef object)
{
    return object->classInfo;
}
