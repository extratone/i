/*
 *  WKContentObservation.c
 *  WebCore
 *
 *  Copyright (C) 2007, 2008, Apple Inc.  All rights reserved.
 *
 */

#if PLATFORM(IOS)

#include "config.h"
#include "WKContentObservation.h"

#include "JSDOMBinding.h"
#include "WebCoreThread.h"
#include <wtf/HashMap.h>

WKContentChange _WKContentChange                    = WKContentNoChange;
bool            _WKObservingContentChanges          = false;
bool            _WKObservingIndeterminateChanges    = false;

using namespace WTF;

bool WKObservingContentChanges(void)
{
    return _WKObservingContentChanges;
}

void WKStopObservingContentChanges(void)
{
    _WKObservingContentChanges = false;
    _WKObservingIndeterminateChanges = false;
}

void WKBeginObservingContentChanges(bool allowsIntedeterminateChanges)
{
    _WKContentChange = WKContentNoChange;
    _WKObservingContentChanges = true;
    
    _WKObservingIndeterminateChanges = allowsIntedeterminateChanges;
    if (_WKObservingIndeterminateChanges)
        WebThreadClearObservedContentModifiers();
}

WKContentChange WKObservedContentChange(void)
{
    return _WKContentChange;
}

void WKSetObservedContentChange(WKContentChange aChange)
{
    if (aChange > _WKContentChange && (_WKObservingIndeterminateChanges || aChange != WKContentIndeterminateChange)) {
        _WKContentChange = aChange;
        if (_WKContentChange == WKContentVisibilityChange)
            WebThreadClearObservedContentModifiers();
    }
}

static HashMap<void *, void *> * WebThreadGetObservedContentModifiers()
{
    ASSERT(WebThreadIsLockedOrDisabled());
    typedef HashMap<void *, void *> VoidVoidMap;
    DEFINE_STATIC_LOCAL(VoidVoidMap, observedContentModifiers, ());
    return &observedContentModifiers;
}

int WebThreadCountOfObservedContentModifiers(void)
{
    return WebThreadGetObservedContentModifiers()->size();
}

void WebThreadClearObservedContentModifiers()
{
    WebThreadGetObservedContentModifiers()->clear();
}

bool WebThreadContainsObservedContentModifier(void * aContentModifier)
{
    return WebThreadGetObservedContentModifiers()->contains(aContentModifier);
}

void WebThreadAddObservedContentModifier(void * aContentModifier)
{
    if (_WKContentChange != WKContentVisibilityChange && _WKObservingIndeterminateChanges)
        WebThreadGetObservedContentModifiers()->set(aContentModifier, aContentModifier);
}

void WebThreadRemoveObservedContentModifier(void * aContentModifier)
{
    WebThreadGetObservedContentModifiers()->remove(aContentModifier);
}

#endif
