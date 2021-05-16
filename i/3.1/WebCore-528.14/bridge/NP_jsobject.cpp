/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "NP_jsobject.h"

#include "PlatformString.h"
#include "StringSourceProvider.h"
#include "c_utility.h"
#include "c_instance.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"
#include "runtime_root.h"
#include <runtime/Error.h>
#include <runtime/JSGlobalObject.h>
#include <runtime/JSLock.h>
#include <runtime/PropertyNameArray.h>
#include <parser/SourceCode.h>
#include <runtime/Completion.h>
#include <runtime/Completion.h>

using WebCore::String;
using WebCore::StringSourceProvider;
using namespace JSC;
using namespace JSC::Bindings;

static void getListFromVariantArgs(ExecState* exec, const NPVariant* args, unsigned argCount, RootObject* rootObject, ArgList& aList)
{
    for (unsigned i = 0; i < argCount; ++i)
        aList.append(convertNPVariantToValue(exec, &args[i], rootObject));
}

static NPObject* jsAllocate(NPP, NPClass*)
{
    return static_cast<NPObject*>(malloc(sizeof(JavaScriptObject)));
}

static void jsDeallocate(NPObject* npObj)
{
    JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(npObj);

    if (obj->rootObject && obj->rootObject->isValid())
        obj->rootObject->gcUnprotect(obj->imp);

    if (obj->rootObject)
        obj->rootObject->deref();

    free(obj);
}

static NPClass javascriptClass = { 1, jsAllocate, jsDeallocate, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static NPClass noScriptClass = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

NPClass* NPScriptObjectClass = &javascriptClass;
static NPClass* NPNoScriptObjectClass = &noScriptClass;

NPObject* _NPN_CreateScriptObject(NPP npp, JSObject* imp, PassRefPtr<RootObject> rootObject)
{
    JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(_NPN_CreateObject(npp, NPScriptObjectClass));

    obj->rootObject = rootObject.releaseRef();

    if (obj->rootObject)
        obj->rootObject->gcProtect(imp);
    obj->imp = imp;

    return reinterpret_cast<NPObject*>(obj);
}

NPObject* _NPN_CreateNoScriptObject(void)
{
    return _NPN_CreateObject(0, NPNoScriptObjectClass);
}

bool _NPN_InvokeDefault(NPP, NPObject* o, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o); 
        
        VOID_TO_NPVARIANT(*result);
        
        // Lookup the function object.
        RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;
        
        ExecState* exec = rootObject->globalObject()->globalExec();
        JSLock lock(false);
        
        // Call the function object.
        JSValuePtr function = obj->imp;
        CallData callData;
        CallType callType = function.getCallData(callData);
        if (callType == CallTypeNone)
            return false;
        
        ArgList argList;
        getListFromVariantArgs(exec, args, argCount, rootObject, argList);
        ProtectedPtr<JSGlobalObject> globalObject = rootObject->globalObject();
        globalObject->startTimeoutCheck();
        JSValuePtr resultV = call(exec, function, callType, callData, function, argList);
        globalObject->stopTimeoutCheck();

        // Convert and return the result of the function call.
        convertValueToNPVariant(exec, resultV, result);
        exec->clearException();
        return true;        
    }

    if (o->_class->invokeDefault)
        return o->_class->invokeDefault(o, args, argCount, result);    
    VOID_TO_NPVARIANT(*result);
    return true;
}

