/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "StringPrototype.h"

#include "JSArray.h"
#include "JSFunction.h"
#include "ObjectPrototype.h"
#include "PropertyNameArray.h"
#include "RegExpConstructor.h"
#include "RegExpObject.h"
#include <wtf/ASCIICType.h>
#include <wtf/MathExtras.h>
#include <wtf/unicode/Collator.h>

using namespace WTF;

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(StringPrototype);

static JSValuePtr stringProtoFuncToString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncCharAt(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncCharCodeAt(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncConcat(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncIndexOf(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncLastIndexOf(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncMatch(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncReplace(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncSearch(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncSlice(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncSplit(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncSubstr(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncSubstring(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncToLowerCase(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncToUpperCase(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncLocaleCompare(ExecState*, JSObject*, JSValuePtr, const ArgList&);

static JSValuePtr stringProtoFuncBig(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncSmall(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncBlink(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncBold(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncFixed(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncItalics(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncStrike(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncSub(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncSup(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncFontcolor(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncFontsize(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncAnchor(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr stringProtoFuncLink(ExecState*, JSObject*, JSValuePtr, const ArgList&);

}

#include "StringPrototype.lut.h"

namespace JSC {

const ClassInfo StringPrototype::info = { "String", &StringObject::info, 0, ExecState::stringTable };

/* Source for StringPrototype.lut.h
@begin stringTable 26
    toString              stringProtoFuncToString          DontEnum|Function       0
    valueOf               stringProtoFuncToString          DontEnum|Function       0
    charAt                stringProtoFuncCharAt            DontEnum|Function       1
    charCodeAt            stringProtoFuncCharCodeAt        DontEnum|Function       1
    concat                stringProtoFuncConcat            DontEnum|Function       1
    indexOf               stringProtoFuncIndexOf           DontEnum|Function       1
    lastIndexOf           stringProtoFuncLastIndexOf       DontEnum|Function       1
    match                 stringProtoFuncMatch             DontEnum|Function       1
    replace               stringProtoFuncReplace           DontEnum|Function       2
    search                stringProtoFuncSearch            DontEnum|Function       1
    slice                 stringProtoFuncSlice             DontEnum|Function       2
    split                 stringProtoFuncSplit             DontEnum|Function       2
    substr                stringProtoFuncSubstr            DontEnum|Function       2
    substring             stringProtoFuncSubstring         DontEnum|Function       2
    toLowerCase           stringProtoFuncToLowerCase       DontEnum|Function       0
    toUpperCase           stringProtoFuncToUpperCase       DontEnum|Function       0
    localeCompare         stringProtoFuncLocaleCompare     DontEnum|Function       1

    # toLocaleLowerCase and toLocaleUpperCase are currently identical to toLowerCase and toUpperCase
    toLocaleLowerCase     stringProtoFuncToLowerCase       DontEnum|Function       0
    toLocaleUpperCase     stringProtoFuncToUpperCase       DontEnum|Function       0

    big                   stringProtoFuncBig               DontEnum|Function       0
    small                 stringProtoFuncSmall             DontEnum|Function       0
    blink                 stringProtoFuncBlink             DontEnum|Function       0
    bold                  stringProtoFuncBold              DontEnum|Function       0
    fixed                 stringProtoFuncFixed             DontEnum|Function       0
    italics               stringProtoFuncItalics           DontEnum|Function       0
    strike                stringProtoFuncStrike            DontEnum|Function       0
    sub                   stringProtoFuncSub               DontEnum|Function       0
    sup                   stringProtoFuncSup               DontEnum|Function       0
    fontcolor             stringProtoFuncFontcolor         DontEnum|Function       1
    fontsize              stringProtoFuncFontsize          DontEnum|Function       1
    anchor                stringProtoFuncAnchor            DontEnum|Function       1
    link                  stringProtoFuncLink              DontEnum|Function       1
@end
*/

// ECMA 15.5.4
StringPrototype::StringPrototype(ExecState* exec, PassRefPtr<Structure> structure)
    : StringObject(exec, structure)
{
    // The constructor will be added later, after StringConstructor has been built
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 0), DontDelete | ReadOnly | DontEnum);
}

bool StringPrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<StringObject>(exec, ExecState::stringTable(exec), this, propertyName, slot);
}

// ------------------------------ Functions --------------------------

static inline UString substituteBackreferences(const UString& replacement, const UString& source, const int* ovector, RegExp* reg)
{
    UString substitutedReplacement;
    int offset = 0;
    int i = -1;
    while ((i = replacement.find('$', i + 1)) != -1) {
        if (i + 1 == replacement.size())
            break;

        UChar ref = replacement[i + 1];
        if (ref == '$') {
            // "$$" -> "$"
            ++i;
            substitutedReplacement.append(replacement.data() + offset, i - offset);
            offset = i + 1;
            continue;
        }

        int backrefStart;
        int backrefLength;
        int advance = 0;
        if (ref == '&') {
            backrefStart = ovector[0];
            backrefLength = ovector[1] - backrefStart;
        } else if (ref == '`') {
            backrefStart = 0;
            backrefLength = ovector[0];
        } else if (ref == '\'') {
            backrefStart = ovector[1];
            backrefLength = source.size() - backrefStart;
        } else if (reg && ref >= '0' && ref <= '9') {
            // 1- and 2-digit back references are allowed
            unsigned backrefIndex = ref - '0';
            if (backrefIndex > reg->numSubpatterns())
                continue;
            if (replacement.size() > i + 2) {
                ref = replacement[i + 2];
                if (ref >= '0' && ref <= '9') {
                    backrefIndex = 10 * backrefIndex + ref - '0';
                    if (backrefIndex > reg->numSubpatterns())
                        backrefIndex = backrefIndex / 10;   // Fall back to the 1-digit reference
                    else
                        advance = 1;
                }
            }
            if (!backrefIndex)
                continue;
            backrefStart = ovector[2 * backrefIndex];
            backrefLength = ovector[2 * backrefIndex + 1] - backrefStart;
        } else
            continue;

        if (i - offset)
            substitutedReplacement.append(replacement.data() + offset, i - offset);
        i += 1 + advance;
        offset = i + 1;
        substitutedReplacement.append(source.data() + backrefStart, backrefLength);
    }

    if (!offset)
        return replacement;

    if (replacement.size() - offset)
        substitutedReplacement.append(replacement.data() + offset, replacement.size() - offset);

    return substitutedReplacement;
}

static inline int localeCompare(const UString& a, const UString& b)
{
    return Collator::userDefault()->collate(reinterpret_cast<const ::UChar*>(a.data()), a.size(), reinterpret_cast<const ::UChar*>(b.data()), b.size());
}

JSValuePtr stringProtoFuncReplace(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    JSString* sourceVal = thisValue.toThisJSString(exec);
    const UString& source = sourceVal->value();

    JSValuePtr pattern = args.at(exec, 0);

    JSValuePtr replacement = args.at(exec, 1);
    UString replacementString;
    CallData callData;
    CallType callType = replacement.getCallData(callData);
    if (callType == CallTypeNone)
        replacementString = replacement.toString(exec);

    if (pattern.isObject(&RegExpObject::info)) {
        RegExp* reg = asRegExpObject(pattern)->regExp();
        bool global = reg->global();

        RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();

        int lastIndex = 0;
        int startPosition = 0;

        Vector<UString::Range, 16> sourceRanges;
        Vector<UString, 16> replacements;

        // This is either a loop (if global is set) or a one-way (if not).
        do {
            int matchIndex;
            int matchLen;
            int* ovector;
            regExpConstructor->performMatch(reg, source, startPosition, matchIndex, matchLen, &ovector);
            if (matchIndex < 0)
                break;

            sourceRanges.append(UString::Range(lastIndex, matchIndex - lastIndex));

            if (callType != CallTypeNone) {
                int completeMatchStart = ovector[0];
                ArgList args;

                for (unsigned i = 0; i < reg->numSubpatterns() + 1; ++i) {
                    int matchStart = ovector[i * 2];
                    int matchLen = ovector[i * 2 + 1] - matchStart;

                    if (matchStart < 0)
                        args.append(jsUndefined());
                    else
                        args.append(jsSubstring(exec, source, matchStart, matchLen));
                }

                args.append(jsNumber(exec, completeMatchStart));
                args.append(sourceVal);

                replacements.append(call(exec, replacement, callType, callData, exec->globalThisValue(), args).toString(exec));
                if (exec->hadException())
                    break;
            } else
                replacements.append(substituteBackreferences(replacementString, source, ovector, reg));

            lastIndex = matchIndex + matchLen;
            startPosition = lastIndex;

            // special case of empty match
            if (matchLen == 0) {
                startPosition++;
                if (startPosition > source.size())
                    break;
            }
        } while (global);

        if (lastIndex < source.size())
            sourceRanges.append(UString::Range(lastIndex, source.size() - lastIndex));

        UString result = source.spliceSubstringsWithSeparators(sourceRanges.data(), sourceRanges.size(), replacements.data(), replacements.size());

        if (result == source)
            return sourceVal;

        return jsString(exec, result);
    }

    // First arg is a string
    UString patternString = pattern.toString(exec);
    int matchPos = source.find(patternString);
    int matchLen = patternString.size();
    // Do the replacement
    if (matchPos == -1)
        return sourceVal;

    if (callType != CallTypeNone) {
        ArgList args;
        args.append(jsSubstring(exec, source, matchPos, matchLen));
        args.append(jsNumber(exec, matchPos));
        args.append(sourceVal);

        replacementString = call(exec, replacement, callType, callData, exec->globalThisValue(), args).toString(exec);
    }

    int ovector[2] = { matchPos, matchPos + matchLen };
    return jsString(exec, source.substr(0, matchPos)
        + substituteBackreferences(replacementString, source, ovector, 0)
        + source.substr(matchPos + matchLen));
}

JSValuePtr stringProtoFuncToString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    // Also used for valueOf.

    if (thisValue.isString())
        return thisValue;

    if (thisValue.isObject(&StringObject::info))
        return asStringObject(thisValue)->internalValue();

    return throwError(exec, TypeError);
}

JSValuePtr stringProtoFuncCharAt(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    unsigned len = s.size();
    JSValuePtr a0 = args.at(exec, 0);
    if (a0.isUInt32Fast()) {
        uint32_t i = a0.getUInt32Fast();
        if (i < len)
            return jsSingleCharacterSubstring(exec, s, i);
        return jsEmptyString(exec);
    }
    double dpos = a0.toInteger(exec);
    if (dpos >= 0 && dpos < len)
        return jsSingleCharacterSubstring(exec, s, static_cast<unsigned>(dpos));
    return jsEmptyString(exec);
}

JSValuePtr stringProtoFuncCharCodeAt(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    unsigned len = s.size();
    JSValuePtr a0 = args.at(exec, 0);
    if (a0.isUInt32Fast()) {
        uint32_t i = a0.getUInt32Fast();
        if (i < len)
            return jsNumber(exec, s.data()[i]);
        return jsNaN(exec);
    }
    double dpos = a0.toInteger(exec);
    if (dpos >= 0 && dpos < len)
        return jsNumber(exec, s[static_cast<int>(dpos)]);
    return jsNaN(exec);
}

JSValuePtr stringProtoFuncConcat(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);

    ArgList::const_iterator end = args.end();
    for (ArgList::const_iterator it = args.begin(); it != end; ++it)
        s += (*it).jsValue(exec).toString(exec);
    return jsString(exec, s);
}

JSValuePtr stringProtoFuncIndexOf(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    int len = s.size();

    JSValuePtr a0 = args.at(exec, 0);
    JSValuePtr a1 = args.at(exec, 1);
    UString u2 = a0.toString(exec);
    int pos;
    if (a1.isUndefined())
        pos = 0;
    else if (a1.isUInt32Fast())
        pos = min<uint32_t>(a1.getUInt32Fast(), len);
    else {
        double dpos = a1.toInteger(exec);
        if (dpos < 0)
            dpos = 0;
        else if (dpos > len)
            dpos = len;
        pos = static_cast<int>(dpos);
    }

    return jsNumber(exec, s.find(u2, pos));
}

JSValuePtr stringProtoFuncLastIndexOf(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    int len = s.size();

    JSValuePtr a0 = args.at(exec, 0);
    JSValuePtr a1 = args.at(exec, 1);

    UString u2 = a0.toString(exec);
    double dpos = a1.toIntegerPreserveNaN(exec);
    if (dpos < 0)
        dpos = 0;
    else if (!(dpos <= len)) // true for NaN
        dpos = len;
    return jsNumber(exec, s.rfind(u2, static_cast<int>(dpos)));
}

JSValuePtr stringProtoFuncMatch(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);

    JSValuePtr a0 = args.at(exec, 0);

    UString u = s;
    RefPtr<RegExp> reg;
    RegExpObject* imp = 0;
    if (a0.isObject(&RegExpObject::info))
        reg = asRegExpObject(a0)->regExp();
    else {
        /*
         *  ECMA 15.5.4.12 String.prototype.search (regexp)
         *  If regexp is not an object whose [[Class]] property is "RegExp", it is
         *  replaced with the result of the expression new RegExp(regexp).
         */
        reg = RegExp::create(&exec->globalData(), a0.toString(exec));
    }
    RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();
    int pos;
    int matchLength;
    regExpConstructor->performMatch(reg.get(), u, 0, pos, matchLength);
    if (!(reg->global())) {
        // case without 'g' flag is handled like RegExp.prototype.exec
        if (pos < 0)
            return jsNull();
        return regExpConstructor->arrayOfMatches(exec);
    }

    // return array of matches
    ArgList list;
    int lastIndex = 0;
    while (pos >= 0) {
        list.append(jsSubstring(exec, u, pos, matchLength));
        lastIndex = pos;
        pos += matchLength == 0 ? 1 : matchLength;
        regExpConstructor->performMatch(reg.get(), u, pos, pos, matchLength);
    }
    if (imp)
        imp->setLastIndex(lastIndex);
    if (list.isEmpty()) {
        // if there are no matches at all, it's important to return
        // Null instead of an empty array, because this matches
        // other browsers and because Null is a false value.
        return jsNull();
    }

    return constructArray(exec, list);
}

JSValuePtr stringProtoFuncSearch(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);

    JSValuePtr a0 = args.at(exec, 0);

    UString u = s;
    RefPtr<RegExp> reg;
    if (a0.isObject(&RegExpObject::info))
        reg = asRegExpObject(a0)->regExp();
    else { 
        /*
         *  ECMA 15.5.4.12 String.prototype.search (regexp)
         *  If regexp is not an object whose [[Class]] property is "RegExp", it is
         *  replaced with the result of the expression new RegExp(regexp).
         */
        reg = RegExp::create(&exec->globalData(), a0.toString(exec));
    }
    RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();
    int pos;
    int matchLength;
    regExpConstructor->performMatch(reg.get(), u, 0, pos, matchLength);
    return jsNumber(exec, pos);
}

JSValuePtr stringProtoFuncSlice(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    int len = s.size();

    JSValuePtr a0 = args.at(exec, 0);
    JSValuePtr a1 = args.at(exec, 1);

    // The arg processing is very much like ArrayProtoFunc::Slice
    double start = a0.toInteger(exec);
    double end = a1.isUndefined() ? len : a1.toInteger(exec);
    double from = start < 0 ? len + start : start;
    double to = end < 0 ? len + end : end;
    if (to > from && to > 0 && from < len) {
        if (from < 0)
            from = 0;
        if (to > len)
            to = len;
        return jsSubstring(exec, s, static_cast<unsigned>(from), static_cast<unsigned>(to) - static_cast<unsigned>(from));
    }

    return jsEmptyString(exec);
}

JSValuePtr stringProtoFuncSplit(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);

    JSValuePtr a0 = args.at(exec, 0);
    JSValuePtr a1 = args.at(exec, 1);

    JSArray* result = constructEmptyArray(exec);
    unsigned i = 0;
    int p0 = 0;
    unsigned limit = a1.isUndefined() ? 0xFFFFFFFFU : a1.toUInt32(exec);
    if (a0.isObject(&RegExpObject::info)) {
        RegExp* reg = asRegExpObject(a0)->regExp();
        if (s.isEmpty() && reg->match(s, 0) >= 0) {
            // empty string matched by regexp -> empty array
            return result;
        }
        int pos = 0;
        while (i != limit && pos < s.size()) {
            OwnArrayPtr<int> ovector;
            int mpos = reg->match(s, pos, &ovector);
            if (mpos < 0)
                break;
            int mlen = ovector[1] - ovector[0];
            pos = mpos + (mlen == 0 ? 1 : mlen);
            if (mpos != p0 || mlen) {
                result->put(exec, i++, jsSubstring(exec, s, p0, mpos - p0));
                p0 = mpos + mlen;
            }
            for (unsigned si = 1; si <= reg->numSubpatterns(); ++si) {
                int spos = ovector[si * 2];
                if (spos < 0)
                    result->put(exec, i++, jsUndefined());
                else
                    result->put(exec, i++, jsSubstring(exec, s, spos, ovector[si * 2 + 1] - spos));
            }
        }
    } else {
        UString u2 = a0.toString(exec);
        if (u2.isEmpty()) {
            if (s.isEmpty()) {
                // empty separator matches empty string -> empty array
                return result;
            }
            while (i != limit && p0 < s.size() - 1)
                result->put(exec, i++, jsSingleCharacterSubstring(exec, s, p0++));
        } else {
            int pos;
            while (i != limit && (pos = s.find(u2, p0)) >= 0) {
                result->put(exec, i++, jsSubstring(exec, s, p0, pos - p0));
                p0 = pos + u2.size();
            }
        }
    }

    // add remaining string
    if (i != limit)
        result->put(exec, i++, jsSubstring(exec, s, p0, s.size() - p0));

    return result;
}

JSValuePtr stringProtoFuncSubstr(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    int len = s.size();

    JSValuePtr a0 = args.at(exec, 0);
    JSValuePtr a1 = args.at(exec, 1);

    double start = a0.toInteger(exec);
    double length = a1.isUndefined() ? len : a1.toInteger(exec);
    if (start >= len || length <= 0)
        return jsEmptyString(exec);
    if (start < 0) {
        start += len;
        if (start < 0)
            start = 0;
    }
    if (start + length > len)
        length = len - start;
    return jsSubstring(exec, s, static_cast<unsigned>(start), static_cast<unsigned>(length));
}

JSValuePtr stringProtoFuncSubstring(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    int len = s.size();

    JSValuePtr a0 = args.at(exec, 0);
    JSValuePtr a1 = args.at(exec, 1);

    double start = a0.toNumber(exec);
    double end = a1.toNumber(exec);
    if (isnan(start))
        start = 0;
    if (isnan(end))
        end = 0;
    if (start < 0)
        start = 0;
    if (end < 0)
        end = 0;
    if (start > len)
        start = len;
    if (end > len)
        end = len;
    if (a1.isUndefined())
        end = len;
    if (start > end) {
        double temp = end;
        end = start;
        start = temp;
    }
    return jsSubstring(exec, s, static_cast<unsigned>(start), static_cast<unsigned>(end) - static_cast<unsigned>(start));
}

JSValuePtr stringProtoFuncToLowerCase(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    JSString* sVal = thisValue.toThisJSString(exec);
    const UString& s = sVal->value();

    int sSize = s.size();
    if (!sSize)
        return sVal;

    const UChar* sData = s.data();
    Vector<UChar> buffer(sSize);

    UChar ored = 0;
    for (int i = 0; i < sSize; i++) {
        UChar c = sData[i];
        ored |= c;
        buffer[i] = toASCIILower(c);
    }
    if (!(ored & ~0x7f))
        return jsString(exec, UString(buffer.releaseBuffer(), sSize, false));

    bool error;
    int length = Unicode::toLower(buffer.data(), sSize, sData, sSize, &error);
    if (error) {
        buffer.resize(length);
        length = Unicode::toLower(buffer.data(), length, sData, sSize, &error);
        if (error)
            return sVal;
    }
    if (length == sSize && memcmp(buffer.data(), sData, length * sizeof(UChar)) == 0)
        return sVal;
    return jsString(exec, UString(buffer.releaseBuffer(), length, false));
}

JSValuePtr stringProtoFuncToUpperCase(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    JSString* sVal = thisValue.toThisJSString(exec);
    const UString& s = sVal->value();

    int sSize = s.size();
    if (!sSize)
        return sVal;

    const UChar* sData = s.data();
    Vector<UChar> buffer(sSize);

    UChar ored = 0;
    for (int i = 0; i < sSize; i++) {
        UChar c = sData[i];
        ored |= c;
        buffer[i] = toASCIIUpper(c);
    }
    if (!(ored & ~0x7f))
        return jsString(exec, UString(buffer.releaseBuffer(), sSize, false));

    bool error;
    int length = Unicode::toUpper(buffer.data(), sSize, sData, sSize, &error);
    if (error) {
        buffer.resize(length);
        length = Unicode::toUpper(buffer.data(), length, sData, sSize, &error);
        if (error)
            return sVal;
    }
    if (length == sSize && memcmp(buffer.data(), sData, length * sizeof(UChar)) == 0)
        return sVal;
    return jsString(exec, UString(buffer.releaseBuffer(), length, false));
}

JSValuePtr stringProtoFuncLocaleCompare(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    if (args.size() < 1)
      return jsNumber(exec, 0);

    UString s = thisValue.toThisString(exec);
    JSValuePtr a0 = args.at(exec, 0);
    return jsNumber(exec, localeCompare(s, a0.toString(exec)));
}

JSValuePtr stringProtoFuncBig(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    UString s = thisValue.toThisString(exec);
    return jsNontrivialString(exec, "<big>" + s + "</big>");
}

JSValuePtr stringProtoFuncSmall(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    UString s = thisValue.toThisString(exec);
    return jsNontrivialString(exec, "<small>" + s + "</small>");
}

JSValuePtr stringProtoFuncBlink(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    UString s = thisValue.toThisString(exec);
    return jsNontrivialString(exec, "<blink>" + s + "</blink>");
}

JSValuePtr stringProtoFuncBold(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    UString s = thisValue.toThisString(exec);
    return jsNontrivialString(exec, "<b>" + s + "</b>");
}

JSValuePtr stringProtoFuncFixed(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    UString s = thisValue.toThisString(exec);
    return jsString(exec, "<tt>" + s + "</tt>");
}

JSValuePtr stringProtoFuncItalics(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    UString s = thisValue.toThisString(exec);
    return jsNontrivialString(exec, "<i>" + s + "</i>");
}

JSValuePtr stringProtoFuncStrike(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    UString s = thisValue.toThisString(exec);
    return jsNontrivialString(exec, "<strike>" + s + "</strike>");
}

JSValuePtr stringProtoFuncSub(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    UString s = thisValue.toThisString(exec);
    return jsNontrivialString(exec, "<sub>" + s + "</sub>");
}

JSValuePtr stringProtoFuncSup(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    UString s = thisValue.toThisString(exec);
    return jsNontrivialString(exec, "<sup>" + s + "</sup>");
}

JSValuePtr stringProtoFuncFontcolor(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    JSValuePtr a0 = args.at(exec, 0);
    return jsNontrivialString(exec, "<font color=\"" + a0.toString(exec) + "\">" + s + "</font>");
}

JSValuePtr stringProtoFuncFontsize(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    JSValuePtr a0 = args.at(exec, 0);

    uint32_t smallInteger;
    if (a0.getUInt32(smallInteger) && smallInteger <= 9) {
        unsigned stringSize = s.size();
        unsigned bufferSize = 22 + stringSize;
        UChar* buffer = static_cast<UChar*>(tryFastMalloc(bufferSize * sizeof(UChar)));
        if (!buffer)
            return jsUndefined();
        buffer[0] = '<';
        buffer[1] = 'f';
        buffer[2] = 'o';
        buffer[3] = 'n';
        buffer[4] = 't';
        buffer[5] = ' ';
        buffer[6] = 's';
        buffer[7] = 'i';
        buffer[8] = 'z';
        buffer[9] = 'e';
        buffer[10] = '=';
        buffer[11] = '"';
        buffer[12] = '0' + smallInteger;
        buffer[13] = '"';
        buffer[14] = '>';
        memcpy(&buffer[15], s.data(), stringSize * sizeof(UChar));
        buffer[15 + stringSize] = '<';
        buffer[16 + stringSize] = '/';
        buffer[17 + stringSize] = 'f';
        buffer[18 + stringSize] = 'o';
        buffer[19 + stringSize] = 'n';
        buffer[20 + stringSize] = 't';
        buffer[21 + stringSize] = '>';
        return jsNontrivialString(exec, UString(buffer, bufferSize, false));
    }

    return jsNontrivialString(exec, "<font size=\"" + a0.toString(exec) + "\">" + s + "</font>");
}

JSValuePtr stringProtoFuncAnchor(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    JSValuePtr a0 = args.at(exec, 0);
    return jsNontrivialString(exec, "<a name=\"" + a0.toString(exec) + "\">" + s + "</a>");
}

JSValuePtr stringProtoFuncLink(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    UString s = thisValue.toThisString(exec);
    JSValuePtr a0 = args.at(exec, 0);
    UString linkText = a0.toString(exec);

    unsigned linkTextSize = linkText.size();
    unsigned stringSize = s.size();
    unsigned bufferSize = 15 + linkTextSize + stringSize;
    UChar* buffer = static_cast<UChar*>(tryFastMalloc(bufferSize * sizeof(UChar)));
    if (!buffer)
        return jsUndefined();
    buffer[0] = '<';
    buffer[1] = 'a';
    buffer[2] = ' ';
    buffer[3] = 'h';
    buffer[4] = 'r';
    buffer[5] = 'e';
    buffer[6] = 'f';
    buffer[7] = '=';
    buffer[8] = '"';
    memcpy(&buffer[9], linkText.data(), linkTextSize * sizeof(UChar));
    buffer[9 + linkTextSize] = '"';
    buffer[10 + linkTextSize] = '>';
    memcpy(&buffer[11 + linkTextSize], s.data(), stringSize * sizeof(UChar));
    buffer[11 + linkTextSize + stringSize] = '<';
    buffer[12 + linkTextSize + stringSize] = '/';
    buffer[13 + linkTextSize + stringSize] = 'a';
    buffer[14 + linkTextSize + stringSize] = '>';
    return jsNontrivialString(exec, UString(buffer, bufferSize, false));
}

} // namespace JSC
