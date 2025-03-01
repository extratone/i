/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#ifndef ThreadGlobalData_h
#define ThreadGlobalData_h

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/StringHash.h>

#if ENABLE(WORKERS)
#include <wtf/ThreadSpecific.h>
#include <wtf/Threading.h>
using WTF::ThreadSpecific;
#endif

namespace WebCore {

    class EventNames;
    class ThreadLocalInspectorCounters;
    class ThreadTimers;

    struct CachedResourceRequestInitiators;
    struct ICUConverterWrapper;
    struct TECConverterWrapper;

    class ThreadGlobalData {
        WTF_MAKE_NONCOPYABLE(ThreadGlobalData);
    public:
        ThreadGlobalData();
        ~ThreadGlobalData();
        void destroy(); // called on workers to clean up the ThreadGlobalData before the thread exits.

        const CachedResourceRequestInitiators& cachedResourceRequestInitiators() { return *m_cachedResourceRequestInitiators; }
        EventNames& eventNames() { return *m_eventNames; }
        ThreadTimers& threadTimers() { return *m_threadTimers; }

#if USE(ICU_UNICODE)
        ICUConverterWrapper& cachedConverterICU() { return *m_cachedConverterICU; }
#endif

#if PLATFORM(MAC) && !PLATFORM(IOS)
        TECConverterWrapper& cachedConverterTEC() { return *m_cachedConverterTEC; }
#endif

#if ENABLE(WORKERS) && PLATFORM(IOS)
        void setWebCoreThreadData();
#endif

#if ENABLE(INSPECTOR)
        ThreadLocalInspectorCounters& inspectorCounters() { return *m_inspectorCounters; }
#endif

    private:
        OwnPtr<CachedResourceRequestInitiators> m_cachedResourceRequestInitiators;
        OwnPtr<EventNames> m_eventNames;
        OwnPtr<ThreadTimers> m_threadTimers;

#ifndef NDEBUG
        bool m_isMainThread;
#endif

#if USE(ICU_UNICODE)
        OwnPtr<ICUConverterWrapper> m_cachedConverterICU;
#endif

#if PLATFORM(MAC) && !PLATFORM(IOS)
        OwnPtr<TECConverterWrapper> m_cachedConverterTEC;
#endif

#if ENABLE(INSPECTOR)
        OwnPtr<ThreadLocalInspectorCounters> m_inspectorCounters;
#endif

#if ENABLE(WORKERS)
        static ThreadSpecific<ThreadGlobalData>* staticData;
#if PLATFORM(IOS)
        static ThreadGlobalData* sharedMainThreadStaticData;
#endif
#else
        static ThreadGlobalData* staticData;
#endif
        friend ThreadGlobalData& threadGlobalData();
    };

inline ThreadGlobalData& threadGlobalData() 
{
    // FIXME: Workers are not necessarily the only feature that make per-thread global data necessary.
    // We need to check for e.g. database objects manipulating strings on secondary threads.

#if ENABLE(WORKERS)
#if PLATFORM(IOS)
    if (UNLIKELY(!ThreadGlobalData::staticData)) {
        ThreadGlobalData::staticData = new ThreadSpecific<ThreadGlobalData>;
        // WebThread and main UI thread need to share the same object. Save it in a static
        // here, the WebThread will pick it up in setWebCoreThreadData().
        if (pthread_main_np())
            ThreadGlobalData::sharedMainThreadStaticData = *ThreadGlobalData::staticData;
    }
    return **ThreadGlobalData::staticData;
#else
    // ThreadGlobalData is used on main thread before it could possibly be used on secondary ones, so there is no need for synchronization here.
    if (!ThreadGlobalData::staticData)
        ThreadGlobalData::staticData = new ThreadSpecific<ThreadGlobalData>;
    return **ThreadGlobalData::staticData;
#endif
#else
    if (!ThreadGlobalData::staticData) {
        ThreadGlobalData::staticData = static_cast<ThreadGlobalData*>(fastMalloc(sizeof(ThreadGlobalData)));
        // ThreadGlobalData constructor indirectly uses staticData, so we need to set up the memory before invoking it.
        new (NotNull, ThreadGlobalData::staticData) ThreadGlobalData;
    }
    return *ThreadGlobalData::staticData;
#endif
}
    
} // namespace WebCore

#endif // ThreadGlobalData_h