bool _NPN_Invoke(NPP npp, NPObject* o, NPIdentifier methodName, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o); 

        PrivateIdentifier* i = static_cast<PrivateIdentifier*>(methodName);
        if (!i->isString)
            return false;

        // Special case the "eval" method.
        if (methodName == _NPN_GetStringIdentifier("eval")) {
            if (argCount != 1)
                return false;
            if (args[0].type != NPVariantType_String)
                return false;
            return _NPN_Evaluate(npp, o, const_cast<NPString*>(&args[0].value.stringValue), result);
        }

        // Look up the function object.
        RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;
        ExecState* exec = rootObject->globalObject()->globalExec();
        JSLock lock(false);
        JSValuePtr function = obj->imp->get(exec, identifierFromNPIdentifier(i->value.string));
        CallData callData;
        CallType callType = function.getCallData(callData);
        if (callType == CallTypeNone)
            return false;

        // Call the function object.
        ArgList argList;
        getListFromVariantArgs(exec, args, argCount, rootObject, argList);
        ProtectedPtr<JSGlobalObject> globalObject = rootObject->globalObject();
        globalObject->startTimeoutCheck();
        JSValuePtr resultV = call(exec, function, callType, callData, obj->imp, argList);
        globalObject->stopTimeoutCheck();

        // Convert and return the result of the function call.
        convertValueToNPVariant(exec, resultV, result);
        exec->clearException();
        return true;
    }

    if (o->_class->invoke)
        return o->_class->invoke(o, methodName, args, argCount, result);
    
    VOID_TO_NPVARIANT(*result);
    return true;
}

bool _NPN_Evaluate(NPP, NPObject* o, NPString* s, NPVariant* variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o); 

        RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;

        ExecState* exec = rootObject->globalObject()->globalExec();
        JSLock lock(false);
        String scriptString = convertNPStringToUTF16(s);
        ProtectedPtr<JSGlobalObject> globalObject = rootObject->globalObject();
        globalObject->startTimeoutCheck();
        Completion completion = JSC::evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(scriptString));
        globalObject->stopTimeoutCheck();
        ComplType type = completion.complType();
        
        JSValuePtr result;
        if (type == Normal) {
            result = completion.value();
            if (!result)
                result = jsUndefined();
        } else
            result = jsUndefined();

        convertValueToNPVariant(exec, result, variant);
        exec->clearException();
        return true;
    }

    VOID_TO_NPVARIANT(*variant);
    return false;
}

bool _NPN_GetProperty(NPP, NPObject* o, NPIdentifier propertyName, NPVariant* variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o); 

        RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;

        ExecState* exec = rootObject->globalObject()->globalExec();
        PrivateIdentifier* i = static_cast<PrivateIdentifier*>(propertyName);
        
        JSLock lock(false);
        JSValuePtr result;
        if (i->isString)
            result = obj->imp->get(exec, identifierFromNPIdentifier(i->value.string));
        else
            result = obj->imp->get(exec, i->value.number);

        convertValueToNPVariant(exec, result, variant);
        exec->clearException();
        return true;
    }

    if (o->_class->hasProperty && o->_class->getProperty) {
        if (o->_class->hasProperty(o, propertyName))
            return o->_class->getProperty(o, propertyName, variant);
        return false;
    }

    VOID_TO_NPVARIANT(*variant);
    return false;
}

bool _NPN_SetProperty(NPP, NPObject* o, NPIdentifier propertyName, const NPVariant* variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o); 

        RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;

        ExecState* exec = rootObject->globalObject()->globalExec();
        JSLock lock(false);
        PrivateIdentifier* i = static_cast<PrivateIdentifier*>(propertyName);

        if (i->isString) {
            PutPropertySlot slot;
            obj->imp->put(exec, identifierFromNPIdentifier(i->value.string), convertNPVariantToValue(exec, variant, rootObject), slot);
        } else
            obj->imp->put(exec, i->value.number, convertNPVariantToValue(exec, variant, rootObject));
        exec->clearException();
        return true;
    }

    if (o->_class->setProperty)
        return o->_class->setProperty(o, propertyName, variant);

    return false;
}

bool _NPN_RemoveProperty(NPP, NPObject* o, NPIdentifier propertyName)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o); 

        RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;

        ExecState* exec = rootObject->globalObject()->globalExec();
        PrivateIdentifier* i = static_cast<PrivateIdentifier*>(propertyName);
        if (i->isString) {
            if (!obj->imp->hasProperty(exec, identifierFromNPIdentifier(i->value.string))) {
                exec->clearException();
                return false;
            }
        } else {
            if (!obj->imp->hasProperty(exec, i->value.number)) {
                exec->clearException();
                return false;
            }
        }

        JSLock lock(false);
        if (i->isString)
            obj->imp->deleteProperty(exec, identifierFromNPIdentifier(i->value.string));
        else
            obj->imp->deleteProperty(exec, i->value.number);

        exec->clearException();
        return true;
    }
    return false;
}

