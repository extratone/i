/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef JSQuarantinedObjectWrapper_h
#define JSQuarantinedObjectWrapper_h

#include <runtime/JSObject.h>

namespace WebCore {

    class JSQuarantinedObjectWrapper : public JSC::JSObject {
    public:
        static JSQuarantinedObjectWrapper* asWrapper(JSC::JSValuePtr);

        virtual ~JSQuarantinedObjectWrapper();

        virtual JSC::JSObject* unwrappedObject() { return m_unwrappedObject; }

        JSC::JSGlobalObject* unwrappedGlobalObject() const { return m_unwrappedGlobalObject; };
        JSC::ExecState* unwrappedExecState() const;

        bool allowsUnwrappedAccessFrom(JSC::ExecState*) const;

        static const JSC::ClassInfo s_info;

        static PassRefPtr<JSC::Structure> createStructure(JSC::JSValuePtr proto) 
        { 
            return JSC::Structure::create(proto, JSC::TypeInfo(JSC::ObjectType, JSC::ImplementsHasInstance | JSC::OverridesHasInstance)); 
        }

    protected:
        JSQuarantinedObjectWrapper(JSC::ExecState* unwrappedExec, JSC::JSObject* unwrappedObject, PassRefPtr<JSC::Structure>);

        virtual void mark();

    private:
        virtual bool getOwnPropertySlot(JSC::ExecState*, const JSC::Identifier&, JSC::PropertySlot&);
        virtual bool getOwnPropertySlot(JSC::ExecState*, unsigned, JSC::PropertySlot&);

        virtual void put(JSC::ExecState*, const JSC::Identifier&, JSC::JSValuePtr, JSC::PutPropertySlot&);
        virtual void put(JSC::ExecState*, unsigned, JSC::JSValuePtr);

        virtual bool deleteProperty(JSC::ExecState*, const JSC::Identifier&);
        virtual bool deleteProperty(JSC::ExecState*, unsigned);

        virtual JSC::CallType getCallData(JSC::CallData&);
        virtual JSC::ConstructType getConstructData(JSC::ConstructData&);

        virtual bool hasInstance(JSC::ExecState*, JSC::JSValuePtr, JSC::JSValuePtr proto);

        virtual void getPropertyNames(JSC::ExecState*, JSC::PropertyNameArray&);

        virtual JSC::UString className() const { return m_unwrappedObject->className(); }

        virtual bool allowsGetProperty() const { return false; }
        virtual bool allowsSetProperty() const { return false; }
        virtual bool allowsDeleteProperty() const { return false; }
        virtual bool allowsConstruct() const { return false; }
        virtual bool allowsHasInstance() const { return false; }
        virtual bool allowsCallAsFunction() const { return false; }
        virtual bool allowsGetPropertyNames() const { return false; }

        virtual JSC::JSValuePtr prepareIncomingValue(JSC::ExecState* unwrappedExec, JSC::JSValuePtr unwrappedValue) const = 0;
        virtual JSC::JSValuePtr wrapOutgoingValue(JSC::ExecState* unwrappedExec, JSC::JSValuePtr unwrappedValue) const = 0;

        static JSC::JSValuePtr cachedValueGetter(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);

        void transferExceptionToExecState(JSC::ExecState*) const;

        static JSC::JSValuePtr call(JSC::ExecState*, JSC::JSObject* function, JSC::JSValuePtr thisValue, const JSC::ArgList&);
        static JSC::JSObject* construct(JSC::ExecState*, JSC::JSObject*, const JSC::ArgList&);

        JSC::JSGlobalObject* m_unwrappedGlobalObject;
        JSC::JSObject* m_unwrappedObject;
    };

} // namespace WebCore

#endif // JSQuarantinedObjectWrapper_h
