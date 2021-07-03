/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Autocapitalize.h"

namespace WebCore {

static const AtomicString& valueDefault()
{
    return nullAtom;
}

static const AtomicString& valueOn()
{
    DEFINE_STATIC_LOCAL(const AtomicString, valueOn, ("on"));
    return valueOn;
}

static const AtomicString& valueOff()
{
    DEFINE_STATIC_LOCAL(const AtomicString, valueOff, ("off"));
    return valueOff;
}

static const AtomicString& valueNone()
{
    DEFINE_STATIC_LOCAL(const AtomicString, valueNone, ("none"));
    return valueNone;
}

static const AtomicString& valueWords()
{
    DEFINE_STATIC_LOCAL(const AtomicString, valueWords, ("words"));
    return valueWords;
}

static const AtomicString& valueSentences()
{
    DEFINE_STATIC_LOCAL(const AtomicString, valueSentences, ("sentences"));
    return valueSentences;
}

static const AtomicString& valueAllCharacters()
{
    DEFINE_STATIC_LOCAL(const AtomicString, valueAllCharacters, ("characters"));
    return valueAllCharacters;
}

WebAutocapitalizeType autocapitalizeTypeForAttributeValue(const AtomicString& attr)
{
    // Omitted / missing values are the Default state.
    if (attr.isNull() || attr.isEmpty())
        return WebAutocapitalizeTypeDefault;

    if (equalIgnoringCase(attr, valueOn()) || equalIgnoringCase(attr, valueSentences()))
        return WebAutocapitalizeTypeSentences;
    if (equalIgnoringCase(attr, valueOff()) || equalIgnoringCase(attr, valueNone()))
        return WebAutocapitalizeTypeNone;
    if (equalIgnoringCase(attr, valueWords()))
        return WebAutocapitalizeTypeWords;
    if (equalIgnoringCase(attr, valueAllCharacters()))
        return WebAutocapitalizeTypeAllCharacters;

    // Unrecognized values fall back to "on".
    return WebAutocapitalizeTypeSentences;
}

const AtomicString& stringForAutocapitalizeType(WebAutocapitalizeType type)
{
    switch (type) {
    case WebAutocapitalizeTypeDefault:
        return valueDefault();
    case WebAutocapitalizeTypeNone:
        return valueNone();
    case WebAutocapitalizeTypeSentences:
        return valueSentences();
    case WebAutocapitalizeTypeWords:
        return valueWords();
    case WebAutocapitalizeTypeAllCharacters:
        return valueAllCharacters();
    }

    ASSERT_NOT_REACHED();
    return valueDefault();
}

} // namespace WebCore
