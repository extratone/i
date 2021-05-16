/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#if BINDINGS
#include "config.h"
#include "runtime.h"
#include "function.h"

#include "JSLock.h"
#if NETSCAPE_API
#include "NP_jsobject.h"
#include "c_instance.h"
#include "jni_instance.h"
#endif
#include "objc_instance.h"
#include "runtime_object.h"

namespace KJS { namespace Bindings {

void deleteMethodList(CFAllocatorRef, const void* value)
{
        const MethodList* methodList = static_cast<const MethodList*>(value);
        int end = methodList->length();
        for (int i = 0; i < end; i++) {
            delete methodList->methodAt(i);
        }
        
        delete methodList;
}

void deleteMethod(CFAllocatorRef, const void* value)
{
    delete static_cast<const Method*>(value);
}

void deleteField(CFAllocatorRef, const void* value)
{
    delete static_cast<const Field*>(value);
}

void MethodList::addMethod(Method *aMethod)
{
    Method **_newMethods = new Method *[_length + 1];
    if (_length > 0) {
        memcpy(_newMethods, _methods, sizeof(Method *) * _length);
        delete [] _methods;
    }
    _methods = _newMethods;
    _methods[_length++] = aMethod;
}

unsigned int MethodList::length() const
{
    return _length;
}

Method *MethodList::methodAt(unsigned int index) const
{
    assert(index < _length);
    return _methods[index];
}
    
MethodList::~MethodList()
{
    delete [] _methods;
}

MethodList::MethodList(const MethodList &other)
{
    _length = other._length;
    _methods = new Method *[_length];
    if (_length > 0)
        memcpy (_methods, other._methods, sizeof(Method *) * _length);
}

MethodList &MethodList::operator=(const MethodList &other)
{
    if (this == &other)
        return *this;
            
    delete [] _methods;
    
    _length = other._length;
    _methods = new Method *[_length];
    if (_length > 0)
        memcpy(_methods, other._methods, sizeof(Method *) * _length);

    return *this;
}


Instance::Instance()
: _executionContext(0)
, _refCount(0)
{
}

static KJSDidExecuteFunctionPtr _DidExecuteFunction;

void Instance::setDidExecuteFunction(KJSDidExecuteFunctionPtr func) { _DidExecuteFunction = func; }
KJSDidExecuteFunctionPtr Instance::didExecuteFunction() { return _DidExecuteFunction; }

JSValue *Instance::getValueOfField(ExecState *exec, const Field *aField) const
{
    return aField->valueFromInstance(exec, this);
}

void Instance::setValueOfField(ExecState *exec, const Field *aField, JSValue *aValue) const
{
    aField->setValueToInstance(exec, this, aValue);
}

Instance *Instance::createBindingForLanguageInstance(BindingLanguage language, void *nativeInstance, const RootObject *executionContext)
{
    Instance *newInstance = 0;
    
    switch (language) {
#if BINDINGS_JAVA
	case Instance::JavaLanguage: {
	    newInstance = new Bindings::JavaInstance((jobject)nativeInstance, executionContext);
	    break;
	}
#endif        
	case Instance::ObjectiveCLanguage: {
	    newInstance = new Bindings::ObjcInstance((ObjectStructPtr)nativeInstance);
	    break;
	}
#if NETSCAPE_API
	case Instance::CLanguage: {
	    newInstance = new Bindings::CInstance((NPObject *)nativeInstance);
	    break;
	}
#endif
	default:
	    break;
    }

    if (newInstance)
	newInstance->setExecutionContext(executionContext);
	
    return newInstance;
}

JSObject *Instance::createRuntimeObject(BindingLanguage language, void *nativeInstance, const RootObject *executionContext)
{
    Instance *interfaceObject = Instance::createBindingForLanguageInstance(language, nativeInstance, executionContext);
    
    JSLock lock;
    return new RuntimeObjectImp(interfaceObject);
}

void *Instance::createLanguageInstanceForValue(ExecState*, BindingLanguage language, JSObject* value, const RootObject* origin, const RootObject* current)
{
    void *result = 0;
    
    if (!value->isObject())
	return 0;

#if NETSCAPE_API
    JSObject *imp = static_cast<JSObject*>(value);
#endif
    
    switch (language) {
#if BINDINGS
	case Instance::ObjectiveCLanguage: {
	    result = createObjcInstanceForValue(value, origin, current);
	    break;
	}
#endif
#if NETSCAPE_API
	case Instance::CLanguage: {
	    result = _NPN_CreateScriptObject(0, imp, origin, current);
	    break;
	}
#endif
	case Instance::JavaLanguage: {
	    // FIXME:  factor creation of jni_jsobjects, also remove unnecessary thread
	    // invocation code.
	    break;
	}
	default:
	    break;
    }
    
    return result;
}

} } // namespace KJS::Bindings
#endif //BINDINGS
