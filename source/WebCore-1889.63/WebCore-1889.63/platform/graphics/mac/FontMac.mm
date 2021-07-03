/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#import "config.h"
#import "Font.h"

#import "GlyphBuffer.h"
#import "GraphicsContext.h"
#import "Logging.h"
#import "SimpleFontData.h"
#import "WebCoreSystemInterface.h"
#if !PLATFORM(IOS)
#import <AppKit/AppKit.h>
#endif
#import <wtf/MathExtras.h>

#if PLATFORM(IOS)
#import "BitmapImage.h"
#import "SharedBuffer.h"
#import "SoftLinking.h"
#import "WAKView.h"
#import "WKGraphics.h"
#import <CoreFoundation/CFPriv.h>
#import <CoreText/CoreText.h>
#import <GraphicsServices/GraphicsServices.h>
#import <objc/runtime.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>

#if ENABLE(LETTERPRESS)
#import <CoreUI/CUICatalog.h>
#import <CoreUI/CUIStyleEffectConfiguration.h>

SOFT_LINK_PRIVATE_FRAMEWORK(CoreUI)
SOFT_LINK_CLASS(CoreUI, CUICatalog)
SOFT_LINK_CLASS(CoreUI, CUIStyleEffectConfiguration)

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK(UIKit, _UIKitGetTextEffectsCatalog, CUICatalog *, (void), ())
#endif

#endif // PLATFORM(IOS)

#define SYNTHETIC_OBLIQUE_ANGLE 14

#ifdef __LP64__
#define URefCon void*
#else
#define URefCon UInt32
#endif

using namespace std;

