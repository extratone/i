/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "JSCustomSQLTransactionErrorCallback.h"

#include "CString.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "ScriptController.h"
#include "JSSQLError.h"
#include <runtime/JSLock.h>

namespace WebCore {
    
using namespace JSC;
    
JSCustomSQLTransactionErrorCallback::JSCustomSQLTransactionErrorCallback(JSObject* callback, Frame* frame)
    : m_callback(callback)
    , m_frame(frame)
{
}
    
bool JSCustomSQLTransactionErrorCallback::handleEvent(SQLError* error)
{
    ASSERT(m_callback);
    ASSERT(m_frame);
        
    if (!m_frame->script()->isEnabled())
        return true;
        
    JSGlobalObject* globalObject = m_frame->script()->globalObject();
    ExecState* exec = globalObject->globalExec();
        
    JSC::JSLock lock(false);
        
    JSValuePtr function = m_callback->get(exec, Identifier(exec, "handleEvent"));
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone) {
        callType = m_callback->getCallData(callData);
        if (callType == CallTypeNone) {
            // FIXME: Should an exception be thrown here?
            return true;
        }
        function = m_callback;
    }

    RefPtr<JSCustomSQLTransactionErrorCallback> protect(this);
        
    ArgList args;
    args.append(toJS(exec, error));

    JSValuePtr result;
    globalObject->startTimeoutCheck();
    result = call(exec, function, callType, callData, m_callback, args);
    globalObject->stopTimeoutCheck();
        
    if (exec->hadException())
        reportCurrentException(exec);
        
    Document::updateDocumentsRendering();
    
    return result.toBoolean(exec);
}
    
}
