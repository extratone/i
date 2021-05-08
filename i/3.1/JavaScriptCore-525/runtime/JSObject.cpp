/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel (eric@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "JSObject.h"

#include "DatePrototype.h"
#include "ErrorConstructor.h"
#include "GetterSetter.h"
#include "JSGlobalObject.h"
#include "NativeErrorConstructor.h"
#include "ObjectPrototype.h"
#include "PropertyNameArray.h"
#include "Lookup.h"
#include "Nodes.h"
#include "Operations.h"
#include <math.h>
#include <wtf/Assertions.h>

#define JSOBJECT_MARK_TRACING 0

#if JSOBJECT_MARK_TRACING

#define JSOBJECT_MARK_BEGIN() \
    static int markStackDepth = 0; \
    for (int i = 0; i < markStackDepth; i++) \
        putchar('-'); \
    printf("%s (%p)\n", className().UTF8String().c_str(), this); \
    markStackDepth++; \

#define JSOBJECT_MARK_END() \
    markStackDepth--;

#else // JSOBJECT_MARK_TRACING

#define JSOBJECT_MARK_BEGIN()
#define JSOBJECT_MARK_END()

#endif // JSOBJECT_MARK_TRACING

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSObject);

void JSObject::mark()
{
    JSOBJECT_MARK_BEGIN();

    JSCell::mark();
    m_structure->mark();

    size_t storageSize = m_structure->propertyStorageSize();
    for (size_t i = 0; i < storageSize; ++i) {
        JSValuePtr v = m_propertyStorage[i];
        if (!v.marked())
            v.mark();
    }

    JSOBJECT_MARK_END();
}

UString JSObject::className() const
{
    const ClassInfo* info = classInfo();
    if (info)
        return info->className;
    return "Object";
}

bool JSObject::getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    return getOwnPropertySlot(exec, Identifier::from(exec, propertyName), slot);
}

static void throwSetterError(ExecState* exec)
{
    throwError(exec, TypeError, "setting a property that has only a getter");
}

// ECMA 8.6.2.2
void JSObject::put(ExecState* exec, const Identifier& propertyName, JSValuePtr value, PutPropertySlot& slot)
{
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    if (propertyName == exec->propertyNames().underscoreProto) {
        // Setting __proto__ to a non-object, non-null value is silently ignored to match Mozilla.
        if (!value.isObject() && !value.isNull())
            return;

        JSValuePtr nextPrototypeValue = value;
        while (nextPrototypeValue && nextPrototypeValue.isObject()) {
            JSObject* nextPrototype = asObject(nextPrototypeValue)->unwrappedObject();
            if (nextPrototype == this) {
                throwError(exec, GeneralError, "cyclic __proto__ value");
                return;
            }
            nextPrototypeValue = nextPrototype->prototype();
        }

        setPrototype(value);
        return;
    }

    // Check if there are any setters or getters in the prototype chain
    JSValuePtr prototype;
    for (JSObject* obj = this; !obj->structure()->hasGetterSetterProperties(); obj = asObject(prototype)) {
        prototype = obj->prototype();
        if (prototype.isNull()) {
            putDirect(propertyName, value, 0, true, slot);
            return;
        }
    }
    
    unsigned attributes;
    if ((m_structure->get(propertyName, attributes) != WTF::notFound) && attributes & ReadOnly)
        return;

    for (JSObject* obj = this; ; obj = asObject(prototype)) {
        if (JSValuePtr gs = obj->getDirect(propertyName)) {
            if (gs.isGetterSetter()) {
                JSObject* setterFunc = asGetterSetter(gs)->setter();        
                if (!setterFunc) {
                    throwSetterError(exec);
                    return;
                }
                
                CallData callData;
                CallType callType = setterFunc->getCallData(callData);
                ArgList args;
                args.append(value);
                call(exec, setterFunc, callType, callData, this, args);
                return;
            }

            // If there's an existing property on the object or one of its 
            // prototypes it should be replaced, so break here.
            break;
        }

        prototype = obj->prototype();
        if (prototype.isNull())
            break;
    }

    putDirect(propertyName, value, 0, true, slot);
    return;
}

void JSObject::put(ExecState* exec, unsigned propertyName, JSValuePtr value)
{
    PutPropertySlot slot;
    put(exec, Identifier::from(exec, propertyName), value, slot);
}

void JSObject::putWithAttributes(ExecState*, const Identifier& propertyName, JSValuePtr value, unsigned attributes)
{
    putDirect(propertyName, value, attributes);
}

void JSObject::putWithAttributes(ExecState* exec, unsigned propertyName, JSValuePtr value, unsigned attributes)
{
    putWithAttributes(exec, Identifier::from(exec, propertyName), value, attributes);
}

bool JSObject::hasProperty(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot;
    return const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot);
}

bool JSObject::hasProperty(ExecState* exec, unsigned propertyName) const
{
    PropertySlot slot;
    return const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot);
}

