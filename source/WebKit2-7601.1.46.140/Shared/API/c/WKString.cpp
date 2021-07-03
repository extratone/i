/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WKString.h"
#include "WKStringPrivate.h"

#include "WKAPICast.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>

using namespace WebKit;

WKTypeID WKStringGetTypeID()
{
    return toAPI(API::String::APIType);
}

WKStringRef WKStringCreateWithUTF8CString(const char* string)
{
    return toAPI(&API::String::create(WTF::String::fromUTF8(string)).leakRef());
}

bool WKStringIsEmpty(WKStringRef stringRef)
{
    return toImpl(stringRef)->stringView().isEmpty();
}

size_t WKStringGetLength(WKStringRef stringRef)
{
    return toImpl(stringRef)->stringView().length();
}

size_t WKStringGetCharacters(WKStringRef stringRef, WKChar* buffer, size_t bufferLength)
{
    static_assert(sizeof(WKChar) == sizeof(UChar), "Size of WKChar must match size of UChar");

    unsigned unsignedBufferLength = std::min<size_t>(bufferLength, std::numeric_limits<unsigned>::max());
    auto substring = toImpl(stringRef)->stringView().substring(0, unsignedBufferLength);

    substring.getCharactersWithUpconvert(static_cast<UChar*>(buffer));
    return substring.length();
}

size_t WKStringGetMaximumUTF8CStringSize(WKStringRef stringRef)
{
    return toImpl(stringRef)->stringView().length() * 3 + 1;
}

size_t WKStringGetUTF8CString(WKStringRef stringRef, char* buffer, size_t bufferSize)
{
    if (!bufferSize)
        return 0;

    auto stringView = toImpl(stringRef)->stringView();

    char* p = buffer;
    WTF::Unicode::ConversionResult result;

    if (stringView.is8Bit()) {
        const LChar* characters = stringView.characters8();
        result = WTF::Unicode::convertLatin1ToUTF8(&characters, characters + stringView.length(), &p, p + bufferSize - 1);
    } else {
        const UChar* characters = stringView.characters16();
        result = WTF::Unicode::convertUTF16ToUTF8(&characters, characters + stringView.length(), &p, p + bufferSize - 1, /* strict */ true);
    }

    if (result != WTF::Unicode::conversionOK && result != WTF::Unicode::targetExhausted)
        return 0;

    *p++ = '\0';
    return p - buffer;
}

bool WKStringIsEqual(WKStringRef aRef, WKStringRef bRef)
{
    return toImpl(aRef)->stringView() == toImpl(bRef)->stringView();
}

bool WKStringIsEqualToUTF8CString(WKStringRef aRef, const char* b)
{
    return toImpl(aRef)->stringView() == WTF::String::fromUTF8(b);
}

bool WKStringIsEqualToUTF8CStringIgnoringCase(WKStringRef aRef, const char* b)
{
    // FIXME: Instead of copying the string here, we should add a version of equalIgnoringCase that takes StringViews.
    return equalIgnoringCase(toImpl(aRef)->string(), WTF::String::fromUTF8(b));
}

WKStringRef WKStringCreateWithJSString(JSStringRef jsStringRef)
{
    auto apiString = jsStringRef ? API::String::create(jsStringRef->string()) : API::String::createNull();

    return toAPI(&apiString.leakRef());
}

JSStringRef WKStringCopyJSString(WKStringRef stringRef)
{
    JSC::initializeThreading();
    return OpaqueJSString::create(toImpl(stringRef)->string()).leakRef();
}
