/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#import "Font.h"
#import "FontData.h"
#import "Color.h"

#import <wtf/Assertions.h>


#import "FontCache.h"

#import "WebCoreSystemInterface.h"

#import "FloatRect.h"
#import "FontDescription.h"

#import <float.h>

#import <unicode/uchar.h>
#import <unicode/unorm.h>

// FIXME: Just temporary for the #defines of constants that we will eventually stop using.
#import "GlyphBuffer.h"
namespace WebCore
{

#define SMALLCAPS_FONTSIZE_MULTIPLIER 0.7f
#define SPACE 0x0020
#define CONTEXT_DPI (72.0)
#define SCALE_EM_TO_UNITS(X, U_PER_EM) (X * ((1.0 * CONTEXT_DPI) / (CONTEXT_DPI * U_PER_EM)))
void FontData::platformInit()
{
    m_syntheticBoldOffset = m_font.syntheticBold ? ceilf(GSFontGetSize(m_font.font)  / 24.0f) : 0.f;
    m_spaceGlyph = 0;
    m_spaceWidth = 0;
    m_smallCapsFont = 0;
    m_adjustedSpaceWidth = 0;
}

void FontData::platformDestroy()
{
    if (m_font.font)
        CFRelease(m_font.font);
}

FontData* FontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        NS_DURING
			GSFontTraitMask fontTraits= GSFontGetTraits(m_font.font);
			FontPlatformData smallCapsFont(GSFontCreateWithName(GSFontGetFamilyName(m_font.font), fontTraits, GSFontGetSize(m_font.font) * SMALLCAPS_FONTSIZE_MULTIPLIER));
            // AppKit resets the type information (screen/printer) when you convert a font to a different size.
            // We have to fix up the font that we're handed back.
            if (smallCapsFont.font) {
                if (m_font.syntheticBold)
                    fontTraits |= GSBoldFontMask;
                if (m_font.syntheticOblique)
                    fontTraits |= GSItalicFontMask;
					
				GSFontTraitMask smallCapsFontTraits= GSFontGetTraits(smallCapsFont.font);
				smallCapsFont.syntheticBold = (fontTraits & GSBoldFontMask) && !(smallCapsFontTraits & GSBoldFontMask);
				smallCapsFont.syntheticOblique = (fontTraits & GSItalicFontMask) && !(smallCapsFontTraits & GSItalicFontMask);
				
				m_smallCapsFontData = FontCache::getCachedFontData(&smallCapsFont);
            }
        NS_HANDLER
            NSLog(@"uncaught exception selecting font for small caps: %@", localException);
        NS_ENDHANDLER
    }
    return m_smallCapsFontData;
}

bool FontData::containsCharacters(const UChar* characters, int length) const
{
	return 0;
}

void FontData::determinePitch()
{
    GSFontRef f = m_font.font;

	const char *fullName = GSFontGetFullName(f);
    m_treatAsFixedPitch = (GSFontIsFixedPitch(f) || (fullName && (strcasecmp(fullName, "Osaka-Mono") == 0 || strcasecmp(fullName, "MS-PGothic") == 0)));
}

float FontData::platformWidthForGlyph(Glyph glyph) const
{
    GSFontRef font = m_font.font;
    float pointSize = GSFontGetSize(font);
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    CGSize advance;
    if (!GSFontGetGlyphTransformedAdvances(font, &m, kCGFontRenderingModeAntialiased, &glyph, 1, &advance)) {      
        LOG_ERROR("Unable to cache glyph widths for %@ %f", GSFontGetFullName(font), pointSize);
        advance.width = 0;
    }
    return advance.width + m_syntheticBoldOffset;
}

}