// ECMA 8.6.2.5
bool JSObject::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    unsigned attributes;
    if (m_structure->get(propertyName, attributes) != WTF::notFound) {
        if ((attributes & DontDelete))
            return false;
        removeDirect(propertyName);
        return true;
    }

    // Look in the static hashtable of properties
    const HashEntry* entry = findPropertyHashEntry(exec, propertyName);
    if (entry && entry->attributes() & DontDelete)
        return false; // this builtin property can't be deleted

    // FIXME: Should the code here actually do some deletion?
    return true;
}

bool JSObject::hasOwnProperty(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot;
    return const_cast<JSObject*>(this)->getOwnPropertySlot(exec, propertyName, slot);
}

bool JSObject::deleteProperty(ExecState* exec, unsigned propertyName)
{
    return deleteProperty(exec, Identifier::from(exec, propertyName));
}

static ALWAYS_INLINE JSValuePtr callDefaultValueFunction(ExecState* exec, const JSObject* object, const Identifier& propertyName)
{
    JSValuePtr function = object->get(exec, propertyName);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return exec->exception();

    // Prevent "toString" and "valueOf" from observing execution if an exception
    // is pending.
    if (exec->hadException())
        return exec->exception();

    JSValuePtr result = call(exec, function, callType, callData, const_cast<JSObject*>(object), exec->emptyList());
    ASSERT(!result.isGetterSetter());
    if (exec->hadException())
        return exec->exception();
    if (result.isObject())
        return noValue();
    return result;
}

bool JSObject::getPrimitiveNumber(ExecState* exec, double& number, JSValuePtr& result)
{
    result = defaultValue(exec, PreferNumber);
    number = result.toNumber(exec);
    return !result.isString();
}

// ECMA 8.6.2.6
JSValuePtr JSObject::defaultValue(ExecState* exec, PreferredPrimitiveType hint) const
{
    // Must call toString first for Date objects.
    if ((hint == PreferString) || (hint != PreferNumber && prototype() == exec->lexicalGlobalObject()->datePrototype())) {
        JSValuePtr value = callDefaultValueFunction(exec, this, exec->propertyNames().toString);
        if (value)
            return value;
        value = callDefaultValueFunction(exec, this, exec->propertyNames().valueOf);
        if (value)
            return value;
    } else {
        JSValuePtr value = callDefaultValueFunction(exec, this, exec->propertyNames().valueOf);
        if (value)
            return value;
        value = callDefaultValueFunction(exec, this, exec->propertyNames().toString);
        if (value)
            return value;
    }

    ASSERT(!exec->hadException());

    return throwError(exec, TypeError, "No default value");
}

const HashEntry* JSObject::findPropertyHashEntry(ExecState* exec, const Identifier& propertyName) const
{
    for (const ClassInfo* info = classInfo(); info; info = info->parentClass) {
        if (const HashTable* propHashTable = info->propHashTable(exec)) {
            if (const HashEntry* entry = propHashTable->entry(exec, propertyName))
                return entry;
        }
    }
    return 0;
}

void JSObject::defineGetter(ExecState* exec, const Identifier& propertyName, JSObject* getterFunction)
{
    JSValuePtr object = getDirect(propertyName);
    if (object && object.isGetterSetter()) {
        ASSERT(m_structure->hasGetterSetterProperties());
        asGetterSetter(object)->setGetter(getterFunction);
        return;
    }

    PutPropertySlot slot;
    GetterSetter* getterSetter = new (exec) GetterSetter;
    putDirect(propertyName, getterSetter, None, true, slot);

    // putDirect will change our Structure if we add a new property. For
    // getters and setters, though, we also need to change our Structure
    // if we override an existing non-getter or non-setter.
    if (slot.type() != PutPropertySlot::NewProperty) {
        if (!m_structure->isDictionary()) {
            RefPtr<Structure> structure = Structure::getterSetterTransition(m_structure);
            setStructure(structure.release());
        }
    }

    m_structure->setHasGetterSetterProperties(true);
    getterSetter->setGetter(getterFunction);
}

void JSObject::defineSetter(ExecState* exec, const Identifier& propertyName, JSObject* setterFunction)
{
    JSValuePtr object = getDirect(propertyName);
    if (object && object.isGetterSetter()) {
        ASSERT(m_structure->hasGetterSetterProperties());
        asGetterSetter(object)->setSetter(setterFunction);
        return;
    }

    PutPropertySlot slot;
    GetterSetter* getterSetter = new (exec) GetterSetter;
    putDirect(propertyName, getterSetter, None, true, slot);

    // putDirect will change our Structure if we add a new property. For
    // getters and setters, though, we also need to change our Structure
    // if we override an existing non-getter or non-setter.
    if (slot.type() != PutPropertySlot::NewProperty) {
        if (!m_structure->isDictionary()) {
            RefPtr<Structure> structure = Structure::getterSetterTransition(m_structure);
            setStructure(structure.release());
        }
    }

    m_structure->setHasGetterSetterProperties(true);
    getterSetter->setSetter(setterFunction);
}