bool _NPN_HasProperty(NPP, NPObject* o, NPIdentifier propertyName)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o); 

        RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;

        ExecState* exec = rootObject->globalObject()->globalExec();
        PrivateIdentifier* i = static_cast<PrivateIdentifier*>(propertyName);
        JSLock lock(false);
        if (i->isString) {
            bool result = obj->imp->hasProperty(exec, identifierFromNPIdentifier(i->value.string));
            exec->clearException();
            return result;
        }

        bool result = obj->imp->hasProperty(exec, i->value.number);
        exec->clearException();
        return result;
    }

    if (o->_class->hasProperty)
        return o->_class->hasProperty(o, propertyName);

    return false;
}

bool _NPN_HasMethod(NPP, NPObject* o, NPIdentifier methodName)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o); 

        PrivateIdentifier* i = static_cast<PrivateIdentifier*>(methodName);
        if (!i->isString)
            return false;

        RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;

        ExecState* exec = rootObject->globalObject()->globalExec();
        JSLock lock(false);
        JSValuePtr func = obj->imp->get(exec, identifierFromNPIdentifier(i->value.string));
        exec->clearException();
        return !func.isUndefined();
    }
    
    if (o->_class->hasMethod)
        return o->_class->hasMethod(o, methodName);
    
    return false;
}

void _NPN_SetException(NPObject*, const NPUTF8* message)
{
    // Ignorning the NPObject param is consistent with the Mozilla implementation.
    UString exception(message);
    CInstance::setGlobalException(exception);
}

bool _NPN_Enumerate(NPP, NPObject* o, NPIdentifier** identifier, uint32_t* count)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o); 
        
        RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;
        
        ExecState* exec = rootObject->globalObject()->globalExec();
        JSLock lock(false);
        PropertyNameArray propertyNames(exec);

        obj->imp->getPropertyNames(exec, propertyNames);
        unsigned size = static_cast<unsigned>(propertyNames.size());
        // FIXME: This should really call NPN_MemAlloc but that's in WebKit
        NPIdentifier* identifiers = static_cast<NPIdentifier*>(malloc(sizeof(NPIdentifier) * size));
        
        for (unsigned i = 0; i < size; ++i)
            identifiers[i] = _NPN_GetStringIdentifier(propertyNames[i].ustring().UTF8String().c_str());

        *identifier = identifiers;
        *count = size;

        exec->clearException();
        return true;
    }
    
    if (NP_CLASS_STRUCT_VERSION_HAS_ENUM(o->_class) && o->_class->enumerate)
        return o->_class->enumerate(o, identifier, count);
    
    return false;
}

bool _NPN_Construct(NPP, NPObject* o, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = reinterpret_cast<JavaScriptObject*>(o);
        
        VOID_TO_NPVARIANT(*result);
        
        // Lookup the constructor object.
        RootObject* rootObject = obj->rootObject;
        if (!rootObject || !rootObject->isValid())
            return false;
        
        ExecState* exec = rootObject->globalObject()->globalExec();
        JSLock lock(false);
        
        // Call the constructor object.
        JSValuePtr constructor = obj->imp;
        ConstructData constructData;
        ConstructType constructType = constructor.getConstructData(constructData);
        if (constructType == ConstructTypeNone)
            return false;
        
        ArgList argList;
        getListFromVariantArgs(exec, args, argCount, rootObject, argList);
        ProtectedPtr<JSGlobalObject> globalObject = rootObject->globalObject();
        globalObject->startTimeoutCheck();
        JSValuePtr resultV = construct(exec, constructor, constructType, constructData, argList);
        globalObject->stopTimeoutCheck();
        
        // Convert and return the result.
        convertValueToNPVariant(exec, resultV, result);
        exec->clearException();
        return true;
    }
    
    if (NP_CLASS_STRUCT_VERSION_HAS_CTOR(o->_class) && o->_class->construct)
        return o->_class->construct(o, args, argCount, result);
    
    return false;
}

#endif // ENABLE(NETSCAPE_PLUGIN_API)
