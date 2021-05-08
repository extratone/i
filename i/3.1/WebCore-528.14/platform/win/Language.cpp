/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "Language.h"

#include "CString.h"
#include "PlatformString.h"

namespace WebCore {

static String localeInfo(LCTYPE localeType, const String& fallback)
{
    LANGID langID = GetUserDefaultUILanguage();
    int localeChars = GetLocaleInfo(langID, localeType, 0, 0);
    if (!localeChars)
        return fallback;
    Vector<WCHAR> localeNameBuf(localeChars);
    localeChars = GetLocaleInfo(langID, localeType, localeNameBuf.data(), localeChars);
    if (!localeChars)
        return fallback;
    String localeName = String::adopt(localeNameBuf);
    if (localeName.isEmpty())
        return fallback;

    return localeName;
}

String defaultLanguage()
{
    static String computedDefaultLanguage;
    if (!computedDefaultLanguage.isEmpty())
        return computedDefaultLanguage;

    String languageName = localeInfo(LOCALE_SISO639LANGNAME, "en");
    String countryName = localeInfo(LOCALE_SISO3166CTRYNAME, String());

    if (countryName.isEmpty())
        computedDefaultLanguage = languageName;
    else
        computedDefaultLanguage = String::format("%s-%s", languageName.latin1().data(), countryName.latin1().data());

    return computedDefaultLanguage;
}

}
