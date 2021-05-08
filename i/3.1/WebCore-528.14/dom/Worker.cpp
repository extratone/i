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

#include "config.h"

#if ENABLE(WORKERS)

#include "Worker.h"

#include "CachedScript.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "Document.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "MessageEvent.h"
#include "SecurityOrigin.h"
#include "WorkerContext.h"
#include "WorkerMessagingProxy.h"
#include "WorkerTask.h"
#include "WorkerThread.h"
#include <wtf/MainThread.h>

namespace WebCore {

Worker::Worker(const String& url, Document* doc, ExceptionCode& ec)
    : ActiveDOMObject(doc, this)
    , m_messagingProxy(new WorkerMessagingProxy(doc, this))
{
    m_scriptURL = doc->completeURL(url);
    if (url.isEmpty() || !m_scriptURL.isValid()) {
        ec = SYNTAX_ERR;
        return;
    }

    if (!doc->securityOrigin()->canAccess(SecurityOrigin::create(m_scriptURL).get())) {
        ec = SECURITY_ERR;
        return;
    }

    m_cachedScript = doc->docLoader()->requestScript(m_scriptURL, document()->charset());
    if (!m_cachedScript) {
        dispatchErrorEvent();
        return;
    }

    setPendingActivity(this);  // The worker context does not exist while loading, so we much ensure that the worker object is not collected, as well as its event listeners.
    m_cachedScript->addClient(this);
}

Worker::~Worker()
{
    ASSERT(isMainThread());
    ASSERT(scriptExecutionContext()); // The context is protected by messaging proxy, so it cannot be destroyed while a Worker exists.
    m_messagingProxy->workerObjectDestroyed();
}

Document* Worker::document() const
{
    ASSERT(scriptExecutionContext()->isDocument());
    return static_cast<Document*>(scriptExecutionContext());
}

void Worker::postMessage(const String& message)
{
    m_messagingProxy->postMessageToWorkerContext(message);
}

void Worker::terminate()
{
    m_messagingProxy->terminate();
}

bool Worker::canSuspend() const
{
    // FIXME: It is not currently possible to suspend a worker, so pages with workers can not go into page cache.
    return false;
}

void Worker::stop()
{
    terminate();
}

bool Worker::hasPendingActivity() const
{
    return m_messagingProxy->workerThreadHasPendingActivity() || ActiveDOMObject::hasPendingActivity();
}

void Worker::notifyFinished(CachedResource* unusedResource)
{
    ASSERT_UNUSED(unusedResource, unusedResource == m_cachedScript);

    if (m_cachedScript->errorOccurred())
        dispatchErrorEvent();
    else {
        String userAgent = document()->frame() ? document()->frame()->loader()->userAgent(m_scriptURL) : String();
        RefPtr<WorkerThread> thread = WorkerThread::create(m_scriptURL, userAgent, m_cachedScript->script(), m_messagingProxy);
        m_messagingProxy->workerThreadCreated(thread);
        thread->start();
    }

    m_cachedScript->removeClient(this);
    m_cachedScript = 0;

    unsetPendingActivity(this);
}

void Worker::dispatchErrorEvent()
{
    RefPtr<Event> evt = Event::create(eventNames().errorEvent, false, true);
    if (m_onErrorListener) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onErrorListener->handleEvent(evt.get(), true);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

void Worker::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end()) {
        ListenerVector listeners;
        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    } else {
        ListenerVector& listeners = iter->second;
        for (ListenerVector::iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
            if (*listenerIter == eventListener)
                return;
        }
        
        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    }    
}

void Worker::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end())
        return;
    
    ListenerVector& listeners = iter->second;
    for (ListenerVector::const_iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter) {
        if (*listenerIter == eventListener) {
            listeners.remove(listenerIter - listeners.begin());
            return;
        }
    }
}

bool Worker::dispatchEvent(PassRefPtr<Event> event, ExceptionCode& ec)
{
    if (!event || event->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }

    ListenerVector listenersCopy = m_eventListeners.get(event->type());
    for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
        event->setTarget(this);
        event->setCurrentTarget(this);
        listenerIter->get()->handleEvent(event.get(), false);
    }

    return !event->defaultPrevented();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
