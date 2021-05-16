// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "APICast.h"
#include "JSValueRef.h"
#include "JSObjectRef.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"
#include "JSClassRef.h"

#include "identifier.h"
#include "function.h"
#include "nodes.h"
#include "internal.h"
#include "object.h"
#include "PropertyNameArray.h"

using namespace KJS;

JSClassRef JSClassCreate(JSClassDefinition* definition)
{
    JSClassRef jsClass = (definition->attributes & kJSClassAttributeNoPrototype)
        ? OpaqueJSClass::createNoPrototype(definition)
        : OpaqueJSClass::create(definition);
    
    return JSClassRetain(jsClass);
}

JSClassRef JSClassRetain(JSClassRef jsClass)
{
    ++jsClass->refCount;
    return jsClass;
}

void JSClassRelease(JSClassRef jsClass)
{
    if (--jsClass->refCount == 0)
        delete jsClass;
}

JSObjectRef JSObjectMake(JSContextRef ctx, JSClassRef jsClass, void* data)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);

    JSValue* jsPrototype = jsClass 
        ? jsClass->prototype(ctx) 
        : exec->lexicalInterpreter()->builtinObjectPrototype();

    return JSObjectMakeWithPrototype(ctx, jsClass, data, toRef(jsPrototype));
}

JSObjectRef JSObjectMakeWithPrototype(JSContextRef ctx, JSClassRef jsClass, void* data, JSValueRef prototype)
{
    JSLock lock;

    ExecState* exec = toJS(ctx);
    JSValue* jsPrototype = toJS(prototype);

    if (!prototype)
        jsPrototype = exec->lexicalInterpreter()->builtinObjectPrototype();

    if (!jsClass)
        return toRef(new JSObject(jsPrototype)); // slightly more efficient
    
    return toRef(new JSCallbackObject(exec, jsClass, jsPrototype, data));
}

JSObjectRef JSObjectMakeFunctionWithCallback(JSContextRef ctx, JSStringRef name, JSObjectCallAsFunctionCallback callAsFunction)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    Identifier nameID = name ? Identifier(toJS(name)) : Identifier("anonymous");
    
    return toRef(new JSCallbackFunction(exec, callAsFunction, nameID));
}

JSObjectRef JSObjectMakeConstructor(JSContextRef ctx, JSClassRef jsClass, JSObjectCallAsConstructorCallback callAsConstructor)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    
    JSValue* jsPrototype = jsClass 
        ? jsClass->prototype(ctx)
        : exec->dynamicInterpreter()->builtinObjectPrototype();
    
    JSObject* constructor = new JSCallbackConstructor(exec, jsClass, callAsConstructor);
    constructor->put(exec, prototypePropertyName, jsPrototype, DontEnum|DontDelete|ReadOnly);
    return toRef(constructor);
}

JSObjectRef JSObjectMakeFunction(JSContextRef ctx, JSStringRef name, unsigned parameterCount, const JSStringRef parameterNames[], JSStringRef body, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception)
{
    JSLock lock;
    
    ExecState* exec = toJS(ctx);
    UString::Rep* bodyRep = toJS(body);
    UString::Rep* sourceURLRep = sourceURL ? toJS(sourceURL) : &UString::Rep::null;
    
    Identifier nameID = name ? Identifier(toJS(name)) : Identifier("anonymous");
    
    List args;
    for (unsigned i = 0; i < parameterCount; i++)
        args.append(jsString(UString(toJS(parameterNames[i]))));
    args.append(jsString(UString(bodyRep)));

    JSObject* result = exec->dynamicInterpreter()->builtinFunction()->construct(exec, args, nameID, UString(sourceURLRep), startingLineNumber);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
        result = 0;
    }
    return toRef(result);
}

JSValueRef JSObjectGetPrototype(JSContextRef, JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    return toRef(jsObject->prototype());
}

void JSObjectSetPrototype(JSContextRef, JSObjectRef object, JSValueRef value)
{
    JSObject* jsObject = toJS(object);
    JSValue* jsValue = toJS(value);

    jsObject->setPrototype(jsValue);
}

bool JSObjectHasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);
    
    return jsObject->hasProperty(exec, Identifier(nameRep));
}