JSValuePtr JSObject::lookupGetter(ExecState*, const Identifier& propertyName)
{
    JSObject* object = this;
    while (true) {
        if (JSValuePtr value = object->getDirect(propertyName)) {
            if (!value.isGetterSetter())
                return jsUndefined();
            JSObject* functionObject = asGetterSetter(value)->getter();
            if (!functionObject)
                return jsUndefined();
            return functionObject;
        }

        if (!object->prototype() || !object->prototype().isObject())
            return jsUndefined();
        object = asObject(object->prototype());
    }
}

JSValuePtr JSObject::lookupSetter(ExecState*, const Identifier& propertyName)
{
    JSObject* object = this;
    while (true) {
        if (JSValuePtr value = object->getDirect(propertyName)) {
            if (!value.isGetterSetter())
                return jsUndefined();
            JSObject* functionObject = asGetterSetter(value)->setter();
            if (!functionObject)
                return jsUndefined();
            return functionObject;
        }

        if (!object->prototype() || !object->prototype().isObject())
            return jsUndefined();
        object = asObject(object->prototype());
    }
}

bool JSObject::hasInstance(ExecState* exec, JSValuePtr value, JSValuePtr proto)
{
    if (!proto.isObject()) {
        throwError(exec, TypeError, "instanceof called on an object with an invalid prototype property.");
        return false;
    }

    if (!value.isObject())
        return false;

    JSObject* object = asObject(value);
    while ((object = object->prototype().getObject())) {
        if (proto == object)
            return true;
    }
    return false;
}

bool JSObject::propertyIsEnumerable(ExecState* exec, const Identifier& propertyName) const
{
    unsigned attributes;
    if (!getPropertyAttributes(exec, propertyName, attributes))
        return false;
    return !(attributes & DontEnum);
}

bool JSObject::getPropertyAttributes(ExecState* exec, const Identifier& propertyName, unsigned& attributes) const
{
    if (m_structure->get(propertyName, attributes) != WTF::notFound)
        return true;
    
    // Look in the static hashtable of properties
    const HashEntry* entry = findPropertyHashEntry(exec, propertyName);
    if (entry) {
        attributes = entry->attributes();
        return true;
    }
    
    return false;
}

void JSObject::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    m_structure->getEnumerablePropertyNames(exec, propertyNames, this);
}

bool JSObject::toBoolean(ExecState*) const
{
    return true;
}

double JSObject::toNumber(ExecState* exec) const
{
    JSValuePtr primitive = toPrimitive(exec, PreferNumber);
    if (exec->hadException()) // should be picked up soon in Nodes.cpp
        return 0.0;
    return primitive.toNumber(exec);
}

UString JSObject::toString(ExecState* exec) const
{
    JSValuePtr primitive = toPrimitive(exec, PreferString);
    if (exec->hadException())
        return "";
    return primitive.toString(exec);
}

JSObject* JSObject::toObject(ExecState*) const
{
    return const_cast<JSObject*>(this);
}

JSObject* JSObject::toThisObject(ExecState*) const
{
    return const_cast<JSObject*>(this);
}

JSObject* JSObject::unwrappedObject()
{
    return this;
}

void JSObject::removeDirect(const Identifier& propertyName)
{
    size_t offset;
    if (m_structure->isDictionary()) {
        offset = m_structure->removePropertyWithoutTransition(propertyName);
        if (offset != WTF::notFound)
            m_propertyStorage[offset] = jsUndefined();
        return;
    }

    RefPtr<Structure> structure = Structure::removePropertyTransition(m_structure, propertyName, offset);
    if (offset != WTF::notFound)
        m_propertyStorage[offset] = jsUndefined();
    setStructure(structure.release());
}

void JSObject::putDirectFunction(ExecState* exec, InternalFunction* function, unsigned attr)
{
    putDirect(Identifier(exec, function->name(&exec->globalData())), function, attr);
}

void JSObject::putDirectFunctionWithoutTransition(ExecState* exec, InternalFunction* function, unsigned attr)
{
    putDirectWithoutTransition(Identifier(exec, function->name(&exec->globalData())), function, attr);
}

NEVER_INLINE void JSObject::fillGetterPropertySlot(PropertySlot& slot, JSValuePtr* location)
{
    if (JSObject* getterFunction = asGetterSetter(*location)->getter())
        slot.setGetterSlot(getterFunction);
    else
        slot.setUndefined();
}

Structure* JSObject::createInheritorID()
{
    m_inheritorID = JSObject::createStructure(this);
    return m_inheritorID.get();
}

void JSObject::allocatePropertyStorage(size_t oldSize, size_t newSize)
{
    allocatePropertyStorageInline(oldSize, newSize);
}

JSObject* constructEmptyObject(ExecState* exec)
{
    return new (exec) JSObject(exec->lexicalGlobalObject()->emptyObjectStructure());
}

} // namespace JSC
