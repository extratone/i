/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSString.h"

#include "JSGlobalObject.h"
#include "JSObject.h"
#include "StringObject.h"
#include "StringPrototype.h"

namespace JSC {

JSValuePtr JSString::toPrimitive(ExecState*, PreferredPrimitiveType) const
{
    return const_cast<JSString*>(this);
}

bool JSString::getPrimitiveNumber(ExecState*, double& number, JSValuePtr& value)
{
    value = this;
    number = m_value.toDouble();
    return false;
}

bool JSString::toBoolean(ExecState*) const
{
    return !m_value.isEmpty();
}

double JSString::toNumber(ExecState*) const
{
    return m_value.toDouble();
}

UString JSString::toString(ExecState*) const
{
    return m_value;
}

UString JSString::toThisString(ExecState*) const
{
    return m_value;
}

JSString* JSString::toThisJSString(ExecState*)
{
    return this;
}

inline StringObject* StringObject::create(ExecState* exec, JSString* string)
{
    return new (exec) StringObject(exec->lexicalGlobalObject()->stringObjectStructure(), string);
}

JSObject* JSString::toObject(ExecState* exec) const
{
    return StringObject::create(exec, const_cast<JSString*>(this));
}

JSObject* JSString::toThisObject(ExecState* exec) const
{
    return StringObject::create(exec, const_cast<JSString*>(this));
}

bool JSString::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // The semantics here are really getPropertySlot, not getOwnPropertySlot.
    // This function should only be called by JSValue::get.
    if (getStringPropertySlot(exec, propertyName, slot))
        return true;
    slot.setBase(this);
    JSObject* object;
    for (JSValuePtr prototype = exec->lexicalGlobalObject()->stringPrototype(); !prototype.isNull(); prototype = object->prototype()) {
        object = asObject(prototype);
        if (object->getOwnPropertySlot(exec, propertyName, slot))
            return true;
    }
    slot.setUndefined();
    return true;
}

bool JSString::getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    // The semantics here are really getPropertySlot, not getOwnPropertySlot.
    // This function should only be called by JSValue::get.
    if (getStringPropertySlot(exec, propertyName, slot))
        return true;
    return JSString::getOwnPropertySlot(exec, Identifier::from(exec, propertyName), slot);
}

JSString* jsString(JSGlobalData* globalData, const UString& s)
{
    int size = s.size();
    if (!size)
        return globalData->smallStrings.emptyString(globalData);
    if (size == 1) {
        UChar c = s.data()[0];
        if (c <= 0xFF)
            return globalData->smallStrings.singleCharacterString(globalData, c);
    }
    return new (globalData) JSString(globalData, s);
}
    
JSString* jsSubstring(JSGlobalData* globalData, const UString& s, unsigned offset, unsigned length)
{
    ASSERT(offset <= static_cast<unsigned>(s.size()));
    ASSERT(length <= static_cast<unsigned>(s.size()));
    ASSERT(offset + length <= static_cast<unsigned>(s.size()));
    if (!length)
        return globalData->smallStrings.emptyString(globalData);
    if (length == 1) {
        UChar c = s.data()[offset];
        if (c <= 0xFF)
            return globalData->smallStrings.singleCharacterString(globalData, c);
    }
    return new (globalData) JSString(globalData, UString::Rep::create(s.rep(), offset, length));
}

JSString* jsOwnedString(JSGlobalData* globalData, const UString& s)
{
    int size = s.size();
    if (!size)
        return globalData->smallStrings.emptyString(globalData);
    if (size == 1) {
        UChar c = s.data()[0];
        if (c <= 0xFF)
            return globalData->smallStrings.singleCharacterString(globalData, c);
    }
    return new (globalData) JSString(globalData, s, JSString::HasOtherOwner);
}

} // namespace JSC
