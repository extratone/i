/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef C_UTILITY_H_
#define C_UTILITY_H_

#include "npruntime.h"

namespace KJS {

class ExecState;
class JSValue;

namespace Bindings {

typedef uint16_t NPUTF16;

enum NP_ValueType {
    NP_NumberValueType,
    NP_StringValueType,
    NP_BooleanValueType,
    NP_NullValueType,
    NP_UndefinedValueType,
    NP_ObjectValueType,
    NP_InvalidValueType
};

void convertNPStringToUTF16(const NPString*, NPUTF16** UTF16Chars, unsigned int* UTF16Length);
void convertUTF8ToUTF16(const NPUTF8* UTF8Chars, int UTF8Length, NPUTF16** UTF16Chars, unsigned int* UTF16Length);
void coerceValueToNPVariantStringType(ExecState*, JSValue*, NPVariant* result);
void convertValueToNPVariant(ExecState*, JSValue*, NPVariant* result);
JSValue* convertNPVariantToValue(ExecState*, const NPVariant*);

struct PrivateIdentifier {
    union {
        const NPUTF8* string;
        int32_t number;
    } value;
    bool isString;
};

} }

#endif
