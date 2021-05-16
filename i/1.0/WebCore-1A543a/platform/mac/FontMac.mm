/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#import "config.h"
#import "Font.h"

#import "Logging.h"
#import "BlockExceptions.h"
#import "FoundationExtras.h"

#import "FontFallbackList.h"
#import "GraphicsContext.h"
#import "Settings.h"

#import "FontData.h"

#import "IntRect.h"

#import "WebCoreSystemInterface.h"
#import "WebCoreTextRenderer.h"

#import "WKGraphics.h"
#import <GraphicsServices/GraphicsServices.h>

#define SYNTHETIC_OBLIQUE_ANGLE 14

#define POP_DIRECTIONAL_FORMATTING 0x202C
#define LEFT_TO_RIGHT_OVERRIDE 0x202D
#define RIGHT_TO_LEFT_OVERRIDE 0x202E

#if defined(__LP64__)
#define URefCon void*
#else
#define URefCon UInt32
#endif

using namespace std;

namespace WebCore {


Font::Font(const FontPlatformData& fontData)
:m_letterSpacing(0), m_wordSpacing(0)
{
    m_fontList = new FontFallbackList();
    m_fontList->setPlatformFont(fontData);
	m_fontDescription.setSpecifiedSize (GSFontGetSize(fontData.font));
	m_fontDescription.setComputedSize (GSFontGetSize(fontData.font));
	m_fontDescription.setItalic (GSFontGetTraits(fontData.font) & GSItalicFontMask);
	m_fontDescription.setWeight ((GSFontGetTraits(fontData.font) & GSBoldFontMask)?cBoldWeight:cNormalWeight);
}


void Font::drawGlyphs(GraphicsContext* context, const FontData* font, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point, bool setColor) const
{
    // FIXME: Grab the CGContext from the GraphicsContext eventually, when we have made sure to shield against flipping caused by calls into us
    // from Safari.
    CGContextRef cgContext = WKGetCurrentGraphicsContext();

    // Enable font smoothing
    bool originalShouldUseFontSmoothing = CGContextGetShouldSmoothFonts(cgContext);
    bool shouldUseFontSmoothing = WebCoreShouldUseFontSmoothing();
    if (shouldUseFontSmoothing != originalShouldUseFontSmoothing) 
        CGContextSetShouldSmoothFonts(cgContext, shouldUseFontSmoothing);

#if defined(__arm__)
    // Font smoothing style
    CGFontSmoothingStyle originalFontSmoothingStyle = CGContextGetFontSmoothingStyle(cgContext);
    CGFontSmoothingStyle fontSmoothingStyle = WebCoreFontSmoothingStyle();
    if (shouldUseFontSmoothing && fontSmoothingStyle != originalFontSmoothingStyle)
        CGContextSetFontSmoothingStyle(cgContext, fontSmoothingStyle);
    
    // Font antialiasing style
    CGFontAntialiasingStyle originalFontAntialiasingStyle = CGContextGetFontAntialiasingStyle(cgContext);
    CGFontAntialiasingStyle fontAntialiasingStyle = WebCoreFontAntialiasingStyle();
    if (fontAntialiasingStyle != originalFontAntialiasingStyle)
        CGContextSetFontAntialiasingStyle(cgContext, fontAntialiasingStyle);
#endif /* defined(__arm__) */
    
    const FontPlatformData& platformData = font->platformData();
    
    GSFontSetFont(cgContext, platformData.font);
    float fontSize = GSFontGetSize(platformData.font);
    CGAffineTransform matrix = CGAffineTransformMakeScale(fontSize, fontSize);
    matrix.b = -matrix.b;
    matrix.d = -matrix.d;    
    if (platformData.syntheticOblique)
        matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, -tanf(SYNTHETIC_OBLIQUE_ANGLE * acosf(0) / 90), 1, 0, 0)); 
    CGContextSetTextMatrix(cgContext, matrix);

    CGContextSetFontSize(cgContext, 1.0f);

    if (setColor) {
        CGColorRef color = cgColor(context->pen().color());
        GSColorSetColor(cgContext, color);
        CGColorRelease (color);
    }

    CGContextSetTextPosition(cgContext, point.x(), point.y());
    CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    if (font->m_syntheticBoldOffset) {
        CGContextSetTextPosition(cgContext, point.x() + font->m_syntheticBoldOffset, point.y());
        CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    }

    if (shouldUseFontSmoothing != originalShouldUseFontSmoothing) 
        CGContextSetShouldSmoothFonts(cgContext, originalShouldUseFontSmoothing);
        
#if defined(__arm__)
    if (shouldUseFontSmoothing && fontSmoothingStyle != originalFontSmoothingStyle)
        CGContextSetFontSmoothingStyle(cgContext, originalFontSmoothingStyle);
    
    if (fontAntialiasingStyle != originalFontAntialiasingStyle)
        CGContextSetFontAntialiasingStyle(cgContext, originalFontAntialiasingStyle);
#endif /* defined(__arm__) */
}

}
