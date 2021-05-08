/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "RegExpConstructor.h"

#include "ArrayPrototype.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "RegExpMatchesArray.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "RegExp.h"

namespace JSC {

static JSValuePtr regExpConstructorInput(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorMultiline(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorLastMatch(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorLastParen(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorLeftContext(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorRightContext(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorDollar1(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorDollar2(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorDollar3(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorDollar4(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorDollar5(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorDollar6(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorDollar7(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorDollar8(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr regExpConstructorDollar9(ExecState*, const Identifier&, const PropertySlot&);

static void setRegExpConstructorInput(ExecState*, JSObject*, JSValuePtr);
static void setRegExpConstructorMultiline(ExecState*, JSObject*, JSValuePtr);

} // namespace JSC

#include "RegExpConstructor.lut.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(RegExpConstructor);

const ClassInfo RegExpConstructor::info = { "Function", &InternalFunction::info, 0, ExecState::regExpConstructorTable };

/* Source for RegExpConstructor.lut.h
@begin regExpConstructorTable
    input           regExpConstructorInput          None
    $_              regExpConstructorInput          DontEnum
    multiline       regExpConstructorMultiline      None
    $*              regExpConstructorMultiline      DontEnum
    lastMatch       regExpConstructorLastMatch      DontDelete|ReadOnly
    $&              regExpConstructorLastMatch      DontDelete|ReadOnly|DontEnum
    lastParen       regExpConstructorLastParen      DontDelete|ReadOnly
    $+              regExpConstructorLastParen      DontDelete|ReadOnly|DontEnum
    leftContext     regExpConstructorLeftContext    DontDelete|ReadOnly
    $`              regExpConstructorLeftContext    DontDelete|ReadOnly|DontEnum
    rightContext    regExpConstructorRightContext   DontDelete|ReadOnly
    $'              regExpConstructorRightContext   DontDelete|ReadOnly|DontEnum
    $1              regExpConstructorDollar1        DontDelete|ReadOnly
    $2              regExpConstructorDollar2        DontDelete|ReadOnly
    $3              regExpConstructorDollar3        DontDelete|ReadOnly
    $4              regExpConstructorDollar4        DontDelete|ReadOnly
    $5              regExpConstructorDollar5        DontDelete|ReadOnly
    $6              regExpConstructorDollar6        DontDelete|ReadOnly
    $7              regExpConstructorDollar7        DontDelete|ReadOnly
    $8              regExpConstructorDollar8        DontDelete|ReadOnly
    $9              regExpConstructorDollar9        DontDelete|ReadOnly
@end
*/

struct RegExpConstructorPrivate {
    // Global search cache / settings
    RegExpConstructorPrivate()
        : lastNumSubPatterns(0)
        , multiline(false)
    {
    }

    UString input;
    UString lastInput;
    OwnArrayPtr<int> lastOvector;
    unsigned lastNumSubPatterns : 31;
    bool multiline : 1;
};

RegExpConstructor::RegExpConstructor(ExecState* exec, PassRefPtr<Structure> structure, RegExpPrototype* regExpPrototype)
    : InternalFunction(&exec->globalData(), structure, Identifier(exec, "RegExp"))
    , d(new RegExpConstructorPrivate)
{
    // ECMA 15.10.5.1 RegExp.prototype
    putDirectWithoutTransition(exec->propertyNames().prototype, regExpPrototype, DontEnum | DontDelete | ReadOnly);

    // no. of arguments for constructor
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 2), ReadOnly | DontDelete | DontEnum);
}

/* 
  To facilitate result caching, exec(), test(), match(), search(), and replace() dipatch regular
  expression matching through the performMatch function. We use cached results to calculate, 
  e.g., RegExp.lastMatch and RegExp.leftParen.
*/
void RegExpConstructor::performMatch(RegExp* r, const UString& s, int startOffset, int& position, int& length, int** ovector)
{
    OwnArrayPtr<int> tmpOvector;
    position = r->match(s, startOffset, &tmpOvector);

    if (ovector)
        *ovector = tmpOvector.get();

    if (position != -1) {
        ASSERT(tmpOvector);

        length = tmpOvector[1] - tmpOvector[0];

        d->input = s;
        d->lastInput = s;
        d->lastOvector.set(tmpOvector.release());
        d->lastNumSubPatterns = r->numSubpatterns();
    }
}

RegExpMatchesArray::RegExpMatchesArray(ExecState* exec, RegExpConstructorPrivate* data)
    : JSArray(exec->lexicalGlobalObject()->regExpMatchesArrayStructure(), data->lastNumSubPatterns + 1)
{
    RegExpConstructorPrivate* d = new RegExpConstructorPrivate;
    d->input = data->lastInput;
    d->lastInput = data->lastInput;
    d->lastNumSubPatterns = data->lastNumSubPatterns;
    unsigned offsetVectorSize = (data->lastNumSubPatterns + 1) * 2; // only copying the result part of the vector
    d->lastOvector.set(new int[offsetVectorSize]);
    memcpy(d->lastOvector.get(), data->lastOvector.get(), offsetVectorSize * sizeof(int));
    // d->multiline is not needed, and remains uninitialized

    setLazyCreationData(d);
}

RegExpMatchesArray::~RegExpMatchesArray()
{
    delete static_cast<RegExpConstructorPrivate*>(lazyCreationData());
}

void RegExpMatchesArray::fillArrayInstance(ExecState* exec)
{
    RegExpConstructorPrivate* d = static_cast<RegExpConstructorPrivate*>(lazyCreationData());
    ASSERT(d);

    unsigned lastNumSubpatterns = d->lastNumSubPatterns;

    for (unsigned i = 0; i <= lastNumSubpatterns; ++i) {
        int start = d->lastOvector[2 * i];
        if (start >= 0)
            JSArray::put(exec, i, jsSubstring(exec, d->lastInput, start, d->lastOvector[2 * i + 1] - start));
    }

    PutPropertySlot slot;
    JSArray::put(exec, exec->propertyNames().index, jsNumber(exec, d->lastOvector[0]), slot);
    JSArray::put(exec, exec->propertyNames().input, jsString(exec, d->input), slot);

    delete d;
    setLazyCreationData(0);
}

JSObject* RegExpConstructor::arrayOfMatches(ExecState* exec) const
{
    return new (exec) RegExpMatchesArray(exec, d.get());
}

JSValuePtr RegExpConstructor::getBackref(ExecState* exec, unsigned i) const
{
    if (d->lastOvector && i <= d->lastNumSubPatterns) {
        int start = d->lastOvector[2 * i];
        if (start >= 0)
            return jsSubstring(exec, d->lastInput, start, d->lastOvector[2 * i + 1] - start);
    }
    return jsEmptyString(exec);
}

JSValuePtr RegExpConstructor::getLastParen(ExecState* exec) const
{
    unsigned i = d->lastNumSubPatterns;
    if (i > 0) {
        ASSERT(d->lastOvector);
        int start = d->lastOvector[2 * i];
        if (start >= 0)
            return jsSubstring(exec, d->lastInput, start, d->lastOvector[2 * i + 1] - start);
    }
    return jsEmptyString(exec);
}

JSValuePtr RegExpConstructor::getLeftContext(ExecState* exec) const
{
    if (d->lastOvector)
        return jsSubstring(exec, d->lastInput, 0, d->lastOvector[0]);
    return jsEmptyString(exec);
}

JSValuePtr RegExpConstructor::getRightContext(ExecState* exec) const
{
    if (d->lastOvector)
        return jsSubstring(exec, d->lastInput, d->lastOvector[1], d->lastInput.size() - d->lastOvector[1]);
    return jsEmptyString(exec);
}
    
bool RegExpConstructor::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<RegExpConstructor, InternalFunction>(exec, ExecState::regExpConstructorTable(exec), this, propertyName, slot);
}

JSValuePtr regExpConstructorDollar1(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getBackref(exec, 1);
}

JSValuePtr regExpConstructorDollar2(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getBackref(exec, 2);
}

JSValuePtr regExpConstructorDollar3(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getBackref(exec, 3);
}

JSValuePtr regExpConstructorDollar4(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getBackref(exec, 4);
}

JSValuePtr regExpConstructorDollar5(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getBackref(exec, 5);
}

JSValuePtr regExpConstructorDollar6(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getBackref(exec, 6);
}

JSValuePtr regExpConstructorDollar7(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getBackref(exec, 7);
}

JSValuePtr regExpConstructorDollar8(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getBackref(exec, 8);
}

JSValuePtr regExpConstructorDollar9(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getBackref(exec, 9);
}

JSValuePtr regExpConstructorInput(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return jsString(exec, asRegExpConstructor(slot.slotBase())->input());
}

JSValuePtr regExpConstructorMultiline(ExecState*, const Identifier&, const PropertySlot& slot)
{
    return jsBoolean(asRegExpConstructor(slot.slotBase())->multiline());
}

JSValuePtr regExpConstructorLastMatch(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getBackref(exec, 0);
}

JSValuePtr regExpConstructorLastParen(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getLastParen(exec);
}

JSValuePtr regExpConstructorLeftContext(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getLeftContext(exec);
}

JSValuePtr regExpConstructorRightContext(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return asRegExpConstructor(slot.slotBase())->getRightContext(exec);
}

void RegExpConstructor::put(ExecState* exec, const Identifier& propertyName, JSValuePtr value, PutPropertySlot& slot)
{
    lookupPut<RegExpConstructor, InternalFunction>(exec, propertyName, value, ExecState::regExpConstructorTable(exec), this, slot);
}

void setRegExpConstructorInput(ExecState* exec, JSObject* baseObject, JSValuePtr value)
{
    asRegExpConstructor(baseObject)->setInput(value.toString(exec));
}

void setRegExpConstructorMultiline(ExecState* exec, JSObject* baseObject, JSValuePtr value)
{
    asRegExpConstructor(baseObject)->setMultiline(value.toBoolean(exec));
}
  
// ECMA 15.10.4
JSObject* constructRegExp(ExecState* exec, const ArgList& args)
{
    JSValuePtr arg0 = args.at(exec, 0);
    JSValuePtr arg1 = args.at(exec, 1);

    if (arg0.isObject(&RegExpObject::info)) {
        if (!arg1.isUndefined())
            return throwError(exec, TypeError, "Cannot supply flags when constructing one RegExp from another.");
        return asObject(arg0);
    }

    UString pattern = arg0.isUndefined() ? UString("") : arg0.toString(exec);
    UString flags = arg1.isUndefined() ? UString("") : arg1.toString(exec);

    RefPtr<RegExp> regExp = RegExp::create(&exec->globalData(), pattern, flags);
    if (!regExp->isValid())
        return throwError(exec, SyntaxError, UString("Invalid regular expression: ").append(regExp->errorMessage()));
    return new (exec) RegExpObject(exec->lexicalGlobalObject()->regExpStructure(), regExp.release());
}

static JSObject* constructWithRegExpConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    return constructRegExp(exec, args);
}

ConstructType RegExpConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithRegExpConstructor;
    return ConstructTypeHost;
}

// ECMA 15.10.3
static JSValuePtr callRegExpConstructor(ExecState* exec, JSObject*, JSValuePtr, const ArgList& args)
{
    return constructRegExp(exec, args);
}

CallType RegExpConstructor::getCallData(CallData& callData)
{
    callData.native.function = callRegExpConstructor;
    return CallTypeHost;
}

void RegExpConstructor::setInput(const UString& input)
{
    d->input = input;
}

const UString& RegExpConstructor::input() const
{
    // Can detect a distinct initial state that is invisible to JavaScript, by checking for null
    // state (since jsString turns null strings to empty strings).
    return d->input;
}

void RegExpConstructor::setMultiline(bool multiline)
{
    d->multiline = multiline;
}

bool RegExpConstructor::multiline() const
{
    return d->multiline;
}

} // namespace JSC
