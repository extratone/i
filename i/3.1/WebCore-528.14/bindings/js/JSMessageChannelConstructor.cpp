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
#include "JSMessageChannelConstructor.h"

#include "Document.h"
#include "JSDocument.h"
#include "JSMessageChannel.h"
#include "JSWorkerContext.h"
#include "MessageChannel.h"

using namespace JSC;

namespace WebCore {

const ClassInfo JSMessageChannelConstructor::s_info = { "MessageChannelConstructor", 0, 0, 0 };

JSMessageChannelConstructor::JSMessageChannelConstructor(ExecState* exec, ScriptExecutionContext* scriptExecutionContext)
    : DOMObject(JSMessageChannelConstructor::createStructure(exec->lexicalGlobalObject()->objectPrototype()))
    , m_scriptExecutionContext(scriptExecutionContext)
{
    if (m_scriptExecutionContext->isDocument())
        m_contextWrapper = toJS(exec, static_cast<Document*>(scriptExecutionContext));
#if ENABLE(WORKERS)
    else if (m_scriptExecutionContext->isWorkerContext())
        m_contextWrapper = toJSDOMGlobalObject(scriptExecutionContext);
#endif
    else
        ASSERT_NOT_REACHED();

    putDirect(exec->propertyNames().prototype, JSMessageChannelPrototype::self(exec, exec->lexicalGlobalObject()), None);
}

JSMessageChannelConstructor::~JSMessageChannelConstructor()
{
}

ConstructType JSMessageChannelConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = construct;
    return ConstructTypeHost;
}

JSObject* JSMessageChannelConstructor::construct(ExecState* exec, JSObject* constructor, const ArgList&)
{
    return asObject(toJS(exec, MessageChannel::create(static_cast<JSMessageChannelConstructor*>(constructor)->scriptExecutionContext())));
}

void JSMessageChannelConstructor::mark()
{
    DOMObject::mark();
    if (!m_contextWrapper.marked())
        m_contextWrapper.mark();
}

} // namespace WebCore
