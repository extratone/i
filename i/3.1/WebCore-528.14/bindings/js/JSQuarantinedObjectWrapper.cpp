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
#include "JSQuarantinedObjectWrapper.h"

#include <runtime/JSGlobalObject.h>

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSQuarantinedObjectWrapper)

const ClassInfo JSQuarantinedObjectWrapper::s_info = { "JSQuarantinedObjectWrapper", 0, 0, 0 };

JSQuarantinedObjectWrapper* JSQuarantinedObjectWrapper::asWrapper(JSValuePtr value)
{
    if (!value.isObject())
        return 0;

    JSObject* object = asObject(value);

    if (!object->inherits(&JSQuarantinedObjectWrapper::s_info))
        return 0;

    return static_cast<JSQuarantinedObjectWrapper*>(object);
}

JSValuePtr JSQuarantinedObjectWrapper::cachedValueGetter(ExecState*, const Identifier&, const PropertySlot& slot)
{
    JSValuePtr v = slot.slotBase();
    ASSERT(v);
    return v;
}

JSQuarantinedObjectWrapper::JSQuarantinedObjectWrapper(ExecState* unwrappedExec, JSObject* unwrappedObject, PassRefPtr<Structure> structure)
    : JSObject(structure)
    , m_unwrappedGlobalObject(unwrappedExec->dynamicGlobalObject())
    , m_unwrappedObject(unwrappedObject)
{
    ASSERT_ARG(unwrappedExec, unwrappedExec);
    ASSERT_ARG(unwrappedObject, unwrappedObject);
    ASSERT(this->structure());
}

JSQuarantinedObjectWrapper::~JSQuarantinedObjectWrapper()
{
}

bool JSQuarantinedObjectWrapper::allowsUnwrappedAccessFrom(ExecState* exec) const
{
    return m_unwrappedGlobalObject->profileGroup() == exec->dynamicGlobalObject()->profileGroup();
}

ExecState* JSQuarantinedObjectWrapper::unwrappedExecState() const
{
    return m_unwrappedGlobalObject->globalExec();
}

void JSQuarantinedObjectWrapper::transferExceptionToExecState(ExecState* exec) const
{
    ASSERT(exec != unwrappedExecState());

    if (!unwrappedExecState()->hadException())
        return;

    exec->setException(wrapOutgoingValue(unwrappedExecState(), unwrappedExecState()->exception()));
    unwrappedExecState()->clearException();
}

void JSQuarantinedObjectWrapper::mark()
{
    JSObject::mark();

    if (!m_unwrappedObject->marked())
        m_unwrappedObject->mark();
    if (!m_unwrappedGlobalObject->marked())
        m_unwrappedGlobalObject->mark();
}

bool JSQuarantinedObjectWrapper::getOwnPropertySlot(ExecState* exec, const Identifier& identifier, PropertySlot& slot)
{
    if (!allowsGetProperty()) {
        slot.setUndefined();
        return true;
    }

    PropertySlot unwrappedSlot(m_unwrappedObject);
    bool result = m_unwrappedObject->getOwnPropertySlot(unwrappedExecState(), identifier, unwrappedSlot);
    if (result) {
        JSValuePtr unwrappedValue = unwrappedSlot.getValue(unwrappedExecState(), identifier);
        slot.setCustom(wrapOutgoingValue(unwrappedExecState(), unwrappedValue), cachedValueGetter);
    }

    transferExceptionToExecState(exec);

    return result;
}

bool JSQuarantinedObjectWrapper::getOwnPropertySlot(ExecState* exec, unsigned identifier, PropertySlot& slot)
{
    if (!allowsGetProperty()) {
        slot.setUndefined();
        return true;
    }

    PropertySlot unwrappedSlot(m_unwrappedObject);
    bool result = m_unwrappedObject->getOwnPropertySlot(unwrappedExecState(), identifier, unwrappedSlot);
    if (result) {
        JSValuePtr unwrappedValue = unwrappedSlot.getValue(unwrappedExecState(), identifier);
        slot.setCustom(wrapOutgoingValue(unwrappedExecState(), unwrappedValue), cachedValueGetter);
    }

    transferExceptionToExecState(exec);

    return result;
}

void JSQuarantinedObjectWrapper::put(ExecState* exec, const Identifier& identifier, JSValuePtr value, PutPropertySlot& slot)
{
    if (!allowsSetProperty())
        return;

    m_unwrappedObject->put(unwrappedExecState(), identifier, prepareIncomingValue(exec, value), slot);

    transferExceptionToExecState(exec);
}

void JSQuarantinedObjectWrapper::put(ExecState* exec, unsigned identifier, JSValuePtr value)
{
    if (!allowsSetProperty())
        return;

    m_unwrappedObject->put(unwrappedExecState(), identifier, prepareIncomingValue(exec, value));

    transferExceptionToExecState(exec);
}

