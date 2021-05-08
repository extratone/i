/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "DateConstructor.h"

#include "DateInstance.h"
#include "DateMath.h"
#include "DatePrototype.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "PrototypeFunction.h"
#include <math.h>
#include <time.h>
#include <wtf/MathExtras.h>

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SYS_TIMEB_H)
#include <sys/timeb.h>
#endif

namespace JSC {

// TODO: MakeTime (15.9.11.1) etc. ?

ASSERT_CLASS_FITS_IN_CELL(DateConstructor);

static JSValuePtr dateParse(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateNow(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr dateUTC(ExecState*, JSObject*, JSValuePtr, const ArgList&);

DateConstructor::DateConstructor(ExecState* exec, PassRefPtr<Structure> structure, Structure* prototypeFunctionStructure, DatePrototype* datePrototype)
    : InternalFunction(&exec->globalData(), structure, Identifier(exec, datePrototype->classInfo()->className))
{
      putDirectWithoutTransition(exec->propertyNames().prototype, datePrototype, DontEnum|DontDelete|ReadOnly);

      putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 1, exec->propertyNames().parse, dateParse), DontEnum);
      putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 7, exec->propertyNames().UTC, dateUTC), DontEnum);
      putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 0, exec->propertyNames().now, dateNow), DontEnum);

      putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 7), ReadOnly | DontEnum | DontDelete);
}

// ECMA 15.9.3
JSObject* constructDate(ExecState* exec, const ArgList& args)
{
    int numArgs = args.size();

    double value;

    if (numArgs == 0) // new Date() ECMA 15.9.3.3
        value = getCurrentUTCTime();
    else if (numArgs == 1) {
        if (args.at(exec, 0).isObject(&DateInstance::info))
            value = asDateInstance(args.at(exec, 0))->internalNumber();
        else {
            JSValuePtr primitive = args.at(exec, 0).toPrimitive(exec);
            if (primitive.isString())
                value = parseDate(primitive.getString());
            else
                value = primitive.toNumber(exec);
        }
    } else {
        if (isnan(args.at(exec, 0).toNumber(exec))
                || isnan(args.at(exec, 1).toNumber(exec))
                || (numArgs >= 3 && isnan(args.at(exec, 2).toNumber(exec)))
                || (numArgs >= 4 && isnan(args.at(exec, 3).toNumber(exec)))
                || (numArgs >= 5 && isnan(args.at(exec, 4).toNumber(exec)))
                || (numArgs >= 6 && isnan(args.at(exec, 5).toNumber(exec)))
                || (numArgs >= 7 && isnan(args.at(exec, 6).toNumber(exec))))
            value = NaN;
        else {
          GregorianDateTime t;
          int year = args.at(exec, 0).toInt32(exec);
          t.year = (year >= 0 && year <= 99) ? year : year - 1900;
          t.month = args.at(exec, 1).toInt32(exec);
          t.monthDay = (numArgs >= 3) ? args.at(exec, 2).toInt32(exec) : 1;
          t.hour = args.at(exec, 3).toInt32(exec);
          t.minute = args.at(exec, 4).toInt32(exec);
          t.second = args.at(exec, 5).toInt32(exec);
          t.isDST = -1;
          double ms = (numArgs >= 7) ? args.at(exec, 6).toNumber(exec) : 0;
          value = gregorianDateTimeToMS(t, ms, false);
        }
    }

    DateInstance* result = new (exec) DateInstance(exec->lexicalGlobalObject()->dateStructure());
    result->setInternalValue(jsNumber(exec, timeClip(value)));
    return result;
}
    
static JSObject* constructWithDateConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    return constructDate(exec, args);
}

ConstructType DateConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithDateConstructor;
    return ConstructTypeHost;
}

// ECMA 15.9.2
static JSValuePtr callDate(ExecState* exec, JSObject*, JSValuePtr, const ArgList&)
{
    time_t localTime = time(0);
    tm localTM;
    getLocalTime(&localTime, &localTM);
    GregorianDateTime ts(localTM);
    return jsNontrivialString(exec, formatDate(ts) + " " + formatTime(ts, false));
}

CallType DateConstructor::getCallData(CallData& callData)
{
    callData.native.function = callDate;
    return CallTypeHost;
}

static JSValuePtr dateParse(ExecState* exec, JSObject*, JSValuePtr, const ArgList& args)
{
    return jsNumber(exec, parseDate(args.at(exec, 0).toString(exec)));
}

static JSValuePtr dateNow(ExecState* exec, JSObject*, JSValuePtr, const ArgList&)
{
    return jsNumber(exec, getCurrentUTCTime());
}

static JSValuePtr dateUTC(ExecState* exec, JSObject*, JSValuePtr, const ArgList& args)
{
    int n = args.size();
    if (isnan(args.at(exec, 0).toNumber(exec))
            || isnan(args.at(exec, 1).toNumber(exec))
            || (n >= 3 && isnan(args.at(exec, 2).toNumber(exec)))
            || (n >= 4 && isnan(args.at(exec, 3).toNumber(exec)))
            || (n >= 5 && isnan(args.at(exec, 4).toNumber(exec)))
            || (n >= 6 && isnan(args.at(exec, 5).toNumber(exec)))
            || (n >= 7 && isnan(args.at(exec, 6).toNumber(exec))))
        return jsNaN(exec);

    GregorianDateTime t;
    int year = args.at(exec, 0).toInt32(exec);
    t.year = (year >= 0 && year <= 99) ? year : year - 1900;
    t.month = args.at(exec, 1).toInt32(exec);
    t.monthDay = (n >= 3) ? args.at(exec, 2).toInt32(exec) : 1;
    t.hour = args.at(exec, 3).toInt32(exec);
    t.minute = args.at(exec, 4).toInt32(exec);
    t.second = args.at(exec, 5).toInt32(exec);
    double ms = (n >= 7) ? args.at(exec, 6).toNumber(exec) : 0;
    return jsNumber(exec, gregorianDateTimeToMS(t, ms, true));
}

} // namespace JSC
