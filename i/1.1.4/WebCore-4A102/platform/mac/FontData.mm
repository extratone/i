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
#import "WebCoreTextRenderer.h"

#import <wtf/Assertions.h>

#import <Foundation/Foundation.h>

#import <WebTextRendererFactory.h>

#import "WebCoreSystemInterface.h"

#import "FloatRect.h"
#import "FontDescription.h"

#import <float.h>

#import <unicode/uchar.h>
#import <unicode/unorm.h>

#import <GraphicsServices/GSColor.h>
#import <GraphicsServices/GSDraw.h>
#import <GraphicsServices/GSGeometry.h>
#import <WebCore/WKGraphics.h>

// FIXME: Just temporary for the #defines of constants that we will eventually stop using.
#import "GlyphBuffer.h"

namespace WebCore
{

// FIXME: FATAL seems like a bad idea; lets stop using it.

#define SMALLCAPS_FONTSIZE_MULTIPLIER 0.7f
#define SYNTHETIC_OBLIQUE_ANGLE 14

// Should be more than enough for normal usage.
#define NUM_SUBSTITUTE_FONT_MAPS 10

// According to http://www.unicode.org/Public/UNIDATA/UCD.html#Canonical_Combining_Class_Values
#define HIRAGANA_KATAKANA_VOICING_MARKS 8

#define SPACE 0x0020
#define NO_BREAK_SPACE 0x00A0
#define ZERO_WIDTH_SPACE 0x200B
#define POP_DIRECTIONAL_FORMATTING 0x202C
#define LEFT_TO_RIGHT_OVERRIDE 0x202D
#define RIGHT_TO_LEFT_OVERRIDE 0x202E

// MAX_GLYPH_EXPANSION is the maximum numbers of glyphs that may be
// use to represent a single Unicode code point.
#define MAX_GLYPH_EXPANSION 4
#define LOCAL_BUFFER_SIZE 2048

// Covers Latin-1.
#define INITIAL_BLOCK_SIZE 0x200

// Get additional blocks of glyphs and widths in bigger chunks.
// This will typically be for other character sets.
#define INCREMENTAL_BLOCK_SIZE 0x400

#define CONTEXT_DPI (72.0)
#define SCALE_EM_TO_UNITS(X, U_PER_EM) (X * ((1.0 * CONTEXT_DPI) / (CONTEXT_DPI * U_PER_EM)))

#define WKGlyphVectorSize (50 * 32)

#define nonGlyphID 65535L

typedef float WebGlyphWidth;

struct WidthMap {
    WidthMap() :next(0), widths(0) {}

