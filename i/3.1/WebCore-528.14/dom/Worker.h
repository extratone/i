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

#ifndef Worker_h
#define Worker_h

#if ENABLE(WORKERS)

#include "AtomicStringHash.h"
#include "ActiveDOMObject.h"
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "EventListener.h"
#include "EventTarget.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class CachedResource;
    class CachedScript;
    class Document;
    class ScriptExecutionContext;
    class String;
    class WorkerMessagingProxy;
    class WorkerTask;

    typedef int ExceptionCode;

    class Worker : public RefCounted<Worker>, public ActiveDOMObject, private CachedResourceClient, public EventTarget {
    public:
        static PassRefPtr<Worker> create(const String& url, Document* document, ExceptionCode& ec) { return adoptRef(new Worker(url, document, ec)); }
        ~Worker();

        virtual ScriptExecutionContext* scriptExecutionContext() const { return ActiveDOMObject::scriptExecutionContext(); }
        Document* document() const;

        virtual Worker* toWorker() { return this; }

        void postMessage(const String& message);

        void terminate();

        virtual bool canSuspend() const;
        virtual void stop();
        virtual bool hasPendingActivity() const;

        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);

        typedef Vector<RefPtr<EventListener> > ListenerVector;
        typedef HashMap<AtomicString, ListenerVector> EventListenersMap;
        EventListenersMap& eventListeners() { return m_eventListeners; }

        using RefCounted<Worker>::ref;
        using RefCounted<Worker>::deref;

        void setOnmessage(PassRefPtr<EventListener> eventListener) { m_onMessageListener = eventListener; }
        EventListener* onmessage() const { return m_onMessageListener.get(); }

        void setOnerror(PassRefPtr<EventListener> eventListener) { m_onErrorListener = eventListener; }
        EventListener* onerror() const { return m_onErrorListener.get(); }

    private:
        Worker(const String&, Document*, ExceptionCode&);

        virtual void notifyFinished(CachedResource*);

        void dispatchErrorEvent();

        virtual void refEventTarget() { ref(); }
        virtual void derefEventTarget() { deref(); }

        KURL m_scriptURL;
        CachedResourceHandle<CachedScript> m_cachedScript;

        WorkerMessagingProxy* m_messagingProxy; // The proxy outlives the worker to perform thread shutdown.

        RefPtr<EventListener> m_onMessageListener;
        RefPtr<EventListener> m_onErrorListener;
        EventListenersMap m_eventListeners;
    };

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // Worker_h
