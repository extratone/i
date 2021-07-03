/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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

#include "config.h"
#include "DocumentEventQueue.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "RuntimeApplicationChecks.h"
#include "ScriptExecutionContext.h"
#include "SuspendableTimer.h"

namespace WebCore {
    
class DocumentEventQueueTimer FINAL : public SuspendableTimer {
public:
    static PassOwnPtr<DocumentEventQueueTimer> create(DocumentEventQueue* eventQueue, ScriptExecutionContext* context)
    {
        return adoptPtr(new DocumentEventQueueTimer(eventQueue, context));
    }

private:
    DocumentEventQueueTimer(DocumentEventQueue* eventQueue, ScriptExecutionContext* context)
        : SuspendableTimer(context)
        , m_eventQueue(eventQueue) { }

    virtual void fired() OVERRIDE
    {
        ASSERT(!isSuspended());
        m_eventQueue->pendingEventTimerFired();
    }

    DocumentEventQueue* m_eventQueue;
};

PassRefPtr<DocumentEventQueue> DocumentEventQueue::create(ScriptExecutionContext* context)
{
    return adoptRef(new DocumentEventQueue(context));
}

DocumentEventQueue::DocumentEventQueue(ScriptExecutionContext* context)
    : m_pendingEventTimer(DocumentEventQueueTimer::create(this, context))
    , m_isClosed(false)
{
    m_pendingEventTimer->suspendIfNeeded();
}

DocumentEventQueue::~DocumentEventQueue()
{
}

bool DocumentEventQueue::enqueueEvent(PassRefPtr<Event> event)
{
    if (m_isClosed)
        return false;

    ASSERT(event->target());
    bool wasAdded = m_queuedEvents.add(event).isNewEntry;
    ASSERT_UNUSED(wasAdded, wasAdded); // It should not have already been in the list.
    
    if (!m_pendingEventTimer->isActive())
        m_pendingEventTimer->startOneShot(0);

    return true;
}

void DocumentEventQueue::enqueueOrDispatchScrollEvent(PassRefPtr<Node> target, ScrollEventTargetType targetType)
{
    if (!target->document()->hasListenerType(Document::SCROLL_LISTENER))
        return;

    // Per the W3C CSSOM View Module, scroll events fired at the document should bubble, others should not.
    bool canBubble = targetType == ScrollEventDocumentTarget;
    RefPtr<Event> scrollEvent = Event::create(eventNames().scrollEvent, canBubble, false /* non cancelleable */);
     
    if (!m_nodesWithQueuedScrollEvents.add(target.get()).isNewEntry)
        return;

    scrollEvent->setTarget(target);
    enqueueEvent(scrollEvent.release());
}

bool DocumentEventQueue::cancelEvent(Event* event)
{
    ListHashSet<RefPtr<Event>, 16>::iterator it = m_queuedEvents.find(event);
    bool found = it != m_queuedEvents.end();
    if (found)
        m_queuedEvents.remove(it);
    if (m_queuedEvents.isEmpty())
        m_pendingEventTimer->cancel();
    return found;
}

void DocumentEventQueue::close()
{
    m_isClosed = true;
    m_pendingEventTimer->cancel();
    m_queuedEvents.clear();
}

void DocumentEventQueue::pendingEventTimerFired()
{
    ASSERT(!m_pendingEventTimer->isActive());
    ASSERT(!m_queuedEvents.isEmpty());

    m_nodesWithQueuedScrollEvents.clear();

    // Insert a marker for where we should stop.
    ASSERT(!m_queuedEvents.contains(0));
    bool wasAdded = m_queuedEvents.add(0).isNewEntry;
    ASSERT_UNUSED(wasAdded, wasAdded); // It should not have already been in the list.

    RefPtr<DocumentEventQueue> protector(this);

    while (!m_queuedEvents.isEmpty()) {
        ListHashSet<RefPtr<Event>, 16>::iterator iter = m_queuedEvents.begin();
        RefPtr<Event> event = *iter;
        m_queuedEvents.remove(iter);
        if (!event)
            break;
        dispatchEvent(event.get());
    }
}

void DocumentEventQueue::dispatchEvent(PassRefPtr<Event> event)
{
    EventTarget* eventTarget = event->target();
    if (eventTarget->toDOMWindow())
        eventTarget->toDOMWindow()->dispatchEvent(event, 0);
    else
        eventTarget->dispatchEvent(event);
}

}
