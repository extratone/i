/*
 * Copyright (C) 2007 Alexey Proskuryakov (ap@nypop.com)
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
 */

#include "config.h"
#include "JSCustomXPathNSResolver.h"

#if ENABLE(XPATH)

#include "CString.h"
#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "JSDOMWindowCustom.h"
#include "JSDOMBinding.h"
#include "ScriptController.h"
#include <runtime/JSLock.h>

namespace WebCore {

using namespace JSC;

PassRefPtr<JSCustomXPathNSResolver> JSCustomXPathNSResolver::create(JSC::ExecState* exec, JSC::JSValuePtr value)
{
    if (value.isUndefinedOrNull())
        return 0;

    JSObject* resolverObject = value.getObject();
    if (!resolverObject) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return 0;
    }
    
    return adoptRef(new JSCustomXPathNSResolver(resolverObject, asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame()));
}

JSCustomXPathNSResolver::JSCustomXPathNSResolver(JSObject* customResolver, Frame* frame)
    : m_customResolver(customResolver)
    , m_frame(frame)
{
}

JSCustomXPathNSResolver::~JSCustomXPathNSResolver()
{
}

String JSCustomXPathNSResolver::lookupNamespaceURI(const String& prefix)
{
    ASSERT(m_customResolver);

    if (!m_frame)
        return String();
    if (!m_frame->script()->isEnabled())
        return String();

    JSLock lock(false);

    JSGlobalObject* globalObject = m_frame->script()->globalObject();
    ExecState* exec = globalObject->globalExec();
        
    JSValuePtr function = m_customResolver->get(exec, Identifier(exec, "lookupNamespaceURI"));
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone) {
        callType = m_customResolver->getCallData(callData);
        if (callType == CallTypeNone) {
            // FIXME: Pass actual line number and source URL.
            m_frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, "XPathNSResolver does not have a lookupNamespaceURI method.", 0, String());
            return String();
        }
        function = m_customResolver;
    }

    RefPtr<JSCustomXPathNSResolver> selfProtector(this);

    ArgList args;
    args.append(jsString(exec, prefix));

    globalObject->startTimeoutCheck();
    JSValuePtr retval = call(exec, function, callType, callData, m_customResolver, args);
    globalObject->stopTimeoutCheck();

    String result;
    if (exec->hadException())
        reportCurrentException(exec);
    else {
        if (!retval.isUndefinedOrNull())
            result = retval.toString(exec);
    }

    Document::updateDocumentsRendering();

    return result;
}

} // namespace WebCore

#endif // ENABLE(XPATH)
