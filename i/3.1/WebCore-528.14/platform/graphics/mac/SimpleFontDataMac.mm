/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
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
#import "SimpleFontData.h"

#import "BlockExceptions.h"
#import "Color.h"
#import "FloatRect.h"
#import "Font.h"
#import "FontCache.h"
#import "FontDescription.h"
#import "SharedBuffer.h"
#import "WebCoreSystemInterface.h"
#import <CoreText/CoreText.h>
#import <float.h>
#import <unicode/uchar.h>
#import <wtf/Assertions.h>
#import <wtf/StdLibExtras.h>
#import <wtf/RetainPtr.h>

#include <wtf/UnusedParam.h>


namespace WebCore {
  
const float smallCapsFontSizeMultiplier = 0.7f;
const float contextDPI = 72.0f;
static inline float scaleEmToUnits(float x, unsigned unitsPerEm) { return x * (contextDPI / (contextDPI * unitsPerEm)); }

void SimpleFontData::platformInit()
{
    m_syntheticBoldOffset = m_font.m_syntheticBold ? ceilf(GSFontGetSize(m_font.font())  / 24.0f) : 0.f;
    m_spaceGlyph = 0;
    m_spaceWidth = 0;
    m_adjustedSpaceWidth = 0;
    m_ascent = ceilf(GSFontGetAscent(m_font.font()));
    m_descent = ceilf(-GSFontGetDescent(m_font.font()));
    m_lineSpacing = GSFontGetLineSpacing(m_font.font());
    m_lineGap = GSFontGetLineGap(m_font.font());
    m_xHeight = GSFontGetXHeight(m_font.font());
    m_unitsPerEm = GSFontGetUnitsPerEm(m_font.font());    
}

void SimpleFontData::platformDestroy()
{
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        if (isCustomFont()) {
            FontPlatformData smallCapsFontData(m_font);
            smallCapsFontData.m_size = smallCapsFontData.m_size * smallCapsFontSizeMultiplier;
            m_smallCapsFontData = new SimpleFontData(smallCapsFontData, true, false);
        } else {
            BEGIN_BLOCK_OBJC_EXCEPTIONS;
            GSFontTraitMask fontTraits= GSFontGetTraits(m_font.font());
            FontPlatformData smallCapsFont(GSFontCreateWithName(GSFontGetFamilyName(m_font.font()), fontTraits, GSFontGetSize(m_font.font()) * smallCapsFontSizeMultiplier));
            
            // AppKit resets the type information (screen/printer) when you convert a font to a different size.
            // We have to fix up the font that we're handed back.
            UNUSED_PARAM(fontDescription);

            if (smallCapsFont.font()) {
                if (m_font.m_syntheticBold)
                    fontTraits |= GSBoldFontMask;
                if (m_font.m_syntheticOblique)
                    fontTraits |= GSItalicFontMask;

                GSFontTraitMask smallCapsFontTraits= GSFontGetTraits(smallCapsFont.font());
                smallCapsFont.m_syntheticBold = (fontTraits & GSBoldFontMask) && !(smallCapsFontTraits & GSBoldFontMask);
                smallCapsFont.m_syntheticOblique = (fontTraits & GSItalicFontMask) && !(smallCapsFontTraits & GSItalicFontMask);

                m_smallCapsFontData = fontCache()->getCachedFontData(&smallCapsFont);
            }
            END_BLOCK_OBJC_EXCEPTIONS;
        }
    }
    return m_smallCapsFontData;
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    UNUSED_PARAM(characters);
    UNUSED_PARAM(length);
    return 0;
}

void SimpleFontData::determinePitch()
{
    GSFontRef f = m_font.font();
    m_treatAsFixedPitch = false;
    // GSFont is null in the case of SVG fonts for example.
    if (f) {
        const char *fullName = GSFontGetFullName(f);
        const char *familyName = GSFontGetFamilyName(f);
        m_treatAsFixedPitch = (GSFontIsFixedPitch(f) || (fullName && (strcasecmp(fullName, "Osaka-Mono") == 0 || strcasecmp(fullName, "MS-PGothic") == 0)));        
        if (familyName && strcasecmp(familyName, "Courier New") == 0) // Special case Courier New to not be treated as fixed pitch, as this will make use of a hacked space width which is undesireable for iPhone (see rdar://6269783)
            m_treatAsFixedPitch = false;
    }
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    if (m_font.m_isImageFont)
        return std::min(m_font.m_size + (m_font.m_size <= 15.0f ? 4.0f : 6.0f), 22.0f); // returns the proper scaled advance for the image size - see Font::drawGlyphs

    GSFontRef font = m_font.font();
    float pointSize = GSFontGetSize(font);
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    CGSize advance;
    if (!GSFontGetGlyphTransformedAdvances(font, &m, kCGFontRenderingModeAntialiased, &glyph, 1, &advance)) {      
        LOG_ERROR("Unable to cache glyph widths for %@ %f", GSFontGetFullName(font), pointSize);
        advance.width = 0;
    }
    return advance.width + m_syntheticBoldOffset;
}

#if USE(ATSUI)
void SimpleFontData::checkShapesArabic() const
{
    ASSERT(!m_checkedShapesArabic);

    m_checkedShapesArabic = true;
    
    ATSUFontID fontID = m_font.m_atsuFontID;
    if (!fontID) {
        LOG_ERROR("unable to get ATSUFontID for %@", m_font.font());
        return;
    }

    // This function is called only on fonts that contain Arabic glyphs. Our
    // heuristic is that if such a font has a glyph metamorphosis table, then
    // it includes shaping information for Arabic.
    FourCharCode tables[] = { 'morx', 'mort' };
    for (unsigned i = 0; i < sizeof(tables) / sizeof(tables[0]); ++i) {
        ByteCount tableSize;
        OSStatus status = ATSFontGetTable(fontID, tables[i], 0, 0, 0, &tableSize);
        if (status == noErr) {
            m_shapesArabic = true;
            return;
        }

        if (status != kATSInvalidFontTableAccess)
            LOG_ERROR("ATSFontGetTable failed (%d)", status);
    }
}
#endif

#if USE(CORE_TEXT)
CTFontRef SimpleFontData::getCTFont() const
{
    if (!m_CTFont) {
        m_CTFont.adoptCF(CTFontCreateWithGraphicsFont(GSFontGetCGFont(m_font.font()), m_font.size(), &CGAffineTransformIdentity, NULL));
    }
    return m_CTFont.get();
}

CFDictionaryRef SimpleFontData::getCFStringAttributes() const
{
    if (m_CFStringAttributes)
        return m_CFStringAttributes.get();

    static const float kerningAdjustmentValue = 0;
    static CFNumberRef kerningAdjustment = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &kerningAdjustmentValue);

    static const int ligaturesNotAllowedValue = 0;
    static CFNumberRef ligaturesNotAllowed = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &ligaturesNotAllowedValue);

    static const int ligaturesAllowedValue = 1;
    static CFNumberRef ligaturesAllowed = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &ligaturesAllowedValue);

    static const void* attributeKeys[] = { kCTFontAttributeName, kCTKernAttributeName, kCTLigatureAttributeName };
    const void* attributeValues[] = { getCTFont(), kerningAdjustment, platformData().allowsLigatures() ? ligaturesAllowed : ligaturesNotAllowed };

    m_CFStringAttributes.adoptCF(CFDictionaryCreate(NULL, attributeKeys, attributeValues, sizeof(attributeKeys) / sizeof(*attributeKeys), &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    return m_CFStringAttributes.get();
}

#endif

} // namespace WebCore
