// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
#include "operations.h"

#include "object.h"
#include <math.h>
#include <stdio.h>
#include <wtf/MathExtras.h>

#if HAVE(FUNC_ISINF) && HAVE(IEEEFP_H)
#include <ieeefp.h>
#endif

#if HAVE(FLOAT_H)
#include <float.h>
#endif

namespace KJS {
    
#if !PLATFORM(DARWIN)

// FIXME: Should probably be inlined on non-Darwin platforms too, and controlled exclusively
// by HAVE macros rather than PLATFORM.

// FIXME: Merge with isnan in MathExtras.h and remove this one entirely.
bool isNaN(double d)
{
#if HAVE(FUNC_ISNAN)
    return isnan(d);
#elif HAVE(FLOAT_H)
    return _isnan(d) != 0;
#else
    return !(d == d);
#endif
}

// FIXME: Merge with isinf in MathExtras.h and remove this one entirely.
bool isInf(double d)
{
    // FIXME: should be HAVE(_FPCLASS)
#if PLATFORM(WIN_OS)
    int fpClass = _fpclass(d);
    return _FPCLASS_PINF == fpClass || _FPCLASS_NINF == fpClass;
#elif HAVE(FUNC_ISINF)
    return isinf(d);
#elif HAVE(FUNC_FINITE)
    return finite(d) == 0 && d == d;
#elif HAVE(FUNC__FINITE)
    return _finite(d) == 0 && d == d;
#else
    return false;
#endif
}

bool isPosInf(double d)
{
    // FIXME: should be HAVE(_FPCLASS)
#if PLATFORM(WIN_OS)
    return _FPCLASS_PINF == _fpclass(d);
#elif HAVE(FUNC_ISINF)
    return (isinf(d) == 1);
#elif HAVE(FUNC_FINITE)
    return !finite(d) && d == d; // ### can we distinguish between + and - ?
#elif HAVE(FUNC__FINITE)
    return !_finite(d) && d == d; // ###
#else
    return false;
#endif
}

bool isNegInf(double d)
{
    // FIXME: should be HAVE(_FPCLASS)
#if PLATFORM(WIN_OS)
    return _FPCLASS_NINF == _fpclass(d);
#elif HAVE(FUNC_ISINF)
    return (isinf(d) == -1);
#elif HAVE(FUNC_FINITE)
    return finite(d) == 0 && d == d; // ###
#elif HAVE(FUNC__FINITE)
    return _finite(d) == 0 && d == d; // ###
#else
    return false;
#endif
}

#endif

// ECMA 11.9.3
bool equal(ExecState *exec, JSValue *v1, JSValue *v2)
{
    JSType t1 = v1->type();
    JSType t2 = v2->type();
    
    if (t1 != t2) {
        if (t1 == UndefinedType)
            t1 = NullType;
        if (t2 == UndefinedType)
            t2 = NullType;
        
        if (t1 == BooleanType)
            t1 = NumberType;
        if (t2 == BooleanType)
            t2 = NumberType;
        
        if (t1 == NumberType && t2 == StringType) {
            // use toNumber
        } else if (t1 == StringType && t2 == NumberType)
            t1 = NumberType;
            // use toNumber
        else {
            if ((t1 == StringType || t1 == NumberType) && t2 >= ObjectType)
                return equal(exec, v1, v2->toPrimitive(exec));
            if (t1 == NullType && t2 == ObjectType)
                return static_cast<JSObject *>(v2)->masqueradeAsUndefined();
            if (t1 >= ObjectType && (t2 == StringType || t2 == NumberType))
                return equal(exec, v1->toPrimitive(exec), v2);
            if (t1 == ObjectType && t2 == NullType)
                return static_cast<JSObject *>(v1)->masqueradeAsUndefined();
            if (t1 != t2)
                return false;
        }
    }
    
    if (t1 == UndefinedType || t1 == NullType)
        return true;
    
    if (t1 == NumberType) {
        double d1 = v1->toNumber(exec);
        double d2 = v2->toNumber(exec);
        return d1 == d2;
    }
    
    if (t1 == StringType)
        return v1->toString(exec) == v2->toString(exec);
    
    if (t1 == BooleanType)
        return v1->toBoolean(exec) == v2->toBoolean(exec);
    
    // types are Object
    return v1 == v2;
}

bool strictEqual(ExecState *exec, JSValue *v1, JSValue *v2)
{
    JSType t1 = v1->type();
    JSType t2 = v2->type();
    
    if (t1 != t2)
        return false;
    if (t1 == UndefinedType || t1 == NullType)
        return true;
    if (t1 == NumberType) {
        double n1 = v1->toNumber(exec);
        double n2 = v2->toNumber(exec);
        if (n1 == n2)
            return true;
        return false;
    } else if (t1 == StringType)
        return v1->toString(exec) == v2->toString(exec);
    else if (t2 == BooleanType)
        return v1->toBoolean(exec) == v2->toBoolean(exec);
    
    if (v1 == v2)
        return true;
    /* TODO: joined objects */
    
    return false;
}

int relation(ExecState *exec, JSValue *v1, JSValue *v2)
{
    JSValue *p1 = v1->toPrimitive(exec,NumberType);
    JSValue *p2 = v2->toPrimitive(exec,NumberType);
    
    if (p1->isString() && p2->isString())
        return p1->toString(exec) < p2->toString(exec) ? 1 : 0;
    
    double n1 = p1->toNumber(exec);
    double n2 = p2->toNumber(exec);
    if (n1 < n2)
        return 1;
    if (n1 >= n2)
        return 0;
    return -1; // must be NaN, so undefined
}

int maxInt(int d1, int d2)
{
    return (d1 > d2) ? d1 : d2;
}

int minInt(int d1, int d2)
{
    return (d1 < d2) ? d1 : d2;
}

// ECMA 11.6
JSValue *add(ExecState *exec, JSValue *v1, JSValue *v2, char oper)
{
    // exception for the Date exception in defaultValue()
    JSType preferred = oper == '+' ? UnspecifiedType : NumberType;
    JSValue *p1 = v1->toPrimitive(exec, preferred);
    JSValue *p2 = v2->toPrimitive(exec, preferred);
    
    if ((p1->isString() || p2->isString()) && oper == '+')
        return jsString(p1->toString(exec) + p2->toString(exec));
    
    if (oper == '+')
        return jsNumber(p1->toNumber(exec) + p2->toNumber(exec));
    else
        return jsNumber(p1->toNumber(exec) - p2->toNumber(exec));
}

// ECMA 11.5
JSValue *mult(ExecState *exec, JSValue *v1, JSValue *v2, char oper)
{
    double n1 = v1->toNumber(exec);
    double n2 = v2->toNumber(exec);
    
    double result;
    
    if (oper == '*')
        result = n1 * n2;
    else if (oper == '/')
        result = n1 / n2;
    else
        result = fmod(n1, n2);
    
    return jsNumber(result);
}

}
