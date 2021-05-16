/*
 *  Copyright (C) 1999-2000,2003 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "NumberConstructor.h"

#include "NumberObject.h"
#include "NumberPrototype.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(NumberConstructor);

static JSValuePtr numberConstructorNaNValue(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr numberConstructorNegInfinity(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr numberConstructorPosInfinity(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr numberConstructorMaxValue(ExecState*, const Identifier&, const PropertySlot&);
static JSValuePtr numberConstructorMinValue(ExecState*, const Identifier&, const PropertySlot&);

} // namespace JSC

#include "NumberConstructor.lut.h"

namespace JSC {

const ClassInfo NumberConstructor::info = { "Function", &InternalFunction::info, 0, ExecState::numberTable };

/* Source for NumberConstructor.lut.h
@begin numberTable
   NaN                   numberConstructorNaNValue       DontEnum|DontDelete|ReadOnly
   NEGATIVE_INFINITY     numberConstructorNegInfinity    DontEnum|DontDelete|ReadOnly
   POSITIVE_INFINITY     numberConstructorPosInfinity    DontEnum|DontDelete|ReadOnly
   MAX_VALUE             numberConstructorMaxValue       DontEnum|DontDelete|ReadOnly
   MIN_VALUE             numberConstructorMinValue       DontEnum|DontDelete|ReadOnly
@end
*/

NumberConstructor::NumberConstructor(ExecState* exec, PassRefPtr<Structure> structure, NumberPrototype* numberPrototype)
    : InternalFunction(&exec->globalData(), structure, Identifier(exec, numberPrototype->info.className))
{
    // Number.Prototype
    putDirectWithoutTransition(exec->propertyNames().prototype, numberPrototype, DontEnum | DontDelete | ReadOnly);

    // no. of arguments for constructor
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontEnum | DontDelete);
}

bool NumberConstructor::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<NumberConstructor, InternalFunction>(exec, ExecState::numberTable(exec), this, propertyName, slot);
}

JSValuePtr numberConstructorNaNValue(ExecState* exec, const Identifier&, const PropertySlot&)
{
    return jsNaN(exec);
}

JSValuePtr numberConstructorNegInfinity(ExecState* exec, const Identifier&, const PropertySlot&)
{
    return jsNumber(exec, -Inf);
}

JSValuePtr numberConstructorPosInfinity(ExecState* exec, const Identifier&, const PropertySlot&)
{
    return jsNumber(exec, Inf);
}

JSValuePtr numberConstructorMaxValue(ExecState* exec, const Identifier&, const PropertySlot&)
{
    return jsNumber(exec, 1.7976931348623157E+308);
}

JSValuePtr numberConstructorMinValue(ExecState* exec, const Identifier&, const PropertySlot&)
{
    return jsNumber(exec, 5E-324);
}

// ECMA 15.7.1
static JSObject* constructWithNumberConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    NumberObject* object = new (exec) NumberObject(exec->lexicalGlobalObject()->numberObjectStructure());
    double n = args.isEmpty() ? 0 : args.at(exec, 0).toNumber(exec);
    object->setInternalValue(jsNumber(exec, n));
    return object;
}

ConstructType NumberConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithNumberConstructor;
    return ConstructTypeHost;
}

// ECMA 15.7.2
static JSValuePtr callNumberConstructor(ExecState* exec, JSObject*, JSValuePtr, const ArgList& args)
{
    return jsNumber(exec, args.isEmpty() ? 0 : args.at(exec, 0).toNumber(exec));
}

CallType NumberConstructor::getCallData(CallData& callData)
{
    callData.native.function = callNumberConstructor;
    return CallTypeHost;
}

} // namespace JSC
