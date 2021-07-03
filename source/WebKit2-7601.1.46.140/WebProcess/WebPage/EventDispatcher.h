/*
 * Copyright (C) 2011, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EventDispatcher_h
#define EventDispatcher_h

#include "Connection.h"

#include <WebCore/WheelEventDeltaTracker.h>
#include <WebEvent.h>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/SpinLock.h>
#include <wtf/ThreadingPrimitives.h>

namespace WebCore {
class ThreadedScrollingTree;
}

namespace WebKit {

class WebEvent;
class WebPage;
class WebWheelEvent;

class EventDispatcher : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<EventDispatcher> create();
    ~EventDispatcher();

#if ENABLE(ASYNC_SCROLLING)
    void addScrollingTreeForPage(WebPage*);
    void removeScrollingTreeForPage(WebPage*);
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    typedef Vector<WebTouchEvent, 1> TouchEventQueue;

    void clearQueuedTouchEventsForPage(const WebPage&);
    void getQueuedTouchEventsForPage(const WebPage&, TouchEventQueue&);
#endif

    void initializeConnection(IPC::Connection*);

private:
    EventDispatcher();

    // IPC::Connection::WorkQueueMessageReceiver.
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    // Message handlers
    void wheelEvent(uint64_t pageID, const WebWheelEvent&, bool canRubberBandAtLeft, bool canRubberBandAtRight, bool canRubberBandAtTop, bool canRubberBandAtBottom);
#if ENABLE(IOS_TOUCH_EVENTS)
    void touchEvent(uint64_t pageID, const WebTouchEvent&);
#endif


    // This is called on the main thread.
    void dispatchWheelEvent(uint64_t pageID, const WebWheelEvent&);
#if ENABLE(IOS_TOUCH_EVENTS)
    void dispatchTouchEvents();
#endif

#if ENABLE(ASYNC_SCROLLING)
    void sendDidReceiveEvent(uint64_t pageID, const WebEvent&, bool didHandleEvent);
#endif

    Ref<WorkQueue> m_queue;

#if ENABLE(ASYNC_SCROLLING)
    Mutex m_scrollingTreesMutex;
    HashMap<uint64_t, RefPtr<WebCore::ThreadedScrollingTree>> m_scrollingTrees;
#endif
    std::unique_ptr<WebCore::WheelEventDeltaTracker> m_recentWheelEventDeltaTracker;
#if ENABLE(IOS_TOUCH_EVENTS)
    SpinLock m_touchEventsLock;
    HashMap<uint64_t, TouchEventQueue> m_touchEvents;
#endif
};

} // namespace WebKit

#endif // EventDispatcher_h