bool JSQuarantinedObjectWrapper::deleteProperty(ExecState* exec, const Identifier& identifier)
{
    if (!allowsDeleteProperty())
        return false;

    bool result = m_unwrappedObject->deleteProperty(unwrappedExecState(), identifier);

    transferExceptionToExecState(exec);

    return result;
}

bool JSQuarantinedObjectWrapper::deleteProperty(ExecState* exec, unsigned identifier)
{
    if (!allowsDeleteProperty())
        return false;

    bool result = m_unwrappedObject->deleteProperty(unwrappedExecState(), identifier);

    transferExceptionToExecState(exec);

    return result;
}

JSObject* JSQuarantinedObjectWrapper::construct(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    JSQuarantinedObjectWrapper* wrapper = static_cast<JSQuarantinedObjectWrapper*>(constructor);

    ArgList preparedArgs;
    for (size_t i = 0; i < args.size(); ++i)
        preparedArgs.append(wrapper->prepareIncomingValue(exec, args.at(exec, i)));

    // FIXME: Would be nice to find a way to reuse the result of m_unwrappedObject->getConstructData
    // from when we called it in JSQuarantinedObjectWrapper::getConstructData.
    ConstructData unwrappedConstructData;
    ConstructType unwrappedConstructType = wrapper->m_unwrappedObject->getConstructData(unwrappedConstructData);
    ASSERT(unwrappedConstructType != ConstructTypeNone);

    JSValuePtr unwrappedResult = JSC::construct(wrapper->unwrappedExecState(), wrapper->m_unwrappedObject, unwrappedConstructType, unwrappedConstructData, preparedArgs);

    JSValuePtr resultValue = wrapper->wrapOutgoingValue(wrapper->unwrappedExecState(), unwrappedResult);
    ASSERT(resultValue.isObject());
    JSObject* result = asObject(resultValue);

    wrapper->transferExceptionToExecState(exec);

    return result;
}

ConstructType JSQuarantinedObjectWrapper::getConstructData(ConstructData& constructData)
{
    if (!allowsConstruct())
        return ConstructTypeNone;
    ConstructData unwrappedConstructData;
    if (m_unwrappedObject->getConstructData(unwrappedConstructData) == ConstructTypeNone)
        return ConstructTypeNone;
    constructData.native.function = construct;
    return ConstructTypeHost;
}

bool JSQuarantinedObjectWrapper::hasInstance(ExecState* exec, JSValuePtr value, JSValuePtr proto)
{
    if (!allowsHasInstance())
        return false;

    bool result = m_unwrappedObject->hasInstance(unwrappedExecState(), prepareIncomingValue(exec, value), prepareIncomingValue(exec, proto));

    transferExceptionToExecState(exec);

    return result;
}

JSValuePtr JSQuarantinedObjectWrapper::call(ExecState* exec, JSObject* function, JSValuePtr thisValue, const ArgList& args)
{
    JSQuarantinedObjectWrapper* wrapper = static_cast<JSQuarantinedObjectWrapper*>(function);

    JSValuePtr preparedThisValue = wrapper->prepareIncomingValue(exec, thisValue);

    ArgList preparedArgs;
    for (size_t i = 0; i < args.size(); ++i)
        preparedArgs.append(wrapper->prepareIncomingValue(exec, args.at(exec, i)));

    // FIXME: Would be nice to find a way to reuse the result of m_unwrappedObject->getCallData
    // from when we called it in JSQuarantinedObjectWrapper::getCallData.
    CallData unwrappedCallData;
    CallType unwrappedCallType = wrapper->m_unwrappedObject->getCallData(unwrappedCallData);
    ASSERT(unwrappedCallType != CallTypeNone);

    JSValuePtr unwrappedResult = JSC::call(wrapper->unwrappedExecState(), wrapper->m_unwrappedObject, unwrappedCallType, unwrappedCallData, preparedThisValue, preparedArgs);

    JSValuePtr result = wrapper->wrapOutgoingValue(wrapper->unwrappedExecState(), unwrappedResult);

    wrapper->transferExceptionToExecState(exec);

    return result;
}

CallType JSQuarantinedObjectWrapper::getCallData(CallData& callData)
{
    if (!allowsCallAsFunction())
        return CallTypeNone;
    CallData unwrappedCallData;
    if (m_unwrappedObject->getCallData(unwrappedCallData) == CallTypeNone)
        return CallTypeNone;
    callData.native.function = call;
    return CallTypeHost;
}

void JSQuarantinedObjectWrapper::getPropertyNames(ExecState*, PropertyNameArray& array)
{
    if (!allowsGetPropertyNames())
        return;

    m_unwrappedObject->getPropertyNames(unwrappedExecState(), array);
}

} // namespace WebCore