    CGGlyph startRange;
    CGGlyph endRange;
    WidthMap *next;
    WebGlyphWidth *widths;
};

typedef struct GlyphEntry {
    Glyph glyph;
    const FontData *renderer;
} GlyphEntry;

struct GlyphMap {
    UChar startRange;
    UChar endRange;
    GlyphMap *next;
    GlyphEntry *glyphs;
};

typedef struct WidthIterator {
    FontData *renderer;
    const WebCoreTextRun *run;
    const WebCoreTextStyle *style;
    unsigned currentCharacter;
    float runWidthSoFar;
    float widthToStart;
    float padding;
    float padPerSpace;
    float finalRoundingWidth;
} WidthIterator;


static const FontData *rendererForAlternateFont(const FontData *, FontPlatformData);

static WidthMap *extendWidthMap(const FontData *, CGGlyph);
static CGGlyph extendGlyphMap(const FontData *, UChar32);

static void freeWidthMap(WidthMap *);
static void freeGlyphMap(GlyphMap *);

// Measuring runs.
static float CG_floatWidthForRun(FontData *, const WebCoreTextRun *, const WebCoreTextStyle *,
    float *widthBuffer, FontData **rendererBuffer, CGGlyph *glyphBuffer, float *startPosition, int *numGlyphsResult);

// Drawing runs.
static void CG_draw(FontData *, const WebCoreTextRun *, const WebCoreTextStyle *, const WebCoreTextGeometry *);

// Selection point detection in runs.
static int CG_pointToOffset(FontData *, const WebCoreTextRun *, const WebCoreTextStyle *,
    int x, bool includePartialGlyphs);

// Selection rect.
static NSRect CG_selectionRect(FontData *, const WebCoreTextRun *, const WebCoreTextStyle *, const WebCoreTextGeometry *);

// Drawing highlight.
static void CG_drawHighlight(FontData *, const WebCoreTextRun *, const WebCoreTextStyle *, const WebCoreTextGeometry *);


// Iterator functions
static void initializeWidthIterator(WidthIterator *iterator, FontData *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style);
static unsigned advanceWidthIterator(WidthIterator *iterator, unsigned offset, float *widths, FontData **renderersUsed, CGGlyph *glyphsUsed);




// Character property functions.

void WebCoreInitializeFont(FontPlatformData *font)
{
    font->font = nil;
    font->syntheticBold = NO;
    font->syntheticOblique = NO;
    font->forPrinter = NO;
}

void WebCoreInitializeTextRun(WebCoreTextRun *run, const UniChar *characters, unsigned int length, int from, int to)
{
    run->characters = characters;
    run->length = length;
    run->from = from;
    run->to = to;
}

void WebCoreInitializeEmptyTextStyle(WebCoreTextStyle *style)
{
    style->textColor = nil;
    style->backgroundColor = nil;
    style->letterSpacing = 0;
    style->wordSpacing = 0;
    style->padding = 0;
    style->families = nil;
    style->smallCaps = NO;
    style->rtl = NO;
    style->directionalOverride = NO;
    style->applyRunRounding = YES;
    style->applyWordRounding = YES;
    style->attemptFontSubstitution = YES;
}

void WebCoreInitializeEmptyTextGeometry(WebCoreTextGeometry *geometry)
{
    geometry->useFontMetricsForSelectionYAndHeight = YES;
}

// Map utility functions

float FontData::widthForGlyph(Glyph glyph) const
{
    WidthMap *map;
    for (map = m_glyphToWidthMap; 1; map = map->next) {
        if (!map)
            map = extendWidthMap(this, glyph);
        if (glyph >= map->startRange && glyph <= map->endRange)
            break;
    }
    float width = map->widths[glyph - map->startRange];
    if (width >= 0)
        return width;
    GSFontRef font = m_font.font;
    float pointSize = GSFontGetSize(font);
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    CGSize advance;
    if (!GSFontGetGlyphTransformedAdvances(font, &m, kCGFontRenderingModeAntialiased, &glyph, 1, &advance)) {      
        LOG_ERROR("Unable to cache glyph widths for %p %f", m_font.font, pointSize);
        advance.width = 0;
    }
    width = advance.width + m_syntheticBoldOffset;
    map->widths[glyph - map->startRange] = width;
    return width;
}



FontData::FontData(const FontPlatformData& f)
: m_font(f), m_characterToGlyphMap(0), m_glyphToWidthMap(0), m_treatAsFixedPitch(false), m_smallCapsFontData(0)
{    
    m_font = f;
    m_syntheticBoldOffset = f.syntheticBold ? ceilf(GSFontGetSize(f.font)  / 24.0f) : 0.f;
    m_spaceGlyph = 0;
    m_spaceWidth = 0;
    m_smallCapsFont = 0;
    m_adjustedSpaceWidth = 0;
}

FontData::~FontData()
{

    freeWidthMap(m_glyphToWidthMap);
    freeGlyphMap(m_characterToGlyphMap);

        
    if (m_font.font)
        CFRelease(m_font.font);
    
    // We only get deleted when the cache gets cleared.  Since the smallCapsRenderer is also in that cache,
    // it will be deleted then, so we don't need to do anything here.
}


void FontData::drawRun(const WebCoreTextRun* run, const WebCoreTextStyle* style, const WebCoreTextGeometry* geometry)
{
        CG_draw(this, run, style, geometry);
}

float FontData::floatWidthForRun(const WebCoreTextRun* run, const WebCoreTextStyle* style)
{
    return CG_floatWidthForRun(this, run, style, 0, 0, 0, 0, 0);
}

static void drawHorizontalLine(float x, float y, float width, CGColorRef color, float thickness, bool shouldAntialias)
{
    CGContextRef cgContext = WKGetCurrentGraphicsContext();

    CGContextSaveGState(cgContext);

    GSColorSetColor (cgContext, color);
    CGContextSetLineWidth(cgContext, thickness);
    CGContextSetShouldAntialias(cgContext, shouldAntialias);

    float halfThickness = thickness / 2;

    CGPoint linePoints[2];
    linePoints[0].x = x + halfThickness;
    linePoints[0].y = y + halfThickness;
    linePoints[1].x = x + width - halfThickness;
    linePoints[1].y = y + halfThickness;
    CGContextStrokeLineSegments(cgContext, linePoints, 2);

    CGContextRestoreGState(cgContext);
}

void FontData::drawLineForCharacters(const FloatPoint& point, float yOffset, int width, const Color& color, float thickness)
{
    // Note: This function assumes that point.x and point.y are integers (and that's currently always the case).
    CGContextRef cgContext = WKGetCurrentGraphicsContext();

    bool printing = NO;

    float x = point.x();
    float y = point.y() + yOffset;

    // Leave 1.0 in user space between the baseline of the text and the top of the underline.
    // FIXME: Is this the right distance for space above the underline? Even for thick underlines on large sized text?
    y += 1;

    if (printing) {
        // When printing, use a minimum thickness of 0.5 in user space.
        // See bugzilla bug 4255 for details of why 0.5 is the right minimum thickness to use while printing.
        if (thickness < 0.5)
            thickness = 0.5;

        // When printing, use antialiasing instead of putting things on integral pixel boundaries.
    } else {
        // On screen, use a minimum thickness of 1.0 in user space (later rounded to an integral number in device space).
        if (thickness < 1)
            thickness = 1;

        // On screen, round all parameters to integer boundaries in device space.
        CGRect lineRect = CGContextConvertRectToDeviceSpace(cgContext, CGRectMake(x, y, width, thickness));
        lineRect.origin.x = roundf(lineRect.origin.x);
        lineRect.origin.y = roundf(lineRect.origin.y);
        lineRect.size.width = roundf(lineRect.size.width);
        lineRect.size.height = roundf(lineRect.size.height);
        if (lineRect.size.height == 0) // don't let thickness round down to 0 pixels
            lineRect.size.height = 1;
        lineRect = CGContextConvertRectToUserSpace(cgContext, lineRect);
        x = lineRect.origin.x;
        y = lineRect.origin.y;
        width = (int)(lineRect.size.width);
        thickness = lineRect.size.height;
    }

    // FIXME: How about using a rectangle fill instead of drawing a line?
    drawHorizontalLine(x, y, width, cgColor(color), thickness, printing);
}

FloatRect FontData::selectionRectForRun(const WebCoreTextRun* run, const WebCoreTextStyle* style, const WebCoreTextGeometry* geometry)
{
        return CG_selectionRect(this, run, style, geometry);
}

void FontData::drawHighlightForRun(const WebCoreTextRun* run, const WebCoreTextStyle* style, const WebCoreTextGeometry* geometry)
{
        CG_drawHighlight(this, run, style, geometry);
}

void FontData::drawLineForMisspelling(const FloatPoint& point, int width)
{
    // Constants for pattern color
    static CGPatternRef spellingPattern = NULL;
    static bool usingDot = false;
    int patternHeight = misspellingLineThickness();
    int patternWidth = misspellingLinePatternWidth();
 
    if (!spellingPattern) {
        CGImageRef image = WKGraphicsCreateImageFromBundleWithName("SpellingDot");
        assert(image); // if image is not available, we want to know
        spellingPattern = WKCreatePatternFromCGImage(image);
        CGImageRelease(image);
        usingDot = true;
    }

    // Make sure to draw only complete dots.
    // NOTE: Code here used to shift the underline to the left and increase the width
    // to make sure everything gets underlined, but that results in drawing out of
    // bounds (e.g. when at the edge of a view) and could make it appear that the
    // space between adjacent misspelled words was underlined.
    if (usingDot) {
        // allow slightly more considering that the pattern ends with a transparent pixel
        int widthMod = width % patternWidth;
        if (patternWidth - widthMod > misspellingLinePatternGapWidth())
            width -= widthMod;
    }
    
    // Draw underline
    CGContextRef context = WKGetCurrentGraphicsContext();
    CGContextSaveGState(context);

    WKSetPattern(context, spellingPattern, YES, YES);

    CGPoint transformedOrigin = CGPointApplyAffineTransform(point, CGContextGetCTM(context));
    CGContextSetPatternPhase(context, CGSizeMake(transformedOrigin.x, transformedOrigin.y));

    WKRectFillUsingOperation(context, CGRectMake(point.x(), point.y(), width, patternHeight), kCGCompositeDover);
    
    CGContextRestoreGState(context);
}

int FontData::pointToOffset(const WebCoreTextRun* run, const WebCoreTextStyle* style, int x, bool includePartialGlyphs)
{
    return CG_pointToOffset(this, run, style, x, includePartialGlyphs);
}

FontData* FontData::smallCapsFontData() const
{
    if (!m_smallCapsFontData) {
	NS_DURING
            FontPlatformData smallCapsFont;
            WebCoreInitializeFont(&smallCapsFont);
            smallCapsFont.font = m_font.font;
	    m_smallCapsFontData = (FontData*)rendererForAlternateFont(this, smallCapsFont);
	NS_HANDLER
            NSLog(@"uncaught exception selecting font for small caps: %@", localException);
	NS_ENDHANDLER
    }
    return m_smallCapsFontData;
}


static const FontData *rendererForAlternateFont(const FontData *renderer, FontPlatformData alternateFont)
{
    if (!alternateFont.font)
        return nil;


    return [[WebTextRendererFactory sharedFactory] rendererWithFont:alternateFont];
}



// Useful page for testing http://home.att.net/~jameskass
static void drawGlyphs(GSFontRef font, CGColorRef color, CGGlyph *glyphs, CGSize *advances, float x, float y, int numGlyphs,
    float syntheticBoldOffset, bool syntheticOblique)
{
    CGContextRef cgContext = WKGetCurrentGraphicsContext();


    bool originalShouldUseFontSmoothing = CGContextGetShouldSmoothFonts(cgContext);
    CGContextSetShouldSmoothFonts(cgContext, WebCoreShouldUseFontSmoothing());
    
    
    GSFontSetFont(cgContext, font);
    float fontSize = GSFontGetSize(font);
    CGContextSetFontRenderingMode(cgContext, kCGFontRenderingModeAntialiased);
    CGAffineTransform matrix = CGAffineTransformMakeScale(fontSize, fontSize);
    matrix.b = -matrix.b;
    matrix.d = -matrix.d;    
    if (syntheticOblique)
        matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, -tanf(SYNTHETIC_OBLIQUE_ANGLE * acosf(0) / 90), 1, 0, 0)); 
    CGContextSetTextMatrix(cgContext, matrix);

    CGContextSetFontSize(cgContext, 1.0f);

    if (color)
        GSColorSetColor(cgContext, color);

    CGContextSetTextPosition(cgContext, x, y);
    CGContextShowGlyphsWithAdvances(cgContext, glyphs, advances, numGlyphs);
    if (syntheticBoldOffset) {
        CGContextSetTextPosition(cgContext, x + syntheticBoldOffset, y);
        CGContextShowGlyphsWithAdvances(cgContext, glyphs, advances, numGlyphs);
    }

    CGContextSetShouldSmoothFonts(cgContext, originalShouldUseFontSmoothing);
}

