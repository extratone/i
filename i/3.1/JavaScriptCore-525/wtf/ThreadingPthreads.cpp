/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
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
#include "config.h"
#include "Threading.h"

#include "StdLibExtras.h"

#if USE(PTHREADS)

#include "CurrentTime.h"
#include "HashMap.h"
#include "MainThread.h"
#include "RandomNumberSeed.h"

#include <errno.h>
#include <limits.h>
#include <sys/time.h>

namespace WTF {

typedef HashMap<ThreadIdentifier, pthread_t> ThreadMap;

static Mutex* atomicallyInitializedStaticMutex;

static ThreadIdentifier mainThreadIdentifier; // The thread that was the first to call initializeThreading(), which must be the main thread.

static Mutex& threadMapMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

void initializeThreading()
{
    if (!atomicallyInitializedStaticMutex) {
        atomicallyInitializedStaticMutex = new Mutex;
        threadMapMutex();
        initializeRandomNumberGenerator();
        mainThreadIdentifier = currentThread();
        initializeMainNSThread();
        initializeMainThread();
    }
}

void lockAtomicallyInitializedStaticMutex()
{
    ASSERT(atomicallyInitializedStaticMutex);
    atomicallyInitializedStaticMutex->lock();
}

void unlockAtomicallyInitializedStaticMutex()
{
    atomicallyInitializedStaticMutex->unlock();
}

static ThreadMap& threadMap()
{
    DEFINE_STATIC_LOCAL(ThreadMap, map, ());
    return map;
}

static ThreadIdentifier identifierByPthreadHandle(const pthread_t& pthreadHandle)
{
    MutexLocker locker(threadMapMutex());

    ThreadMap::iterator i = threadMap().begin();
    for (; i != threadMap().end(); ++i) {
        if (pthread_equal(i->second, pthreadHandle))
            return i->first;
    }

    return 0;
}

static ThreadIdentifier establishIdentifierForPthreadHandle(pthread_t& pthreadHandle)
{
    ASSERT(!identifierByPthreadHandle(pthreadHandle));

    MutexLocker locker(threadMapMutex());

    static ThreadIdentifier identifierCount = 1;

    threadMap().add(identifierCount, pthreadHandle);
    
    return identifierCount++;
}

static pthread_t pthreadHandleForIdentifier(ThreadIdentifier id)
{
    MutexLocker locker(threadMapMutex());
    
    return threadMap().get(id);
}

static void clearPthreadHandleForIdentifier(ThreadIdentifier id)
{
    MutexLocker locker(threadMapMutex());

    ASSERT(threadMap().contains(id));
    
    threadMap().remove(id);
}

ThreadIdentifier createThreadInternal(ThreadFunction entryPoint, void* data, const char*)
{
    pthread_t threadHandle;
    if (pthread_create(&threadHandle, NULL, entryPoint, data)) {
        LOG_ERROR("Failed to create pthread at entry point %p with data %p", entryPoint, data);
        return 0;
    }

    return establishIdentifierForPthreadHandle(threadHandle);
}

int waitForThreadCompletion(ThreadIdentifier threadID, void** result)
{
    ASSERT(threadID);
    
    pthread_t pthreadHandle = pthreadHandleForIdentifier(threadID);
 
    int joinResult = pthread_join(pthreadHandle, result);
    if (joinResult == EDEADLK)
        LOG_ERROR("ThreadIdentifier %u was found to be deadlocked trying to quit", threadID);
        
    clearPthreadHandleForIdentifier(threadID);
    return joinResult;
}

void detachThread(ThreadIdentifier threadID)
{
    ASSERT(threadID);
    
    pthread_t pthreadHandle = pthreadHandleForIdentifier(threadID);
    
    pthread_detach(pthreadHandle);
    
    clearPthreadHandleForIdentifier(threadID);
}

ThreadIdentifier currentThread()
{
    pthread_t currentThread = pthread_self();
    if (ThreadIdentifier id = identifierByPthreadHandle(currentThread))
        return id;
    return establishIdentifierForPthreadHandle(currentThread);
}

bool isMainThread()
{
    return currentThread() == mainThreadIdentifier;
}

Mutex::Mutex()
{
    pthread_mutex_init(&m_mutex, NULL);
}

Mutex::~Mutex()
{
    pthread_mutex_destroy(&m_mutex);
}

void Mutex::lock()
{
    if (pthread_mutex_lock(&m_mutex) != 0)
        ASSERT(false);
}
    
bool Mutex::tryLock()
{
    int result = pthread_mutex_trylock(&m_mutex);
    
    if (result == 0)
        return true;
    else if (result == EBUSY)
        return false;

    ASSERT(false);
    return false;
}

void Mutex::unlock()
{
    if (pthread_mutex_unlock(&m_mutex) != 0)
        ASSERT(false);
}

ThreadCondition::ThreadCondition()
{ 
    pthread_cond_init(&m_condition, NULL);
}

ThreadCondition::~ThreadCondition()
{
    pthread_cond_destroy(&m_condition);
}
    
void ThreadCondition::wait(Mutex& mutex)
{
    if (pthread_cond_wait(&m_condition, &mutex.impl()) != 0)
        ASSERT(false);
}

bool ThreadCondition::timedWait(Mutex& mutex, double absoluteTime)
{
    if (absoluteTime < currentTime())
        return false;

    if (absoluteTime > INT_MAX) {
        wait(mutex);
        return true;
    }

    int timeSeconds = static_cast<int>(absoluteTime);
    int timeNanoseconds = static_cast<int>((absoluteTime - timeSeconds) * 1E9);

    timespec targetTime;
    targetTime.tv_sec = timeSeconds;
    targetTime.tv_nsec = timeNanoseconds;

    return pthread_cond_timedwait(&m_condition, &mutex.impl(), &targetTime) == 0;
}

void ThreadCondition::signal()
{
    if (pthread_cond_signal(&m_condition) != 0)
        ASSERT(false);
}

void ThreadCondition::broadcast()
{
    if (pthread_cond_broadcast(&m_condition) != 0)
        ASSERT(false);
}
    
} // namespace WTF

#endif // USE(PTHREADS)