JSValueRef JSObjectGetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);

    JSValue* jsValue = jsObject->get(exec, Identifier(nameRep));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
    return toRef(jsValue);
}

void JSObjectSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);
    JSValue* jsValue = toJS(value);
    
    jsObject->put(exec, Identifier(nameRep), jsValue, attributes);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
}

JSValueRef JSObjectGetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);

    JSValue* jsValue = jsObject->get(exec, propertyIndex);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
    return toRef(jsValue);
}


void JSObjectSetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef value, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    JSValue* jsValue = toJS(value);
    
    jsObject->put(exec, propertyIndex, jsValue);
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
}

bool JSObjectDeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    UString::Rep* nameRep = toJS(propertyName);

    bool result = jsObject->deleteProperty(exec, Identifier(nameRep));
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
    }
    return result;
}

void* JSObjectGetPrivate(JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    
    if (jsObject->inherits(&JSCallbackObject::info))
        return static_cast<JSCallbackObject*>(jsObject)->getPrivate();
    
    return 0;
}

bool JSObjectSetPrivate(JSObjectRef object, void* data)
{
    JSObject* jsObject = toJS(object);
    
    if (jsObject->inherits(&JSCallbackObject::info)) {
        static_cast<JSCallbackObject*>(jsObject)->setPrivate(data);
        return true;
    }
        
    return false;
}

bool JSObjectIsFunction(JSContextRef, JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    return jsObject->implementsCall();
}

JSValueRef JSObjectCallAsFunction(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    JSObject* jsThisObject = toJS(thisObject);

    if (!jsThisObject)
        jsThisObject = exec->dynamicInterpreter()->globalObject();
    
    List argList;
    for (size_t i = 0; i < argumentCount; i++)
        argList.append(toJS(arguments[i]));

    JSValueRef result = toRef(jsObject->call(exec, jsThisObject, argList)); // returns NULL if object->implementsCall() is false
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
        result = 0;
    }
    return result;
}

bool JSObjectIsConstructor(JSContextRef, JSObjectRef object)
{
    JSObject* jsObject = toJS(object);
    return jsObject->implementsConstruct();
}

JSObjectRef JSObjectCallAsConstructor(JSContextRef ctx, JSObjectRef object, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSLock lock;
    ExecState* exec = toJS(ctx);
    JSObject* jsObject = toJS(object);
    
    List argList;
    for (size_t i = 0; i < argumentCount; i++)
        argList.append(toJS(arguments[i]));
    
    JSObjectRef result = toRef(jsObject->construct(exec, argList)); // returns NULL if object->implementsCall() is false
    if (exec->hadException()) {
        if (exception)
            *exception = toRef(exec->exception());
        exec->clearException();
        result = 0;
    }
    return result;
}

struct OpaqueJSPropertyNameArray
{
    OpaqueJSPropertyNameArray() : refCount(0)
    {
    }
    
    unsigned refCount;
    PropertyNameArray array;
};

JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef ctx, JSObjectRef object)
{
    JSLock lock;
    JSObject* jsObject = toJS(object);
    ExecState* exec = toJS(ctx);
    
    JSPropertyNameArrayRef propertyNames = new OpaqueJSPropertyNameArray();
    jsObject->getPropertyNames(exec, propertyNames->array);
    
    return JSPropertyNameArrayRetain(propertyNames);
}

JSPropertyNameArrayRef JSPropertyNameArrayRetain(JSPropertyNameArrayRef array)
{
    ++array->refCount;
    return array;
}

void JSPropertyNameArrayRelease(JSPropertyNameArrayRef array)
{
    if (--array->refCount == 0)
        delete array;
}

size_t JSPropertyNameArrayGetCount(JSPropertyNameArrayRef array)
{
    return array->array.size();
}

JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef array, size_t index)
{
    return toRef(array->array[index].ustring().rep());
}

void JSPropertyNameAccumulatorAddName(JSPropertyNameAccumulatorRef array, JSStringRef propertyName)
{
    JSLock lock;
    PropertyNameArray* propertyNames = toJS(array);
    UString::Rep* rep = toJS(propertyName);
    
    propertyNames->add(Identifier(rep));
}
