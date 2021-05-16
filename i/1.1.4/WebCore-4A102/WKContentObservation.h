/*
 *  WKContentObservation.h
 *  WebCore
 *
 *  Copyright (C) 2007, Apple Inc.  All rights reserved.
 *
 */


#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    WKContentNoChange               = 0,
    WKContentVisibilityChange       = 2,
    WKContentIndeterminateChange    = 1
}   WKContentChange;

bool WKObservingContentChanges(void);
bool WKObservingIndeterminateContentChanges(void);

void WKStopObservingContentChanges(void);
void WKBeginObservingContentChanges(bool allowsIntedeterminateChanges);

WKContentChange WKObservedContentChange(void);
void WKSetObservedContentChange(WKContentChange aChange);

int WebThreadCountOfObservedContentModifiers(void);
void WebThreadClearObservedContentModifiers(void);

bool WebThreadContainsObservedContentModifier(void * aContentModifier);
void WebThreadAddObservedContentModifier(void * aContentModifier);
void WebThreadRemoveObservedContentModifier(void * aContentModifier);

#ifdef __cplusplus
}
#endif