namespace WebCore {

bool Font::canReturnFallbackFontsForComplexText()
{
    return true;
}

bool Font::canExpandAroundIdeographsInComplexText()
{
    return true;
}

// CTFontGetVerticalTranslationsForGlyphs is different on Snow Leopard.  It returns values for a font-size of 1
// without unitsPerEm applied.  We have to apply a transform that scales up to the point size and that also 
// divides by unitsPerEm.
static bool hasBrokenCTFontGetVerticalTranslationsForGlyphs()
{
#if !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED == 1060
    return true;
#else
    return false;
#endif
}

#if PLATFORM(IOS) && ENABLE(LETTERPRESS)
static void showLetterpressedGlyphsWithAdvances(const FloatPoint& point, const SimpleFontData* font, CGContextRef context, const CGGlyph* glyphs, const CGSize* advances, size_t count)
{
    if (!count)
        return;

    const FontPlatformData& platformData = font->platformData();

    if (platformData.orientation() == Vertical)
        return;

    CGContextSetTextPosition(context, point.x(), point.y());
    Vector<CGPoint, 256> positions(count);
    CGAffineTransform matrix = CGAffineTransformInvert(CGContextGetTextMatrix(context));
    positions[0] = CGPointZero;
    for (size_t i = 1; i < count; ++i) {
        CGSize advance = CGSizeApplyAffineTransform(advances[i - 1], matrix);
        positions[i].x = positions[i - 1].x + advance.width;
        positions[i].y = positions[i - 1].y + advance.height;
    }

    CTFontRef ctFont = platformData.ctFont();
    CGContextSetFontSize(context, CTFontGetSize(ctFont));

    static CUICatalog *catalog = _UIKitGetTextEffectsCatalog();
    if (!catalog)
        return;

    static CUIStyleEffectConfiguration *styleConfiguration;
    if (!styleConfiguration) {
        styleConfiguration = [[getCUIStyleEffectConfigurationClass() alloc] init];
        styleConfiguration.useSimplifiedEffect = YES;
    }

    [catalog drawGlyphs:glyphs atPositions:positions.data() inContext:context withFont:ctFont count:count stylePresetName:@"_UIKitNewLetterpressStyle" styleConfiguration:styleConfiguration foregroundColor:CGContextGetFillColorAsColor(context)];
}
#endif

static void showGlyphsWithAdvances(const FloatPoint& point, const SimpleFontData* font, CGContextRef context, const CGGlyph* glyphs, const CGSize* advances, size_t count)
{
    if (!count)
        return;
    CGContextSetTextPosition(context, point.x(), point.y());

    const FontPlatformData& platformData = font->platformData();
    Vector<CGPoint, 256> positions(count);
    if (platformData.isColorBitmapFont()) {
        CGAffineTransform matrix = CGAffineTransformInvert(CGContextGetTextMatrix(context));
        positions[0] = CGPointZero;
        for (size_t i = 1; i < count; ++i) {
            CGSize advance = CGSizeApplyAffineTransform(advances[i - 1], matrix);
            positions[i].x = positions[i - 1].x + advance.width;
            positions[i].y = positions[i - 1].y + advance.height;
        }
    }
    bool isVertical = font->platformData().orientation() == Vertical;
    if (isVertical) {
        CGAffineTransform savedMatrix;
        CGAffineTransform rotateLeftTransform = CGAffineTransformMake(0, -1, 1, 0, 0, 0);
        savedMatrix = CGContextGetTextMatrix(context);
        CGAffineTransform runMatrix = CGAffineTransformConcat(savedMatrix, rotateLeftTransform);
        CGContextSetTextMatrix(context, runMatrix);

        CGAffineTransform translationsTransform;
        if (hasBrokenCTFontGetVerticalTranslationsForGlyphs()) {
            translationsTransform = CGAffineTransformMake(platformData.m_size, 0, 0, platformData.m_size, 0, 0);
            translationsTransform = CGAffineTransformConcat(translationsTransform, rotateLeftTransform);
            CGFloat unitsPerEm = CGFontGetUnitsPerEm(platformData.cgFont());
            translationsTransform = CGAffineTransformConcat(translationsTransform, CGAffineTransformMakeScale(1 / unitsPerEm, 1 / unitsPerEm));
        } else
            translationsTransform = rotateLeftTransform;

        Vector<CGSize, 256> translations(count);
        CTFontGetVerticalTranslationsForGlyphs(platformData.ctFont(), glyphs, translations.data(), count);

        CGAffineTransform transform = CGAffineTransformInvert(CGContextGetTextMatrix(context));

        CGPoint position = FloatPoint(point.x(), point.y() + font->fontMetrics().floatAscent(IdeographicBaseline) - font->fontMetrics().floatAscent());
        for (size_t i = 0; i < count; ++i) {
            CGSize translation = CGSizeApplyAffineTransform(translations[i], translationsTransform);
            positions[i] = CGPointApplyAffineTransform(CGPointMake(position.x - translation.width, position.y + translation.height), transform);
            position.x += advances[i].width;
            position.y += advances[i].height;
        }
        if (!platformData.isColorBitmapFont())
            CGContextShowGlyphsAtPositions(context, glyphs, positions.data(), count);
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
        else
            CTFontDrawGlyphs(platformData.ctFont(), glyphs, positions.data(), count, context);
#endif
        CGContextSetTextMatrix(context, savedMatrix);
    } else {
        if (!platformData.isColorBitmapFont())
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CGContextShowGlyphsWithAdvances(context, glyphs, advances, count);
#pragma clang diagnostic pop
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
        else
            CTFontDrawGlyphs(platformData.ctFont(), glyphs, positions.data(), count, context);
#endif
    }
}

#if !PLATFORM(IOS)
void Font::drawGlyphs(GraphicsContext* context, const SimpleFontData* font, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point) const
#else
void Font::drawGlyphs(GraphicsContext* context, const SimpleFontData* font, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& anchorPoint, bool /*setColor*/) const
#endif
{
    if (!font->platformData().size())
        return;

    CGContextRef cgContext = context->platformContext();

    bool shouldSmoothFonts = true;
    bool changeFontSmoothing = false;
    
    switch(fontDescription().fontSmoothing()) {
    case Antialiased: {
        context->setShouldAntialias(true);
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
        break;
    }
    case SubpixelAntialiased: {
        context->setShouldAntialias(true);
        shouldSmoothFonts = true;
        changeFontSmoothing = true;
        break;
    }
    case NoSmoothing: {
        context->setShouldAntialias(false);
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
        break;
    }
    case AutoSmoothing: {
        // For the AutoSmooth case, don't do anything! Keep the default settings.
        break; 
    }
    default: 
        ASSERT_NOT_REACHED();
    }
    
    if (!shouldUseSmoothing()) {
        shouldSmoothFonts = false;
        changeFontSmoothing = true;
    }

#if !PLATFORM(IOS)
    bool originalShouldUseFontSmoothing = false;
    if (changeFontSmoothing) {
        originalShouldUseFontSmoothing = wkCGContextGetShouldSmoothFonts(cgContext);
        CGContextSetShouldSmoothFonts(cgContext, shouldSmoothFonts);
    }
#endif

    const FontPlatformData& platformData = font->platformData();
#if !PLATFORM(IOS)
    NSFont* drawFont;
    if (!isPrinterFont()) {
        drawFont = [platformData.font() screenFont];
        if (drawFont != platformData.font())
            // We are getting this in too many places (3406411); use ERROR so it only prints on debug versions for now. (We should debug this also, eventually).
            LOG_ERROR("Attempting to set non-screen font (%@) when drawing to screen.  Using screen font anyway, may result in incorrect metrics.",
                [[[platformData.font() fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    } else {
        drawFont = [platformData.font() printerFont];
        if (drawFont != platformData.font())
            NSLog(@"Attempting to set non-printer font (%@) when printing.  Using printer font anyway, may result in incorrect metrics.",
                [[[platformData.font() fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    }
#endif

    CGContextSetFont(cgContext, platformData.cgFont());
#if !PLATFORM(IOS)
    CGAffineTransform matrix = CGAffineTransformIdentity;
    if (drawFont && !platformData.isColorBitmapFont())
        memcpy(&matrix, [drawFont matrix], sizeof(matrix));
#else
    FloatPoint point = anchorPoint;
    float fontSize = platformData.size();
    if (platformData.m_isEmoji) {
        if (!context->emojiDrawingEnabled())
            return;

        // Mimic the positioining of non-bitmap glyphs, which are not subpixel-positioned.
        point.setY(ceilf(point.y()));

        // Emoji glyphs snap to the CSS pixel grid.
        point.setX(floorf(point.x()));

        // Emoji glyphs are offset one CSS pixel to the right.
        point.move(1, 0);

        // Emoji glyphs are offset vertically based on font size.
        float y = point.y();
        if (fontSize <= 15) {
            // Undo Core Text's y adjustment.
            static float yAdjustmentFactor = iosExecutableWasLinkedOnOrAfterVersion(wkIOSSystemVersion_6_0) ? .19 : .1;
            point.setY(floorf(y - yAdjustmentFactor * (fontSize + 2) + 2));
        } else {
            if (fontSize < 26)
                y -= .35f * fontSize - 10;

            // Undo Core Text's y adjustment.
            static float yAdjustment = iosExecutableWasLinkedOnOrAfterVersion(wkIOSSystemVersion_6_0) ? 3.8 : 2;
            point.setY(floorf(y - yAdjustment));
        }
    }

#if ENABLE(LETTERPRESS)
    bool isLetterpress = (context->textDrawingMode() & TextModeLetterpress);
    CGAffineTransform matrix = (isLetterpress || platformData.isColorBitmapFont()) ? CGAffineTransformIdentity : CGAffineTransformMakeScale(fontSize, fontSize);
#else
    CGAffineTransform matrix = platformData.isColorBitmapFont() ? CGAffineTransformIdentity : CGAffineTransformMakeScale(fontSize, fontSize);
#endif
#endif
    matrix.b = -matrix.b;
    matrix.d = -matrix.d;

#if !PLATFORM(IOS) || !ENABLE(LETTERPRESS)
    if (platformData.m_syntheticOblique) {
#else
    if (platformData.m_syntheticOblique && !isLetterpress) {
#endif
        static float obliqueSkew = tanf(SYNTHETIC_OBLIQUE_ANGLE * piFloat / 180);
        if (font->platformData().orientation() == Vertical)
            matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, obliqueSkew, 0, 1, 0, 0));
        else
            matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, -obliqueSkew, 1, 0, 0));
    }
    CGContextSetTextMatrix(cgContext, matrix);

#if !PLATFORM(IOS)
#if PLATFORM(MAC)
    wkSetCGFontRenderingMode(cgContext, drawFont, context->shouldSubpixelQuantizeFonts());
#else
    wkSetCGFontRenderingMode(cgContext, drawFont);
#endif
    if (drawFont)
        CGContextSetFontSize(cgContext, 1.0f);
    else
        CGContextSetFontSize(cgContext, platformData.m_size);
#else
    CGContextSetFontSize(cgContext, 1.0f);
#endif


    FloatSize shadowOffset;
    float shadowBlur;
    Color shadowColor;
    ColorSpace shadowColorSpace;
    ColorSpace fillColorSpace = context->fillColorSpace();
    context->getShadow(shadowOffset, shadowBlur, shadowColor, shadowColorSpace);

    AffineTransform contextCTM = context->getCTM();
    float syntheticBoldOffset = font->syntheticBoldOffset();
    if (syntheticBoldOffset && !contextCTM.isIdentityOrTranslationOrFlipped()) {
        FloatSize horizontalUnitSizeInDevicePixels = contextCTM.mapSize(FloatSize(1, 0));
        float horizontalUnitLengthInDevicePixels = sqrtf(horizontalUnitSizeInDevicePixels.width() * horizontalUnitSizeInDevicePixels.width() + horizontalUnitSizeInDevicePixels.height() * horizontalUnitSizeInDevicePixels.height());
        if (horizontalUnitLengthInDevicePixels)
            syntheticBoldOffset /= horizontalUnitLengthInDevicePixels;
    };

    bool hasSimpleShadow = context->textDrawingMode() == TextModeFill && shadowColor.isValid() && !shadowBlur && !platformData.isColorBitmapFont() && (!context->shadowsIgnoreTransforms() || contextCTM.isIdentityOrTranslationOrFlipped()) && !context->isInTransparencyLayer();
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        context->clearShadow();
        Color fillColor = context->fillColor();
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
        context->setFillColor(shadowFillColor, shadowColorSpace);
        float shadowTextX = point.x() + shadowOffset.width();
        // If shadows are ignoring transforms, then we haven't applied the Y coordinate flip yet, so down is negative.
        float shadowTextY = point.y() + shadowOffset.height() * (context->shadowsIgnoreTransforms() ? -1 : 1);
        showGlyphsWithAdvances(FloatPoint(shadowTextX, shadowTextY), font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
#if !PLATFORM(IOS)
        if (syntheticBoldOffset)
#else
        if (syntheticBoldOffset && !platformData.m_isEmoji)
#endif
            showGlyphsWithAdvances(FloatPoint(shadowTextX + syntheticBoldOffset, shadowTextY), font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
        context->setFillColor(fillColor, fillColorSpace);
    }

#if PLATFORM(IOS) && ENABLE(LETTERPRESS)
    if (isLetterpress)
        showLetterpressedGlyphsWithAdvances(point, font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
    else
#endif
    showGlyphsWithAdvances(point, font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
#if !PLATFORM(IOS)
    if (syntheticBoldOffset)
#else
    if (syntheticBoldOffset && !platformData.m_isEmoji)
#endif
        showGlyphsWithAdvances(FloatPoint(point.x() + syntheticBoldOffset, point.y()), font, cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);

    if (hasSimpleShadow)
        context->setShadow(shadowOffset, shadowBlur, shadowColor, shadowColorSpace);

#if !PLATFORM(IOS)
    if (changeFontSmoothing)
        CGContextSetShouldSmoothFonts(cgContext, originalShouldUseFontSmoothing);
#endif
}

}
