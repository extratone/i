/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "FontCache.h"
#import "FontPlatformData.h"
#import "FontPlatformData.h"
#import "Font.h"
#import "WebFontCache.h"
#import "WebCoreSystemInterface.h"
#import "ListBox.h"
#import "WebCoreStringTruncator.h"

namespace WebCore
{

#define MINIMUM_GLYPH_CACHE_SIZE 1536 * 1024

void FontCache::platformInit()
{
	size_t s = 1536*1024;

    wkSetUpFontCache(s);
}

const FontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
    if (length > 0 && *characters == 0xF8FF)
        return getCachedFontData(getAppleFallbackFont(font));
    return getCachedFontData(getLastResortFallbackFont(font));
}

FontPlatformData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    // Attempt to find an appropriate font using a match based on 
    // the presence of keywords in the the requested names.  For example, we'll
    // match any name that contains "Arabic" to Geeza Pro.
    FontPlatformData* platformData = 0;
    const FontFamily* currFamily = &font.fontDescription().family();
    while (currFamily && !platformData) {
        if (currFamily->family().length()) {
            static String matchWords[3] = { String("Arabic"), String("Pashto"), String("Urdu") };
            static AtomicString geezaStr("Geeza Pro");
            for (int j = 0; j < 3 && !platformData; ++j)
                if (currFamily->family().contains(matchWords[j], false))
                    platformData = getCachedFontPlatformData(font.fontDescription(), geezaStr);
        }
        currFamily = currFamily->next();
    }

    return platformData;
}

FontPlatformData* FontCache::getLastResortFallbackFont(const Font& font)
{
    static AtomicString arialUnicodeMSStr("Arial Unicode MS");
    FontPlatformData* platformFont = getCachedFontPlatformData(font.fontDescription(), arialUnicodeMSStr);    
    return platformFont;
}

FontPlatformData* FontCache::getAppleFallbackFont(const Font& font)
{
    static AtomicString helveticaStr("Helvetica");
    FontPlatformData* platformFont = getCachedFontPlatformData(font.fontDescription(), helveticaStr);    
    return platformFont;
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    static AtomicString courier("Courier");
    if (equalIgnoringCase(family, courier))
        return 0;    
    
    GSFontTraitMask traits = 0;
    if (fontDescription.italic())
        traits |= GSItalicFontMask;
    if (fontDescription.bold())
        traits |= GSBoldFontMask;
    float size = fontDescription.computedPixelSize();
	
	GSFontRef gsFont= [WebFontCache createFontWithFamily: family traits: traits size: size];
	if (!gsFont)
		return 0;
	
    GSFontTraitMask actualTraits = 0;
    if (fontDescription.bold() || fontDescription.italic())
        actualTraits = GSFontGetTraits(gsFont);
    
    FontPlatformData* result = new FontPlatformData;
    
    // Use the correct font for print vs. screen.
    result->font = gsFont;
    result->syntheticBold = (traits & GSBoldFontMask) && !(actualTraits & GSBoldFontMask);
    result->syntheticOblique = (traits & GSItalicFontMask) && !(actualTraits & GSItalicFontMask);	
    return result;
}

}
