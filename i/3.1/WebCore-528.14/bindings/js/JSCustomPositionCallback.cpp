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
#include "JSCustomPositionCallback.h"

#include "CString.h"
#include "Frame.h"
#include "JSGeoposition.h"
#include "Page.h"
#include "ScriptController.h"
#include <runtime/JSLock.h>

namespace WebCore {

using namespace JSC;

JSCustomPositionCallback::JSCustomPositionCallback(JSObject* callback, Frame* frame)
    : m_callback(callback)
    , m_frame(frame)
{
}

void JSCustomPositionCallback::handleEvent(Geoposition* geoposition, bool& raisedException)
{
    ASSERT(m_callback);
    ASSERT(m_frame);
    
    if (!m_frame->script()->isEnabled())
        return;
    
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
            return;
        }
        function = m_callback;
    }
    
    RefPtr<JSCustomPositionCallback> protect(this);
    
    ArgList args;
    args.append(toJS(exec, geoposition));
    
    globalObject->startTimeoutCheck();
    call(exec, function, callType, callData, m_callback, args);
    globalObject->stopTimeoutCheck();
    
    if (exec->hadException()) {
        reportCurrentException(exec);
        raisedException = true;
    }
    
    Document::updateDocumentsRendering();
}

} // namespace WebCore