static void CG_drawHighlight(FontData *renderer, const WebCoreTextRun * run, const WebCoreTextStyle *style, const WebCoreTextGeometry *geometry)
{
    if (run->length == 0)
        return;

    if (style->backgroundColor == nil)
        return;

    // check if special marked text background drawing should be done
#if DRAW_HIGHLIGHTED_TEXT    
    CGContextRef context = WKGetCurrentGraphicsContext();
    GSColorSetColor (context, style->backgroundColor);
    WKRectFillUsingOperation(context, CG_selectionRect(renderer, run, style, geometry), kCGCompositeSover);
#endif
}

static NSRect CG_selectionRect(FontData *renderer, const WebCoreTextRun * run, const WebCoreTextStyle *style, const WebCoreTextGeometry *geometry)
{
    float yPos = geometry->useFontMetricsForSelectionYAndHeight
        ? geometry->point.y() - renderer->ascent() - (renderer->lineGap() / 2) : geometry->selectionY;
    float height = geometry->useFontMetricsForSelectionYAndHeight
        ? renderer->lineSpacing() : geometry->selectionHeight;

    WebCoreTextRun completeRun = *run;
    completeRun.from = 0;
    completeRun.to = run->length;

    WidthIterator it;
    initializeWidthIterator(&it, renderer, &completeRun, style);
    
    advanceWidthIterator(&it, run->from, 0, 0, 0);
    float beforeWidth = it.runWidthSoFar;
    advanceWidthIterator(&it, run->to, 0, 0, 0);
    float afterWidth = it.runWidthSoFar;
    // Using roundf() rather than ceilf() for the right edge as a compromise to ensure correct caret positioning
    if (style->rtl) {
        advanceWidthIterator(&it, run->length, 0, 0, 0);
        float totalWidth = it.runWidthSoFar;
        return NSMakeRect(geometry->point.x() + floorf(totalWidth - afterWidth), yPos, roundf(totalWidth - beforeWidth) - floorf(totalWidth - afterWidth), height);
    } else {
        return NSMakeRect(geometry->point.x() + floorf(beforeWidth), yPos, roundf(afterWidth) - floorf(beforeWidth), height);
    }
}

