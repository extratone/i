/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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
#include "Arguments.h"

#include "JSActivation.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"

using namespace std;

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(Arguments);

const ClassInfo Arguments::info = { "Arguments", 0, 0, 0 };

Arguments::~Arguments()
{
    if (d->extraArguments != d->extraArgumentsFixedBuffer)
        delete [] d->extraArguments;
}

void Arguments::mark()
{
    JSObject::mark();

    if (d->registerArray) {
        for (unsigned i = 0; i < d->numParameters; ++i) {
            if (!d->registerArray[i].marked())
                d->registerArray[i].mark();
        }
    }

    if (d->extraArguments) {
        unsigned numExtraArguments = d->numArguments - d->numParameters;
        for (unsigned i = 0; i < numExtraArguments; ++i) {
            if (!d->extraArguments[i].marked())
                d->extraArguments[i].mark();
        }
    }

    if (!d->callee->marked())
        d->callee->mark();

    if (d->activation && !d->activation->marked())
        d->activation->mark();
}

void Arguments::fillArgList(ExecState* exec, ArgList& args)
{
    if (LIKELY(!d->deletedArguments)) {
        if (LIKELY(!d->numParameters)) {
            args.initialize(d->extraArguments, d->numArguments);
            return;
        }

        if (d->numParameters == d->numArguments) {
            args.initialize(&d->registers[d->firstParameterIndex], d->numArguments);
            return;
        }

        unsigned parametersLength = min(d->numParameters, d->numArguments);
        unsigned i = 0;
        for (; i < parametersLength; ++i)
            args.append(d->registers[d->firstParameterIndex + i].jsValue(exec));
        for (; i < d->numArguments; ++i)
            args.append(d->extraArguments[i - d->numParameters].jsValue(exec));
        return;
    }

    unsigned parametersLength = min(d->numParameters, d->numArguments);
    unsigned i = 0;
    for (; i < parametersLength; ++i) {
        if (!d->deletedArguments[i])
            args.append(d->registers[d->firstParameterIndex + i].jsValue(exec));
        else
            args.append(get(exec, i));
    }
    for (; i < d->numArguments; ++i) {
        if (!d->deletedArguments[i])
            args.append(d->extraArguments[i - d->numParameters].jsValue(exec));
        else
            args.append(get(exec, i));
    }
}

bool Arguments::getOwnPropertySlot(ExecState* exec, unsigned i, PropertySlot& slot)
{
    if (i < d->numArguments && (!d->deletedArguments || !d->deletedArguments[i])) {
        if (i < d->numParameters) {
            slot.setRegisterSlot(&d->registers[d->firstParameterIndex + i]);
        } else
            slot.setValue(d->extraArguments[i - d->numParameters].jsValue(exec));
        return true;
    }

    return JSObject::getOwnPropertySlot(exec, Identifier(exec, UString::from(i)), slot);
}

bool Arguments::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(&isArrayIndex);
    if (isArrayIndex && i < d->numArguments && (!d->deletedArguments || !d->deletedArguments[i])) {
        if (i < d->numParameters) {
            slot.setRegisterSlot(&d->registers[d->firstParameterIndex + i]);
        } else
            slot.setValue(d->extraArguments[i - d->numParameters].jsValue(exec));
        return true;
    }

    if (propertyName == exec->propertyNames().length && LIKELY(!d->overrodeLength)) {
        slot.setValue(jsNumber(exec, d->numArguments));
        return true;
    }

    if (propertyName == exec->propertyNames().callee && LIKELY(!d->overrodeCallee)) {
        slot.setValue(d->callee);
        return true;
    }

    return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

void Arguments::put(ExecState* exec, unsigned i, JSValuePtr value, PutPropertySlot& slot)
{
    if (i < d->numArguments && (!d->deletedArguments || !d->deletedArguments[i])) {
        if (i < d->numParameters)
            d->registers[d->firstParameterIndex + i] = JSValuePtr(value);
        else
            d->extraArguments[i - d->numParameters] = JSValuePtr(value);
        return;
    }

    JSObject::put(exec, Identifier(exec, UString::from(i)), value, slot);
}

void Arguments::put(ExecState* exec, const Identifier& propertyName, JSValuePtr value, PutPropertySlot& slot)
{
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(&isArrayIndex);
    if (isArrayIndex && i < d->numArguments && (!d->deletedArguments || !d->deletedArguments[i])) {
        if (i < d->numParameters)
            d->registers[d->firstParameterIndex + i] = JSValuePtr(value);
        else
            d->extraArguments[i - d->numParameters] = JSValuePtr(value);
        return;
    }

    if (propertyName == exec->propertyNames().length && !d->overrodeLength) {
        d->overrodeLength = true;
        putDirect(propertyName, value, DontEnum);
        return;
    }

    if (propertyName == exec->propertyNames().callee && !d->overrodeCallee) {
        d->overrodeCallee = true;
        putDirect(propertyName, value, DontEnum);
        return;
    }

    JSObject::put(exec, propertyName, value, slot);
}

bool Arguments::deleteProperty(ExecState* exec, unsigned i) 
{
    if (i < d->numArguments) {
        if (!d->deletedArguments) {
            d->deletedArguments.set(new bool[d->numArguments]);
            memset(d->deletedArguments.get(), 0, sizeof(bool) * d->numArguments);
        }
        if (!d->deletedArguments[i]) {
            d->deletedArguments[i] = true;
            return true;
        }
    }

    return JSObject::deleteProperty(exec, Identifier(exec, UString::from(i)));
}

bool Arguments::deleteProperty(ExecState* exec, const Identifier& propertyName) 
{
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(&isArrayIndex);
    if (isArrayIndex && i < d->numArguments) {
        if (!d->deletedArguments) {
            d->deletedArguments.set(new bool[d->numArguments]);
            memset(d->deletedArguments.get(), 0, sizeof(bool) * d->numArguments);
        }
        if (!d->deletedArguments[i]) {
            d->deletedArguments[i] = true;
            return true;
        }
    }

    if (propertyName == exec->propertyNames().length && !d->overrodeLength) {
        d->overrodeLength = true;
        return true;
    }

    if (propertyName == exec->propertyNames().callee && !d->overrodeCallee) {
        d->overrodeCallee = true;
        return true;
    }

    return JSObject::deleteProperty(exec, propertyName);
}

} // namespace JSC
