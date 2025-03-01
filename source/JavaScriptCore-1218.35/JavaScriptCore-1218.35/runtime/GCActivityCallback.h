/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GCActivityCallback_h
#define GCActivityCallback_h

#include "HeapTimer.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace JSC {

class Heap;

class GCActivityCallback : public HeapTimer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual void didAllocate(size_t) { }
    virtual void willCollect() { }
    virtual void cancel() { }
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

#if PLATFORM(IOS)
    static bool s_shouldCreateGCTimer;
#endif // PLATFORM(IOS)

protected:
#if USE(CF)
    GCActivityCallback(VM* vm, CFRunLoopRef runLoop)
        : HeapTimer(vm, runLoop)
        , m_enabled(true)
    {
    }
#elif PLATFORM(EFL)
    GCActivityCallback(VM* vm, bool flag)
        : HeapTimer(vm)
        , m_enabled(flag)
    {
    }
#else
    GCActivityCallback(VM* vm)
        : HeapTimer(vm)
        , m_enabled(true)
    {
    }
#endif

    bool m_enabled;
};

class DefaultGCActivityCallback : public GCActivityCallback {
public:
    static PassOwnPtr<DefaultGCActivityCallback> create(Heap*);

    DefaultGCActivityCallback(Heap*);
#if PLATFORM(IOS)
    JS_EXPORT_PRIVATE virtual void didAllocate(size_t);
    JS_EXPORT_PRIVATE virtual void willCollect();
    JS_EXPORT_PRIVATE virtual void cancel();
    
    JS_EXPORT_PRIVATE virtual void doWork();
#else
    virtual void didAllocate(size_t);
    virtual void willCollect();
    virtual void cancel();
    
    virtual void doWork();
#endif

#if USE(CF)
protected:
#if PLATFORM(IOS)
    JS_EXPORT_PRIVATE DefaultGCActivityCallback(Heap*, CFRunLoopRef);
#else
    DefaultGCActivityCallback(Heap*, CFRunLoopRef);
#endif // PLATFORM(IOS)
#endif
#if USE(CF) || PLATFORM(QT) || PLATFORM(EFL)
protected:
    void cancelTimer();
    void scheduleTimer(double);

private:
    double m_delay;
#endif
};

inline PassOwnPtr<DefaultGCActivityCallback> DefaultGCActivityCallback::create(Heap* heap)
{
#if PLATFORM(IOS)
    // Big hack.
    if (GCActivityCallback::s_shouldCreateGCTimer)
        return adoptPtr(new DefaultGCActivityCallback(heap));
    return nullptr;
#else
    return adoptPtr(new DefaultGCActivityCallback(heap));
#endif // PLATFORM(IOS)
}

}

#endif
