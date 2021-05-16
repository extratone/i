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

#ifndef JSMessageChannelConstructor_h
#define JSMessageChannelConstructor_h

#include "JSDOMBinding.h"

namespace WebCore {

    class JSMessageChannelConstructor : public DOMObject {
    public:
        JSMessageChannelConstructor(JSC::ExecState*, ScriptExecutionContext*);
        virtual ~JSMessageChannelConstructor();
        virtual const JSC::ClassInfo* classInfo() const { return &s_info; }
        static const JSC::ClassInfo s_info;

        ScriptExecutionContext* scriptExecutionContext() const { return m_scriptExecutionContext; }

        virtual bool implementsHasInstance() const { return true; }
        static JSC::JSObject* construct(JSC::ExecState*, JSC::JSObject*, const JSC::ArgList&);
        virtual JSC::ConstructType getConstructData(JSC::ConstructData&);

        virtual void mark();

    private:
        ScriptExecutionContext* m_scriptExecutionContext;
        JSC::JSValuePtr m_contextWrapper;
    };

} // namespace WebCore

#endif // JSMessageChannelConstructor_h