static void CG_draw(FontData *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, const WebCoreTextGeometry *geometry)
{
    float *widthBuffer, localWidthBuffer[LOCAL_BUFFER_SIZE];
    CGGlyph *glyphBuffer, localGlyphBuffer[LOCAL_BUFFER_SIZE];
    FontData **rendererBuffer, *localRendererBuffer[LOCAL_BUFFER_SIZE];
    CGSize *advances, localAdvanceBuffer[LOCAL_BUFFER_SIZE];
    int numGlyphs = 0, i;
    float startX;
    unsigned length = run->length;
    
    if (run->length == 0)
        return;

    if (length * MAX_GLYPH_EXPANSION > LOCAL_BUFFER_SIZE) {
        advances = new CGSize[length * MAX_GLYPH_EXPANSION];
        widthBuffer = new float[length * MAX_GLYPH_EXPANSION];
        glyphBuffer = new CGGlyph[length * MAX_GLYPH_EXPANSION];
        rendererBuffer = new FontData*[length * MAX_GLYPH_EXPANSION];
    } else {
        advances = localAdvanceBuffer;
        widthBuffer = localWidthBuffer;
        glyphBuffer = localGlyphBuffer;
        rendererBuffer = localRendererBuffer;
    }

    CG_floatWidthForRun(renderer, run, style, widthBuffer, rendererBuffer, glyphBuffer, &startX, &numGlyphs);
        
    // Eek.  We couldn't generate ANY glyphs for the run.
    if (numGlyphs <= 0)
        return;
        
    // Fill the advances array.
    for (i = 0; i < numGlyphs; i++) {
        advances[i].width = widthBuffer[i];
        advances[i].height = 0;
    }

    // Calculate the starting point of the glyphs to be displayed by adding
    // all the advances up to the first glyph.
    startX += geometry->point.x();

    if (style->backgroundColor != nil)
        CG_drawHighlight(renderer, run, style, geometry);
    
    // Swap the order of the glyphs if right-to-left.
    if (style->rtl) {
        int i;
        int mid = numGlyphs / 2;
        int end;
        for (i = 0, end = numGlyphs - 1; i < mid; ++i, --end) {
            CGGlyph gswap1 = glyphBuffer[i];
            CGGlyph gswap2 = glyphBuffer[end];
            glyphBuffer[i] = gswap2;
            glyphBuffer[end] = gswap1;

            CGSize aswap1 = advances[i];
            CGSize aswap2 = advances[end];
            advances[i] = aswap2;
            advances[end] = aswap1;

            FontData *rswap1 = rendererBuffer[i];
            FontData *rswap2 = rendererBuffer[end];
            rendererBuffer[i] = rswap2;
            rendererBuffer[end] = rswap1;
        }
    }

    // Draw each contiguous run of glyphs that use the same renderer.
    FontData *currentRenderer = rendererBuffer[0];
    float nextX = startX;
    int lastFrom = 0;
    int nextGlyph = 0;
    while (nextGlyph < numGlyphs) {
        FontData *nextRenderer = rendererBuffer[nextGlyph];
        if (nextRenderer != currentRenderer) {
            drawGlyphs(currentRenderer->m_font.font, style->textColor, &glyphBuffer[lastFrom], &advances[lastFrom],
                startX, geometry->point.y(), nextGlyph - lastFrom,
                currentRenderer->m_syntheticBoldOffset, currentRenderer->m_font.syntheticOblique);
            lastFrom = nextGlyph;
            currentRenderer = nextRenderer;
            startX = nextX;
        }
        nextX += advances[nextGlyph].width;
        nextGlyph++;
    }
    drawGlyphs(currentRenderer->m_font.font, style->textColor, &glyphBuffer[lastFrom], &advances[lastFrom],
        startX, geometry->point.y(), nextGlyph - lastFrom,
        currentRenderer->m_syntheticBoldOffset, currentRenderer->m_font.syntheticOblique);

    if (advances != localAdvanceBuffer) {
        delete []advances;
        delete []widthBuffer;
        delete []glyphBuffer;
        delete []rendererBuffer;
    }
}

