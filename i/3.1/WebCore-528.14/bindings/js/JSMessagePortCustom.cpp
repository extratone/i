/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSMessagePort.h"

#include "AtomicString.h"
#include "Event.h"
#include "Frame.h"
#include "JSDOMGlobalObject.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "MessagePort.h"

using namespace JSC;

namespace WebCore {

void JSMessagePort::mark()
{
    DOMObject::mark();

    markIfNotNull(m_impl->onmessage());
    markIfNotNull(m_impl->onclose());

    if (MessagePort* entangledPort = m_impl->entangledPort()) {
        DOMObject* wrapper = getCachedDOMObjectWrapper(*Heap::heap(this)->globalData(), entangledPort);
        if (wrapper && !wrapper->marked())
            wrapper->mark();
    }

    typedef MessagePort::EventListenersMap EventListenersMap;
    typedef MessagePort::ListenerVector ListenerVector;
    EventListenersMap& eventListeners = m_impl->eventListeners();
    for (EventListenersMap::iterator mapIter = eventListeners.begin(); mapIter != eventListeners.end(); ++mapIter) {
        for (ListenerVector::iterator vecIter = mapIter->second.begin(); vecIter != mapIter->second.end(); ++vecIter) 
            (*vecIter)->mark();
    }
}

JSValuePtr JSMessagePort::startConversation(ExecState* exec, const ArgList& args)
{
    JSDOMGlobalObject* globalObject = static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject());
    const UString& message = args.at(exec, 0).toString(exec);

    return toJS(exec, impl()->startConversation(globalObject->scriptExecutionContext(), message).get());
}

JSValuePtr JSMessagePort::addEventListener(ExecState* exec, const ArgList& args)
{
    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(impl()->scriptExecutionContext());
    if (!globalObject)
        return jsUndefined();
    RefPtr<JSEventListener> listener = globalObject->findOrCreateJSEventListener(args.at(exec, 1));
    if (!listener)
        return jsUndefined();
    impl()->addEventListener(args.at(exec, 0).toString(exec), listener.release(), args.at(exec, 2).toBoolean(exec));
    return jsUndefined();
}

JSValuePtr JSMessagePort::removeEventListener(ExecState* exec, const ArgList& args)
{
    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(impl()->scriptExecutionContext());
    if (!globalObject)
        return jsUndefined();
    JSEventListener* listener = globalObject->findJSEventListener(args.at(exec, 1));
    if (!listener)
        return jsUndefined();
    impl()->removeEventListener(args.at(exec, 0).toString(exec), listener, args.at(exec, 2).toBoolean(exec));
    return jsUndefined();
}

} // namespace WebCore
