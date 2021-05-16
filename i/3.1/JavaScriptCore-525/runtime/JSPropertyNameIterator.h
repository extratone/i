/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef JSPropertyNameIterator_h
#define JSPropertyNameIterator_h

#include "JSObject.h"
#include "JSString.h"
#include "PropertyNameArray.h"

namespace JSC {

    class Identifier;
    class JSObject;

    class JSPropertyNameIterator : public JSCell {
    public:
        static JSPropertyNameIterator* create(ExecState*, JSValuePtr);

        virtual ~JSPropertyNameIterator();

        virtual JSValuePtr toPrimitive(ExecState*, PreferredPrimitiveType) const;
        virtual bool getPrimitiveNumber(ExecState*, double&, JSValuePtr&);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual UString toString(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;

        virtual void mark();

        JSValuePtr next(ExecState*);
        void invalidate();

    private:
        JSPropertyNameIterator();
        JSPropertyNameIterator(JSObject*, PassRefPtr<PropertyNameArrayData> propertyNameArrayData);

        JSObject* m_object;
        RefPtr<PropertyNameArrayData> m_data;
        PropertyNameArrayData::const_iterator m_position;
        PropertyNameArrayData::const_iterator m_end;
    };

inline JSPropertyNameIterator::JSPropertyNameIterator()
    : JSCell(0)
    , m_object(0)
    , m_position(0)
    , m_end(0)
{
}

inline JSPropertyNameIterator::JSPropertyNameIterator(JSObject* object, PassRefPtr<PropertyNameArrayData> propertyNameArrayData)
    : JSCell(0)
    , m_object(object)
    , m_data(propertyNameArrayData)
    , m_position(m_data->begin())
    , m_end(m_data->end())
{
}

inline JSPropertyNameIterator* JSPropertyNameIterator::create(ExecState* exec, JSValuePtr v)
{
    if (v.isUndefinedOrNull())
        return new (exec) JSPropertyNameIterator;

    JSObject* o = v.toObject(exec);
    PropertyNameArray propertyNames(exec);
    o->getPropertyNames(exec, propertyNames);
    return new (exec) JSPropertyNameIterator(o, propertyNames.releaseData());
}

inline JSValuePtr JSPropertyNameIterator::next(ExecState* exec)
{
    if (m_position == m_end)
        return noValue();

    if (m_data->cachedStructure() == m_object->structure() && m_data->cachedPrototypeChain() == m_object->structure()->prototypeChain(exec))
        return jsOwnedString(exec, (*m_position++).ustring());

    do {
        if (m_object->hasProperty(exec, *m_position))
            return jsOwnedString(exec, (*m_position++).ustring());
        m_position++;
    } while (m_position != m_end);

    return noValue();
}

} // namespace JSC

#endif // JSPropertyNameIterator_h