static float CG_floatWidthForRun(FontData *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style, float *widthBuffer, FontData **rendererBuffer, CGGlyph *glyphBuffer, float *startPosition, int *numGlyphsResult)
{
    WidthIterator it;
    WebCoreTextRun completeRun;
    const WebCoreTextRun *aRun;
    if (!style->rtl)
        aRun = run;
    else {
        completeRun = *run;
        completeRun.to = run->length;
        aRun = &completeRun;
    }
    initializeWidthIterator(&it, renderer, aRun, style);
    int numGlyphs = advanceWidthIterator(&it, run->to, widthBuffer, rendererBuffer, glyphBuffer);
    float runWidth = it.runWidthSoFar;
    if (startPosition) {
        if (!style->rtl)
            *startPosition = it.widthToStart;
        else {
            float finalRoundingWidth = it.finalRoundingWidth;
            advanceWidthIterator(&it, run->length, 0, 0, 0);
            *startPosition = it.runWidthSoFar - runWidth + finalRoundingWidth;
        }
    }
    if (numGlyphsResult)
        *numGlyphsResult = numGlyphs;
    return runWidth;
}


static CGGlyph extendGlyphMap(const FontData *renderer, UChar32 c)
{
    GlyphMap *map = new GlyphMap;
    UChar32 end, start;
    unsigned blockSize;
    
    if (renderer->m_characterToGlyphMap == 0)
        blockSize = INITIAL_BLOCK_SIZE;
    else
        blockSize = INCREMENTAL_BLOCK_SIZE;
    start = (c / blockSize) * blockSize;
    end = start + (blockSize - 1);

    map->startRange = start;
    map->endRange = end;
    map->next = 0;
    
    unsigned i;
    unsigned count = end - start + 1;
    unsigned short buffer[INCREMENTAL_BLOCK_SIZE * 2 + 2];
    unsigned bufferLength;

    if (start < 0x10000) {
        bufferLength = count;
        for (i = 0; i < count; i++)
            buffer[i] = i + start;

        if (start == 0) {
            // Control characters must not render at all.
            for (i = 0; i < 0x20; ++i)
                buffer[i] = ZERO_WIDTH_SPACE;
            buffer[0x7F] = ZERO_WIDTH_SPACE;

            // But \n, \t, and nonbreaking space must render as a space.
            buffer[(int)'\n'] = ' ';
            buffer[(int)'\t'] = ' ';
            buffer[NO_BREAK_SPACE] = ' ';
        }
    } else {
        bufferLength = count * 2;
        for (i = 0; i < count; i++) {
            int c = i + start;
            buffer[i * 2] = U16_LEAD(c);
            buffer[i * 2 + 1] = U16_TRAIL(c);
        }
    }

    CGGlyph glyphs[count];
    GSFontGetGlyphsForUnichars(renderer->m_font.font, buffer, glyphs, count);

    map->glyphs = new GlyphEntry[count];
    for (i = 0; i < count; i++) {
        map->glyphs[i].glyph = glyphs[i];
        map->glyphs[i].renderer = renderer;
    }
    
    if (renderer->m_characterToGlyphMap == 0)
        renderer->m_characterToGlyphMap = map;
    else {
        GlyphMap *lastMap = renderer->m_characterToGlyphMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    CGGlyph glyph = map->glyphs[c - start].glyph;
    // Special case for characters 007F-00A0.
    if (glyph == 0 && c >= 0x7F && c <= 0xA0) {
        GSFontGetGlyphsForUnichars(renderer->m_font.font, (UniChar*)&c, &glyph, 1);
        map->glyphs[c - start].glyph = glyph;
    }

    return glyph;
}

static WidthMap *extendWidthMap(const FontData *renderer, CGGlyph glyph)
{
    WidthMap *map = new WidthMap;
    unsigned end;
    CGGlyph start;
    unsigned blockSize;
    unsigned i, count;
    
    GSFontRef f = renderer->m_font.font;
    if (renderer->m_glyphToWidthMap == 0) {
        if (GSFontGetNumberOfGlyphs(f) < INITIAL_BLOCK_SIZE)
            blockSize = GSFontGetNumberOfGlyphs(f);
         else
            blockSize = INITIAL_BLOCK_SIZE;
    } else {
        blockSize = INCREMENTAL_BLOCK_SIZE;
    }
    if (blockSize == 0) {
        start = 0;
    } else {
        start = (glyph / blockSize) * blockSize;
    }
    end = ((unsigned)start) + blockSize; 

    map->startRange = start;
    map->endRange = end;
    count = end - start + 1;

    map->widths = new WebGlyphWidth[count];
    for (i = 0; i < count; i++)
        map->widths[i] = NAN;

    if (renderer->m_glyphToWidthMap == 0)
        renderer->m_glyphToWidthMap = map;
    else {
        WidthMap *lastMap = renderer->m_glyphToWidthMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    return map;
}


static bool advanceWidthIteratorOneCharacter(WidthIterator *iterator, float *totalWidth)
{
    float widths[MAX_GLYPH_EXPANSION];
    FontData *renderers[MAX_GLYPH_EXPANSION];
    CGGlyph glyphs[MAX_GLYPH_EXPANSION];            
    unsigned numGlyphs = advanceWidthIterator(iterator, iterator->currentCharacter + 1, widths, renderers, glyphs);
    unsigned i;
    float w = 0;
    for (i = 0; i < numGlyphs; ++i)
        w += widths[i];
    *totalWidth = w;
    return numGlyphs != 0;
}

static int CG_pointToOffset(FontData *renderer, const WebCoreTextRun * run, const WebCoreTextStyle *style,
    int x, bool includePartialGlyphs)
{
    float delta = (float)x;

    WidthIterator it;    
    initializeWidthIterator(&it, renderer, run, style);

    unsigned offset;

    if (style->rtl) {
        delta -= CG_floatWidthForRun(renderer, run, style, 0, 0, 0, 0, 0);
        while (1) {
            offset = it.currentCharacter;
            float w;
            if (!advanceWidthIteratorOneCharacter(&it, &w))
                break;
            delta += w;
            if (includePartialGlyphs) {
                if (delta - w / 2 >= 0)
                    break;
            } else {
                if (delta >= 0)
                    break;
            }
        }
    } else {
        while (1) {
            offset = it.currentCharacter;
            float w;
            if (!advanceWidthIteratorOneCharacter(&it, &w))
                break;
            delta -= w;
            if (includePartialGlyphs) {
                if (delta + w / 2 <= 0)
                    break;
            } else {
                if (delta <= 0)
                    break;
            }
        }
    }

    return offset - run->from;
}

static void freeWidthMap(WidthMap *map)
{
    while (map) {
        WidthMap *next = map->next;
        delete []map->widths;
        delete map;
        map = next;
    }
}

static void freeGlyphMap(GlyphMap *map)
{
    while (map) {
        GlyphMap *next = map->next;
        delete []map->glyphs;
        delete map;
        map = next;
    }
}

Glyph FontData::glyphForCharacter(const FontData **renderer, unsigned c) const
{
    // this loop is hot, so it is written to avoid LSU stalls
    GlyphMap *map;
    GlyphMap *nextMap;
    for (map = (*renderer)->m_characterToGlyphMap; map; map = nextMap) {
        UChar start = map->startRange;
        nextMap = map->next;
        if (c >= start && c <= map->endRange) {
            GlyphEntry *ge = &map->glyphs[c - start];
            *renderer = ge->renderer;
            return ge->glyph;
        }
    }

    return extendGlyphMap(*renderer, c);
}

static void initializeWidthIterator(WidthIterator *iterator, FontData *renderer, const WebCoreTextRun *run, const WebCoreTextStyle *style) 
{
    iterator->renderer = renderer;
    iterator->run = run;
    iterator->style = style;
    iterator->currentCharacter = run->from;
    iterator->runWidthSoFar = 0;
    iterator->finalRoundingWidth = 0;

    // If the padding is non-zero, count the number of spaces in the run
    // and divide that by the padding for per space addition.
    if (!style->padding) {
        iterator->padding = 0;
        iterator->padPerSpace = 0;
    } else {
        float numSpaces = 0;
        int k;
        for (k = run->from; k < run->to; k++)
            if (Font::treatAsSpace(run->characters[k]))
                numSpaces++;

        iterator->padding = style->padding;
        iterator->padPerSpace = ceilf(iterator->padding / numSpaces);
    }
    
    // Calculate width up to starting position of the run.  This is
    // necessary to ensure that our rounding hacks are always consistently
    // applied.
    if (run->from == 0) {
        iterator->widthToStart = 0;
    } else {
        WebCoreTextRun startPositionRun = *run;
        startPositionRun.from = 0;
        startPositionRun.to = run->length;
        WidthIterator startPositionIterator;
        initializeWidthIterator(&startPositionIterator, renderer, &startPositionRun, style);
        advanceWidthIterator(&startPositionIterator, run->from, 0, 0, 0);
        iterator->widthToStart = startPositionIterator.runWidthSoFar;
    }
}

static UChar32 normalizeVoicingMarks(WidthIterator *iterator)
{
    unsigned currentCharacter = iterator->currentCharacter;
    const WebCoreTextRun *run = iterator->run;
    if (currentCharacter + 1 < (unsigned)run->to) {
        if (u_getCombiningClass(run->characters[currentCharacter + 1]) == HIRAGANA_KATAKANA_VOICING_MARKS) {
            // Normalize into composed form using 3.2 rules.
            UChar normalizedCharacters[2] = { 0, 0 };
            UErrorCode uStatus = (UErrorCode)0;                
            int32_t resultLength = unorm_normalize(&run->characters[currentCharacter], 2,
                UNORM_NFC, UNORM_UNICODE_3_2, &normalizedCharacters[0], 2, &uStatus);
            if (resultLength == 1 && uStatus == 0)
                return normalizedCharacters[0];
        }
    }
    return 0;
}

static unsigned advanceWidthIterator(WidthIterator *iterator, unsigned offset, float *widths, FontData **renderersUsed, CGGlyph *glyphsUsed)
{
    const WebCoreTextRun *run = iterator->run;
    if (offset > (unsigned)run->to)
        offset = run->to;

    unsigned numGlyphs = 0;

    unsigned currentCharacter = iterator->currentCharacter;
    const UniChar *cp = &run->characters[currentCharacter];

    const WebCoreTextStyle *style = iterator->style;
    bool rtl = style->rtl;
    bool needCharTransform = rtl || style->smallCaps;
    bool hasExtraSpacing = style->letterSpacing || style->wordSpacing || style->padding;

    float runWidthSoFar = iterator->runWidthSoFar;
    float lastRoundingWidth = iterator->finalRoundingWidth;

    while (currentCharacter < offset) {
        UChar32 c = *cp;

        unsigned clusterLength = 1;
        if (c >= 0x3041) {
            if (c <= 0x30FE) {
                // Deal with Hiragana and Katakana voiced and semi-voiced syllables.
                // Normalize into composed form, and then look for glyph with base + combined mark.
                // Check above for character range to minimize performance impact.
                UChar32 normalized = normalizeVoicingMarks(iterator);
                if (normalized) {
                    c = normalized;
                    clusterLength = 2;
                }
            } else if (U16_IS_SURROGATE(c)) {
                if (!U16_IS_SURROGATE_LEAD(c))
                    break;

                // Do we have a surrogate pair?  If so, determine the full Unicode (32 bit)
                // code point before glyph lookup.
                // Make sure we have another character and it's a low surrogate.
                if (currentCharacter + 1 >= run->length)
                    break;
                UniChar low = cp[1];
                if (!U16_IS_TRAIL(low))
                    break;
                c = U16_GET_SUPPLEMENTARY(c, low);
                clusterLength = 2;
            }
        }

        const FontData *renderer = iterator->renderer;

        if (needCharTransform) {
            if (rtl)
                c = u_charMirror(c);

            // If small-caps, convert lowercase to upper.
            if (style->smallCaps && !u_isUUppercase(c)) {
                UChar32 upperC = u_toupper(c);
                if (upperC != c) {
                    c = upperC;
                    renderer = renderer->smallCapsFontData();
                }
            }
        }

        Glyph glyph = renderer->glyphForCharacter(&renderer, c);

        // Now that we have glyph and font, get its width.
        WebGlyphWidth width;
        if (c == '\t' && style->tabWidth) {
            width = style->tabWidth - fmodf(style->xpos + runWidthSoFar, style->tabWidth);
        } else {
            width = renderer->widthForGlyph(glyph);
            // We special case spaces in two ways when applying word rounding.
            // First, we round spaces to an adjusted width in all fonts.
            // Second, in fixed-pitch fonts we ensure that all characters that
            // match the width of the space character have the same width as the space character.
            if (width == renderer->m_spaceWidth && (renderer->m_treatAsFixedPitch || glyph == renderer->m_spaceGlyph) && style->applyWordRounding)
                width = renderer->m_adjustedSpaceWidth;
        }

        // Try to find a substitute font if this font didn't have a glyph for a character in the
        // string. If one isn't found we end up drawing and measuring the 0 glyph, usually a box.

        if (hasExtraSpacing) {
            // Account for letter-spacing.
            if (width && style->letterSpacing)
                width += style->letterSpacing;

            if (Font::treatAsSpace(c)) {
                // Account for padding. WebCore uses space padding to justify text.
                // We distribute the specified padding over the available spaces in the run.
                if (style->padding) {
                    // Use left over padding if not evenly divisible by number of spaces.
                    if (iterator->padding < iterator->padPerSpace) {
                        width += iterator->padding;
                        iterator->padding = 0;
                    } else {
                        width += iterator->padPerSpace;
                        iterator->padding -= iterator->padPerSpace;
                    }
                }

                // Account for word spacing.
                // We apply additional space between "words" by adding width to the space character.
                if (currentCharacter != 0 && !Font::treatAsSpace(cp[-1]) && style->wordSpacing)
                    width += style->wordSpacing;
            }
        }

        // Advance past the character we just dealt with.
        cp += clusterLength;
        currentCharacter += clusterLength;

        // Account for float/integer impedance mismatch between CG and KHTML. "Words" (characters 
        // followed by a character defined by isRoundingHackCharacter()) are always an integer width.
        // We adjust the width of the last character of a "word" to ensure an integer width.
        // If we move KHTML to floats we can remove this (and related) hacks.

        float oldWidth = width;

        // Force characters that are used to determine word boundaries for the rounding hack
        // to be integer width, so following words will start on an integer boundary.
        if (style->applyWordRounding && Font::isRoundingHackCharacter(c))
            width = ceilf(width);

        // Check to see if the next character is a "rounding hack character", if so, adjust
        // width so that the total run width will be on an integer boundary.
        if ((style->applyWordRounding && currentCharacter < run->length && Font::isRoundingHackCharacter(*cp))
                || (style->applyRunRounding && currentCharacter >= (unsigned)run->to)) {
            float totalWidth = iterator->widthToStart + runWidthSoFar + width;
            width += ceilf(totalWidth) - totalWidth;
        }

        runWidthSoFar += width;

        if (!widths) {
            assert(!renderersUsed);
            assert(!glyphsUsed);
        } else {
            assert(renderersUsed);
            assert(glyphsUsed);
            *widths++ = (rtl ? oldWidth + lastRoundingWidth : width);
            *renderersUsed++ = (FontData*)renderer;
            *glyphsUsed++ = glyph;
        }

        lastRoundingWidth = width - oldWidth;
        ++numGlyphs;
    }

    iterator->currentCharacter = currentCharacter;
    iterator->runWidthSoFar = runWidthSoFar;
    iterator->finalRoundingWidth = lastRoundingWidth;

    return numGlyphs;
}


}
