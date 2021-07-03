/*
 *  WebCoreThreadRun.cpp
 *  WebCore
 *
 *  Copyright (C) 2010 Apple Inc.  All rights reserved.
 *
 */

#include "config.h"
#include "WebCoreThreadRun.h"

#include "WebCoreThread.h"
#include "WebCoreThreadInternal.h"
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Vector.h>

namespace {

class WebThreadBlockState {
public:
    WebThreadBlockState()
        : m_completed(false)
    {
    }

    void waitForCompletion()
    {
        MutexLocker locker(m_stateMutex);
        while (!m_completed)
            m_completionCondition.wait(m_stateMutex);
    }

    void setCompleted()
    {
        MutexLocker locker(m_stateMutex);
        ASSERT(!m_completed);
        m_completed = true;
        m_completionCondition.signal();
    }
private:
    WTF::Mutex m_stateMutex;
    WTF::ThreadCondition m_completionCondition;
    bool m_completed;
};

class WebThreadBlock {
public:
    WebThreadBlock(void (^task)(), WebThreadBlockState* state)
        : m_task(Block_copy(task))
        , m_state(state)
    {
    }

    WebThreadBlock(const WebThreadBlock& other)
        : m_task(Block_copy(other.m_task))
        , m_state(other.m_state)
    {
    }

    WebThreadBlock& operator=(const WebThreadBlock& other)
    {
        void (^oldTask)() = m_task;
        m_task = Block_copy(other.m_task);
        Block_release(oldTask);
        m_state = other.m_state;
        return *this;
    }

    ~WebThreadBlock()
    {
        Block_release(m_task);
    }

    void operator()() const
    {
        m_task();
        if (m_state)
            m_state->setCompleted();
    }

private:
    void (^m_task)();
    WebThreadBlockState* m_state;
};

}

extern "C" {

typedef WTF::Vector<WebThreadBlock> WebThreadRunQueue;

static WTF::Mutex *runQueueLock = NULL;
static CFRunLoopSourceRef runSource = NULL;
static WebThreadRunQueue *runQueue = NULL;

static void HandleRunSource(void *info)
{
    UNUSED_PARAM(info);
    ASSERT(WebThreadIsCurrent());
    ASSERT(runQueueLock);
    ASSERT(runSource);
    ASSERT(runQueue);

    WebThreadRunQueue queueCopy;
    {
        MutexLocker locker(*runQueueLock);
        queueCopy = *runQueue;
        runQueue->clear();
    }

    for (WebThreadRunQueue::const_iterator it = queueCopy.begin(); it != queueCopy.end(); ++it)
        (*it)();
}

static void _WebThreadRun(void (^task)(), bool synchronous)
{
    if (WebThreadIsCurrent() || !WebThreadIsEnabled()) {
        task();
        return;
    }
    
    ASSERT(runQueueLock);
    ASSERT(runSource);
    ASSERT(runQueue);

    WebThreadBlockState* state = 0;
    if (synchronous)
        state = new WebThreadBlockState;

    {
        MutexLocker locker(*runQueueLock);
        runQueue->append(WebThreadBlock(task, state));
    }

    CFRunLoopSourceSignal(runSource);
    CFRunLoopWakeUp(WebThreadRunLoop());

    if (synchronous) {
        state->waitForCompletion();
        delete state;
    }
}

void WebThreadRun(void (^task)())
{
    _WebThreadRun(task, false);
}

void WebThreadRunSync(void (^task)())
{
    _WebThreadRun(task, true);
}
    
void WebThreadInitRunQueue()
{
    ASSERT(!runQueue);
    ASSERT(!runQueueLock);
    ASSERT(!runSource);

    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        runQueue = new WebThreadRunQueue;

        CFRunLoopSourceContext runSourceContext = {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, HandleRunSource};
        runSource = CFRunLoopSourceCreate(NULL, -1, &runSourceContext);
        CFRunLoopAddSource(WebThreadRunLoop(), runSource, kCFRunLoopDefaultMode);

        runQueueLock = new WTF::Mutex;
    });
}

}
