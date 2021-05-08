/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSString_h
#define JSString_h

#include "CommonIdentifiers.h"
#include "CallFrame.h"
#include "Identifier.h"
#include "JSNumberCell.h"
#include "PropertySlot.h"

namespace JSC {

    class JSString;

    JSString* jsEmptyString(JSGlobalData*);
    JSString* jsEmptyString(ExecState*);
    JSString* jsString(JSGlobalData*, const UString&); // returns empty string if passed null string
    JSString* jsString(ExecState*, const UString&); // returns empty string if passed null string

    JSString* jsSingleCharacterString(JSGlobalData*, UChar);
    JSString* jsSingleCharacterString(ExecState*, UChar);
    JSString* jsSingleCharacterSubstring(JSGlobalData*, const UString&, unsigned offset);
    JSString* jsSingleCharacterSubstring(ExecState*, const UString&, unsigned offset);
    JSString* jsSubstring(JSGlobalData*, const UString&, unsigned offset, unsigned length);
    JSString* jsSubstring(ExecState*, const UString&, unsigned offset, unsigned length);

    // Non-trivial strings are two or more characters long.
    // These functions are faster than just calling jsString.
    JSString* jsNontrivialString(JSGlobalData*, const UString&);
    JSString* jsNontrivialString(ExecState*, const UString&);
    JSString* jsNontrivialString(JSGlobalData*, const char*);
    JSString* jsNontrivialString(ExecState*, const char*);

    // Should be used for strings that are owned by an object that will
    // likely outlive the JSValue this makes, such as the parse tree or a
    // DOM object that contains a UString
    JSString* jsOwnedString(JSGlobalData*, const UString&); 
    JSString* jsOwnedString(ExecState*, const UString&); 

    class JSString : public JSCell {
        friend class JIT;
        friend class Interpreter;

    public:
        JSString(JSGlobalData* globalData, const UString& value)
            : JSCell(globalData->stringStructure.get())
            , m_value(value)
        {
            Heap::heap(this)->reportExtraMemoryCost(value.cost());
        }

        enum HasOtherOwnerType { HasOtherOwner };
        JSString(JSGlobalData* globalData, const UString& value, HasOtherOwnerType)
            : JSCell(globalData->stringStructure.get())
            , m_value(value)
        {
        }
        JSString(JSGlobalData* globalData, PassRefPtr<UString::Rep> value, HasOtherOwnerType)
            : JSCell(globalData->stringStructure.get())
            , m_value(value)
        {
        }
        
        const UString& value() const { return m_value; }

        bool getStringPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        bool getStringPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        bool canGetIndex(unsigned i) { return i < static_cast<unsigned>(m_value.size()); }
        JSString* getIndex(JSGlobalData*, unsigned);

        static PassRefPtr<Structure> createStructure(JSValuePtr proto) { return Structure::create(proto, TypeInfo(StringType, NeedsThisConversion)); }

    private:
        enum VPtrStealingHackType { VPtrStealingHack };
        JSString(VPtrStealingHackType) 
            : JSCell(0)
        {
        }

        virtual JSValuePtr toPrimitive(ExecState*, PreferredPrimitiveType) const;
        virtual bool getPrimitiveNumber(ExecState*, double& number, JSValuePtr& value);
        virtual bool toBoolean(ExecState*) const;
        virtual double toNumber(ExecState*) const;
        virtual JSObject* toObject(ExecState*) const;
        virtual UString toString(ExecState*) const;

        virtual JSObject* toThisObject(ExecState*) const;
        virtual UString toThisString(ExecState*) const;
        virtual JSString* toThisJSString(ExecState*);

        // Actually getPropertySlot, not getOwnPropertySlot (see JSCell).
        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);

        UString m_value;
    };

    JSString* asString(JSValuePtr);

    inline JSString* asString(JSValuePtr value)
    {
        ASSERT(asCell(value)->isString());
        return static_cast<JSString*>(asCell(value));
    }

    inline JSString* jsEmptyString(JSGlobalData* globalData)
    {
        return globalData->smallStrings.emptyString(globalData);
    }

    inline JSString* jsSingleCharacterString(JSGlobalData* globalData, UChar c)
    {
        if (c <= 0xFF)
            return globalData->smallStrings.singleCharacterString(globalData, c);
        return new (globalData) JSString(globalData, UString(&c, 1));
    }

    inline JSString* jsSingleCharacterSubstring(JSGlobalData* globalData, const UString& s, unsigned offset)
    {
        ASSERT(offset < static_cast<unsigned>(s.size()));
        UChar c = s.data()[offset];
        if (c <= 0xFF)
            return globalData->smallStrings.singleCharacterString(globalData, c);
        return new (globalData) JSString(globalData, UString::Rep::create(s.rep(), offset, 1));
    }

    inline JSString* jsNontrivialString(JSGlobalData* globalData, const char* s)
    {
        ASSERT(s);
        ASSERT(s[0]);
        ASSERT(s[1]);
        return new (globalData) JSString(globalData, s);
    }

    inline JSString* jsNontrivialString(JSGlobalData* globalData, const UString& s)
    {
        ASSERT(s.size() > 1);
        return new (globalData) JSString(globalData, s);
    }

    inline JSString* JSString::getIndex(JSGlobalData* globalData, unsigned i)
    {
        ASSERT(canGetIndex(i));
        return jsSingleCharacterSubstring(globalData, m_value, i);
    }

    inline JSString* jsEmptyString(ExecState* exec) { return jsEmptyString(&exec->globalData()); }
    inline JSString* jsString(ExecState* exec, const UString& s) { return jsString(&exec->globalData(), s); }
    inline JSString* jsSingleCharacterString(ExecState* exec, UChar c) { return jsSingleCharacterString(&exec->globalData(), c); }
    inline JSString* jsSingleCharacterSubstring(ExecState* exec, const UString& s, unsigned offset) { return jsSingleCharacterSubstring(&exec->globalData(), s, offset); }
    inline JSString* jsSubstring(ExecState* exec, const UString& s, unsigned offset, unsigned length) { return jsSubstring(&exec->globalData(), s, offset, length); }
    inline JSString* jsNontrivialString(ExecState* exec, const UString& s) { return jsNontrivialString(&exec->globalData(), s); }
    inline JSString* jsNontrivialString(ExecState* exec, const char* s) { return jsNontrivialString(&exec->globalData(), s); }
    inline JSString* jsOwnedString(ExecState* exec, const UString& s) { return jsOwnedString(&exec->globalData(), s); } 

    ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
    {
        if (propertyName == exec->propertyNames().length) {
            slot.setValue(jsNumber(exec, m_value.size()));
            return true;
        }

        bool isStrictUInt32;
        unsigned i = propertyName.toStrictUInt32(&isStrictUInt32);
        if (isStrictUInt32 && i < static_cast<unsigned>(m_value.size())) {
            slot.setValue(jsSingleCharacterSubstring(exec, m_value, i));
            return true;
        }

        return false;
    }
        
    ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
    {
        if (propertyName < static_cast<unsigned>(m_value.size())) {
            slot.setValue(jsSingleCharacterSubstring(exec, m_value, propertyName));
            return true;
        }

        return false;
    }

    // --- JSValue inlines ----------------------------

    inline JSString* JSValuePtr::toThisJSString(ExecState* exec)
    {
        return JSImmediate::isImmediate(asValue()) ? jsString(exec, JSImmediate::toString(asValue())) : asCell()->toThisJSString(exec);
    }

} // namespace JSC

#endif // JSString_h
