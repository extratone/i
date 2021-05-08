/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2003 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "ArrayPrototype.h"

#include "CodeBlock.h"
#include "Interpreter.h"
#include "ObjectPrototype.h"
#include "Lookup.h"
#include "Operations.h"
#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(ArrayPrototype);

static JSValuePtr arrayProtoFuncToString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncToLocaleString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncConcat(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncJoin(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncPop(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncPush(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncReverse(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncShift(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncSlice(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncSort(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncSplice(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncUnShift(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncEvery(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncForEach(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncSome(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncIndexOf(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncFilter(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncMap(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr arrayProtoFuncLastIndexOf(ExecState*, JSObject*, JSValuePtr, const ArgList&);

}

#include "ArrayPrototype.lut.h"

namespace JSC {

static inline bool isNumericCompareFunction(CallType callType, const CallData& callData)
{
    if (callType != CallTypeJS)
        return false;
    
    return callData.js.functionBody->bytecode(callData.js.scopeChain).isNumericCompareFunction();
}

// ------------------------------ ArrayPrototype ----------------------------

const ClassInfo ArrayPrototype::info = {"Array", &JSArray::info, 0, ExecState::arrayTable};

/* Source for ArrayPrototype.lut.h
@begin arrayTable 16
  toString       arrayProtoFuncToString       DontEnum|Function 0
  toLocaleString arrayProtoFuncToLocaleString DontEnum|Function 0
  concat         arrayProtoFuncConcat         DontEnum|Function 1
  join           arrayProtoFuncJoin           DontEnum|Function 1
  pop            arrayProtoFuncPop            DontEnum|Function 0
  push           arrayProtoFuncPush           DontEnum|Function 1
  reverse        arrayProtoFuncReverse        DontEnum|Function 0
  shift          arrayProtoFuncShift          DontEnum|Function 0
  slice          arrayProtoFuncSlice          DontEnum|Function 2
  sort           arrayProtoFuncSort           DontEnum|Function 1
  splice         arrayProtoFuncSplice         DontEnum|Function 2
  unshift        arrayProtoFuncUnShift        DontEnum|Function 1
  every          arrayProtoFuncEvery          DontEnum|Function 1
  forEach        arrayProtoFuncForEach        DontEnum|Function 1
  some           arrayProtoFuncSome           DontEnum|Function 1
  indexOf        arrayProtoFuncIndexOf        DontEnum|Function 1
  lastIndexOf    arrayProtoFuncLastIndexOf    DontEnum|Function 1
  filter         arrayProtoFuncFilter         DontEnum|Function 1
  map            arrayProtoFuncMap            DontEnum|Function 1
@end
*/

// ECMA 15.4.4
ArrayPrototype::ArrayPrototype(PassRefPtr<Structure> structure)
    : JSArray(structure)
{
}

bool ArrayPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSArray>(exec, ExecState::arrayTable(exec), this, propertyName, slot);
}

// ------------------------------ Array Functions ----------------------------

// Helper function
static JSValuePtr getProperty(ExecState* exec, JSObject* obj, unsigned index)
{
    PropertySlot slot(obj);
    if (!obj->getPropertySlot(exec, index, slot))
        return noValue();
    return slot.getValue(exec, index);
}

static void putProperty(ExecState* exec, JSObject* obj, const Identifier& propertyName, JSValuePtr value)
{
    PutPropertySlot slot;
    obj->put(exec, propertyName, value, slot);
}

JSValuePtr arrayProtoFuncToString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&JSArray::info))
        return throwError(exec, TypeError);
    JSObject* thisObj = asArray(thisValue);

    HashSet<JSObject*>& arrayVisitedElements = exec->globalData().arrayVisitedElements;
    if (arrayVisitedElements.size() > MaxReentryDepth)
        return throwError(exec, RangeError, "Maximum call stack size exceeded.");

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    if (alreadyVisited)
        return jsEmptyString(exec); // return an empty string, avoiding infinite recursion.

    Vector<UChar, 256> strBuffer;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(',');
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValuePtr element = thisObj->get(exec, k);
        if (element.isUndefinedOrNull())
            continue;

        UString str = element.toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    arrayVisitedElements.remove(thisObj);
    return jsString(exec, UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValuePtr arrayProtoFuncToLocaleString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (!thisValue.isObject(&JSArray::info))
        return throwError(exec, TypeError);
    JSObject* thisObj = asArray(thisValue);

    HashSet<JSObject*>& arrayVisitedElements = exec->globalData().arrayVisitedElements;
    if (arrayVisitedElements.size() > MaxReentryDepth)
        return throwError(exec, RangeError, "Maximum call stack size exceeded.");

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    if (alreadyVisited)
        return jsEmptyString(exec); // return an empty string, avoding infinite recursion.

    Vector<UChar, 256> strBuffer;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(',');
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValuePtr element = thisObj->get(exec, k);
        if (element.isUndefinedOrNull())
            continue;

        JSObject* o = element.toObject(exec);
        JSValuePtr conversionFunction = o->get(exec, exec->propertyNames().toLocaleString);
        UString str;
        CallData callData;
        CallType callType = conversionFunction.getCallData(callData);
        if (callType != CallTypeNone)
            str = call(exec, conversionFunction, callType, callData, element, exec->emptyList()).toString(exec);
        else
            str = element.toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    arrayVisitedElements.remove(thisObj);
    return jsString(exec, UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValuePtr arrayProtoFuncJoin(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    HashSet<JSObject*>& arrayVisitedElements = exec->globalData().arrayVisitedElements;
    if (arrayVisitedElements.size() > MaxReentryDepth)
        return throwError(exec, RangeError, "Maximum call stack size exceeded.");

    bool alreadyVisited = !arrayVisitedElements.add(thisObj).second;
    if (alreadyVisited)
        return jsEmptyString(exec); // return an empty string, avoding infinite recursion.

    Vector<UChar, 256> strBuffer;

    UChar comma = ',';
    UString separator = args.at(exec, 0).isUndefined() ? UString(&comma, 1) : args.at(exec, 0).toString(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned k = 0; k < length; k++) {
        if (k >= 1)
            strBuffer.append(separator.data(), separator.size());
        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
            break;
        }

        JSValuePtr element = thisObj->get(exec, k);
        if (element.isUndefinedOrNull())
            continue;

        UString str = element.toString(exec);
        strBuffer.append(str.data(), str.size());

        if (!strBuffer.data()) {
            JSObject* error = Error::create(exec, GeneralError, "Out of memory");
            exec->setException(error);
        }

        if (exec->hadException())
            break;
    }
    arrayVisitedElements.remove(thisObj);
    return jsString(exec, UString(strBuffer.data(), strBuffer.data() ? strBuffer.size() : 0));
}

JSValuePtr arrayProtoFuncConcat(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSArray* arr = constructEmptyArray(exec);
    int n = 0;
    JSValuePtr curArg = thisValue.toThisObject(exec);
    ArgList::const_iterator it = args.begin();
    ArgList::const_iterator end = args.end();
    while (1) {
        if (curArg.isObject(&JSArray::info)) {
            JSArray* curArray = asArray(curArg);
            unsigned length = curArray->length();
            for (unsigned k = 0; k < length; ++k) {
                if (JSValuePtr v = getProperty(exec, curArray, k))
                    arr->put(exec, n, v);
                n++;
            }
        } else {
            arr->put(exec, n, curArg);
            n++;
        }
        if (it == end)
            break;
        curArg = (*it).jsValue(exec);
        ++it;
    }
    arr->setLength(n);
    return arr;
}

JSValuePtr arrayProtoFuncPop(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (exec->interpreter()->isJSArray(thisValue))
        return asArray(thisValue)->pop();

    JSObject* thisObj = thisValue.toThisObject(exec);
    JSValuePtr result;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (length == 0) {
        putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length));
        result = jsUndefined();
    } else {
        result = thisObj->get(exec, length - 1);
        thisObj->deleteProperty(exec, length - 1);
        putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length - 1));
    }
    return result;
}

JSValuePtr arrayProtoFuncPush(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    if (exec->interpreter()->isJSArray(thisValue) && args.size() == 1) {
        JSArray* array = asArray(thisValue);
        array->push(exec, args.begin()->jsValue(exec));
        return jsNumber(exec, array->length());
    }

    JSObject* thisObj = thisValue.toThisObject(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned n = 0; n < args.size(); n++)
        thisObj->put(exec, length + n, args.at(exec, n));
    length += args.size();
    putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length));
    return jsNumber(exec, length);
}

JSValuePtr arrayProtoFuncReverse(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    JSObject* thisObj = thisValue.toThisObject(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    unsigned middle = length / 2;

    for (unsigned k = 0; k < middle; k++) {
        unsigned lk1 = length - k - 1;
        JSValuePtr obj2 = getProperty(exec, thisObj, lk1);
        JSValuePtr obj = getProperty(exec, thisObj, k);

        if (obj2)
            thisObj->put(exec, k, obj2);
        else
            thisObj->deleteProperty(exec, k);

        if (obj)
            thisObj->put(exec, lk1, obj);
        else
            thisObj->deleteProperty(exec, lk1);
    }
    return thisObj;
}

JSValuePtr arrayProtoFuncShift(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    JSObject* thisObj = thisValue.toThisObject(exec);
    JSValuePtr result;

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (length == 0) {
        putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length));
        result = jsUndefined();
    } else {
        result = thisObj->get(exec, 0);
        for (unsigned k = 1; k < length; k++) {
            if (JSValuePtr obj = getProperty(exec, thisObj, k))
                thisObj->put(exec, k - 1, obj);
            else
                thisObj->deleteProperty(exec, k - 1);
        }
        thisObj->deleteProperty(exec, length - 1);
        putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length - 1));
    }
    return result;
}

JSValuePtr arrayProtoFuncSlice(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    // http://developer.netscape.com/docs/manuals/js/client/jsref/array.htm#1193713 or 15.4.4.10

    JSObject* thisObj = thisValue.toThisObject(exec);

    // We return a new array
    JSArray* resObj = constructEmptyArray(exec);
    JSValuePtr result = resObj;
    double begin = args.at(exec, 0).toInteger(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (begin >= 0) {
        if (begin > length)
            begin = length;
    } else {
        begin += length;
        if (begin < 0)
            begin = 0;
    }
    double end;
    if (args.at(exec, 1).isUndefined())
        end = length;
    else {
        end = args.at(exec, 1).toInteger(exec);
        if (end < 0) {
            end += length;
            if (end < 0)
                end = 0;
        } else {
            if (end > length)
                end = length;
        }
    }

    int n = 0;
    int b = static_cast<int>(begin);
    int e = static_cast<int>(end);
    for (int k = b; k < e; k++, n++) {
        if (JSValuePtr v = getProperty(exec, thisObj, k))
            resObj->put(exec, n, v);
    }
    resObj->setLength(n);
    return result;
}

JSValuePtr arrayProtoFuncSort(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValuePtr function = args.at(exec, 0);
    CallData callData;
    CallType callType = function.getCallData(callData);

    if (thisObj->classInfo() == &JSArray::info) {
        if (isNumericCompareFunction(callType, callData))
            asArray(thisObj)->sortNumeric(exec, function, callType, callData);
        else if (callType != CallTypeNone)
            asArray(thisObj)->sort(exec, function, callType, callData);
        else
            asArray(thisObj)->sort(exec);
        return thisObj;
    }

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);

    if (!length)
        return thisObj;

    // "Min" sort. Not the fastest, but definitely less code than heapsort
    // or quicksort, and much less swapping than bubblesort/insertionsort.
    for (unsigned i = 0; i < length - 1; ++i) {
        JSValuePtr iObj = thisObj->get(exec, i);
        unsigned themin = i;
        JSValuePtr minObj = iObj;
        for (unsigned j = i + 1; j < length; ++j) {
            JSValuePtr jObj = thisObj->get(exec, j);
            double compareResult;
            if (jObj.isUndefined())
                compareResult = 1; // don't check minObj because there's no need to differentiate == (0) from > (1)
            else if (minObj.isUndefined())
                compareResult = -1;
            else if (callType != CallTypeNone) {
                ArgList l;
                l.append(jObj);
                l.append(minObj);
                compareResult = call(exec, function, callType, callData, exec->globalThisValue(), l).toNumber(exec);
            } else
                compareResult = (jObj.toString(exec) < minObj.toString(exec)) ? -1 : 1;

            if (compareResult < 0) {
                themin = j;
                minObj = jObj;
            }
        }
        // Swap themin and i
        if (themin > i) {
            thisObj->put(exec, i, minObj);
            thisObj->put(exec, themin, iObj);
        }
    }
    return thisObj;
}

JSValuePtr arrayProtoFuncSplice(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    // 15.4.4.12
    JSArray* resObj = constructEmptyArray(exec);
    JSValuePtr result = resObj;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (!args.size())
        return jsUndefined();
    int begin = args.at(exec, 0).toUInt32(exec);
    if (begin < 0)
        begin = std::max<int>(begin + length, 0);
    else
        begin = std::min<int>(begin, length);

    unsigned deleteCount;
    if (args.size() > 1)
        deleteCount = std::min<int>(std::max<int>(args.at(exec, 1).toUInt32(exec), 0), length - begin);
    else
        deleteCount = length - begin;

    for (unsigned k = 0; k < deleteCount; k++) {
        if (JSValuePtr v = getProperty(exec, thisObj, k + begin))
            resObj->put(exec, k, v);
    }
    resObj->setLength(deleteCount);

    unsigned additionalArgs = std::max<int>(args.size() - 2, 0);
    if (additionalArgs != deleteCount) {
        if (additionalArgs < deleteCount) {
            for (unsigned k = begin; k < length - deleteCount; ++k) {
                if (JSValuePtr v = getProperty(exec, thisObj, k + deleteCount))
                    thisObj->put(exec, k + additionalArgs, v);
                else
                    thisObj->deleteProperty(exec, k + additionalArgs);
            }
            for (unsigned k = length; k > length - deleteCount + additionalArgs; --k)
                thisObj->deleteProperty(exec, k - 1);
        } else {
            for (unsigned k = length - deleteCount; (int)k > begin; --k) {
                if (JSValuePtr obj = getProperty(exec, thisObj, k + deleteCount - 1))
                    thisObj->put(exec, k + additionalArgs - 1, obj);
                else
                    thisObj->deleteProperty(exec, k + additionalArgs - 1);
            }
        }
    }
    for (unsigned k = 0; k < additionalArgs; ++k)
        thisObj->put(exec, k + begin, args.at(exec, k + 2));

    putProperty(exec, thisObj, exec->propertyNames().length, jsNumber(exec, length - deleteCount + additionalArgs));
    return result;
}

JSValuePtr arrayProtoFuncUnShift(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    // 15.4.4.13
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    unsigned nrArgs = args.size();
    if (nrArgs) {
        for (unsigned k = length; k > 0; --k) {
            if (JSValuePtr v = getProperty(exec, thisObj, k - 1))
                thisObj->put(exec, k + nrArgs - 1, v);
            else
                thisObj->deleteProperty(exec, k + nrArgs - 1);
        }
    }
    for (unsigned k = 0; k < nrArgs; ++k)
        thisObj->put(exec, k, args.at(exec, k));
    JSValuePtr result = jsNumber(exec, length + nrArgs);
    putProperty(exec, thisObj, exec->propertyNames().length, result);
    return result;
}

JSValuePtr arrayProtoFuncFilter(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValuePtr function = args.at(exec, 0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args.at(exec, 1).isUndefinedOrNull() ? exec->globalThisValue() : args.at(exec, 1).toObject(exec);
    JSArray* resultArray = constructEmptyArray(exec);

    unsigned filterIndex = 0;
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);

        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        JSValuePtr v = slot.getValue(exec, k);

        ArgList eachArguments;

        eachArguments.append(v);
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        JSValuePtr result = call(exec, function, callType, callData, applyThis, eachArguments);

        if (result.toBoolean(exec))
            resultArray->put(exec, filterIndex++, v);
    }
    return resultArray;
}

JSValuePtr arrayProtoFuncMap(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValuePtr function = args.at(exec, 0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args.at(exec, 1).isUndefinedOrNull() ? exec->globalThisValue() : args.at(exec, 1).toObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);

    JSArray* resultArray = constructEmptyArray(exec, length);

    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        JSValuePtr v = slot.getValue(exec, k);

        ArgList eachArguments;

        eachArguments.append(v);
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        JSValuePtr result = call(exec, function, callType, callData, applyThis, eachArguments);
        resultArray->put(exec, k, result);
    }

    return resultArray;
}

// Documentation for these three is available at:
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:every
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:forEach
// http://developer-test.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Array:some

JSValuePtr arrayProtoFuncEvery(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValuePtr function = args.at(exec, 0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args.at(exec, 1).isUndefinedOrNull() ? exec->globalThisValue() : args.at(exec, 1).toObject(exec);

    JSValuePtr result = jsBoolean(true);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);

        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        ArgList eachArguments;

        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        bool predicateResult = call(exec, function, callType, callData, applyThis, eachArguments).toBoolean(exec);

        if (!predicateResult) {
            result = jsBoolean(false);
            break;
        }
    }

    return result;
}

JSValuePtr arrayProtoFuncForEach(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValuePtr function = args.at(exec, 0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args.at(exec, 1).isUndefinedOrNull() ? exec->globalThisValue() : args.at(exec, 1).toObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        ArgList eachArguments;
        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        call(exec, function, callType, callData, applyThis, eachArguments);
    }
    return jsUndefined();
}

JSValuePtr arrayProtoFuncSome(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSObject* thisObj = thisValue.toThisObject(exec);

    JSValuePtr function = args.at(exec, 0);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSObject* applyThis = args.at(exec, 1).isUndefinedOrNull() ? exec->globalThisValue() : args.at(exec, 1).toObject(exec);

    JSValuePtr result = jsBoolean(false);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    for (unsigned k = 0; k < length && !exec->hadException(); ++k) {
        PropertySlot slot(thisObj);
        if (!thisObj->getPropertySlot(exec, k, slot))
            continue;

        ArgList eachArguments;
        eachArguments.append(slot.getValue(exec, k));
        eachArguments.append(jsNumber(exec, k));
        eachArguments.append(thisObj);

        bool predicateResult = call(exec, function, callType, callData, applyThis, eachArguments).toBoolean(exec);

        if (predicateResult) {
            result = jsBoolean(true);
            break;
        }
    }
    return result;
}

JSValuePtr arrayProtoFuncIndexOf(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    // JavaScript 1.5 Extension by Mozilla
    // Documentation: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:indexOf

    JSObject* thisObj = thisValue.toThisObject(exec);

    unsigned index = 0;
    double d = args.at(exec, 1).toInteger(exec);
    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    if (d < 0)
        d += length;
    if (d > 0) {
        if (d > length)
            index = length;
        else
            index = static_cast<unsigned>(d);
    }

    JSValuePtr searchElement = args.at(exec, 0);
    for (; index < length; ++index) {
        JSValuePtr e = getProperty(exec, thisObj, index);
        if (!e)
            continue;
        if (JSValuePtr::strictEqual(searchElement, e))
            return jsNumber(exec, index);
    }

    return jsNumber(exec, -1);
}

JSValuePtr arrayProtoFuncLastIndexOf(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    // JavaScript 1.6 Extension by Mozilla
    // Documentation: http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:lastIndexOf

    JSObject* thisObj = thisValue.toThisObject(exec);

    unsigned length = thisObj->get(exec, exec->propertyNames().length).toUInt32(exec);
    int index = length - 1;
    double d = args.at(exec, 1).toIntegerPreserveNaN(exec);

    if (d < 0) {
        d += length;
        if (d < 0)
            return jsNumber(exec, -1);
    }
    if (d < length)
        index = static_cast<int>(d);

    JSValuePtr searchElement = args.at(exec, 0);
    for (; index >= 0; --index) {
        JSValuePtr e = getProperty(exec, thisObj, index);
        if (!e)
            continue;
        if (JSValuePtr::strictEqual(searchElement, e))
            return jsNumber(exec, index);
    }

    return jsNumber(exec, -1);
}

} // namespace JSC
