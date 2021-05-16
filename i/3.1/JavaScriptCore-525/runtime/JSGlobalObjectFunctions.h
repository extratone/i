/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef JSGlobalObjectFunctions_h
#define JSGlobalObjectFunctions_h

#include <wtf/unicode/Unicode.h>

namespace JSC {

    class ArgList;
    class ExecState;
    class JSObject;
    class JSValuePtr;

    // FIXME: These functions should really be in JSGlobalObject.cpp, but putting them there
    // is a 0.5% reduction.

    JSValuePtr globalFuncEval(ExecState*, JSObject*, JSValuePtr, const ArgList&);
    JSValuePtr globalFuncParseInt(ExecState*, JSObject*, JSValuePtr, const ArgList&);
    JSValuePtr globalFuncParseFloat(ExecState*, JSObject*, JSValuePtr, const ArgList&);
    JSValuePtr globalFuncIsNaN(ExecState*, JSObject*, JSValuePtr, const ArgList&);
    JSValuePtr globalFuncIsFinite(ExecState*, JSObject*, JSValuePtr, const ArgList&);
    JSValuePtr globalFuncDecodeURI(ExecState*, JSObject*, JSValuePtr, const ArgList&);
    JSValuePtr globalFuncDecodeURIComponent(ExecState*, JSObject*, JSValuePtr, const ArgList&);
    JSValuePtr globalFuncEncodeURI(ExecState*, JSObject*, JSValuePtr, const ArgList&);
    JSValuePtr globalFuncEncodeURIComponent(ExecState*, JSObject*, JSValuePtr, const ArgList&);
    JSValuePtr globalFuncEscape(ExecState*, JSObject*, JSValuePtr, const ArgList&);
    JSValuePtr globalFuncUnescape(ExecState*, JSObject*, JSValuePtr, const ArgList&);
#ifndef NDEBUG
    JSValuePtr globalFuncJSCPrint(ExecState*, JSObject*, JSValuePtr, const ArgList&);
#endif

    static const double mantissaOverflowLowerBound = 9007199254740992.0;
    double parseIntOverflow(const char*, int length, int radix);
    bool isStrWhiteSpace(UChar);

} // namespace JSC

#endif // JSGlobalObjectFunctions_h
