/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#define _USE_MATH_DEFINES 1
#include "config.h"
#include "GraphicsContext.h"

#include "TransformationMatrix.h"
#include "FloatConversion.h"
#include "GraphicsContextPrivate.h"
#include "GraphicsContextPlatformPrivateCG.h"
#include "ImageBuffer.h"
#include "KURL.h"
#include "Path.h"
#include "Pattern.h"
#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGPDFContext.h>
#include <wtf/MathExtras.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/RetainPtr.h>

#include <CoreGraphics/CGContextGState.h>
#include <wtf/HashMap.h>
#include <wtf/UnusedParam.h>


using namespace std;

namespace WebCore {

static void setCGFillColor(CGContextRef context, const Color& color)
{
    CGFloat red, green, blue, alpha;
    color.getRGBA(red, green, blue, alpha);
    CGContextSetRGBFillColor(context, red, green, blue, alpha);
}

static void setCGStrokeColor(CGContextRef context, const Color& color)
{
    CGFloat red, green, blue, alpha;
    color.getRGBA(red, green, blue, alpha);
    CGContextSetRGBStrokeColor(context, red, green, blue, alpha);
}

GraphicsContext::GraphicsContext(CGContextRef cgContext, bool setContextColors)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(cgContext))
{
    setPaintingDisabled(!cgContext);
    if (cgContext && setContextColors) {
        // Make sure the context starts in sync with our state.
        setPlatformFillColor(fillColor());
        setPlatformStrokeColor(strokeColor());
        CGContextSetShouldAntialias(m_data->m_cgContext, true);
        CGContextSetShouldAntialiasFonts(m_data->m_cgContext, true);
    }
}

GraphicsContext::~GraphicsContext()
{
    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

CGContextRef GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    ASSERT(m_data->m_cgContext);
    return m_data->m_cgContext;
}

void GraphicsContext::savePlatformState()
{
    // Note: Do not use this function within this class implementation, since we want to avoid the extra
    // save of the secondary context (in GraphicsContextPlatformPrivateCG.h).
    CGContextSaveGState(platformContext());
    m_data->save();
}

void GraphicsContext::restorePlatformState()
{
    // Note: Do not use this function within this class implementation, since we want to avoid the extra
    // restore of the secondary context (in GraphicsContextPlatformPrivateCG.h).
    CGContextRestoreGState(platformContext());
    m_data->restore();
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    // FIXME: this function does not handle patterns and gradients
    // like drawPath does, it probably should.
    if (paintingDisabled())
        return;

    CGContextRef context = platformContext();

    if (fillColor().alpha())
        CGContextFillRect(context, rect);

    if (strokeStyle() != NoStroke && strokeColor().alpha()) {
        // We do a fill of four rects to simulate the stroke of a border.
        Color oldFillColor = fillColor();
        if (oldFillColor != strokeColor())
            setCGFillColor(context, strokeColor());
        CGRect rects[4] = {
            FloatRect(rect.x(), rect.y(), rect.width(), 1),
            FloatRect(rect.x(), rect.bottom() - 1, rect.width(), 1),
            FloatRect(rect.x(), rect.y() + 1, 1, rect.height() - 2),
            FloatRect(rect.right() - 1, rect.y() + 1, 1, rect.height() - 2)
        };
        CGContextFillRects(context, rects, 4);
        if (oldFillColor != strokeColor())
            setCGFillColor(context, oldFillColor);
    }
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2, bool antialias)
{
    if (paintingDisabled())
        return;

    if (strokeStyle() == NoStroke || !strokeColor().alpha())
        return;

    float width = strokeThickness();

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());
    
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (strokeStyle() == DottedStroke || strokeStyle() == DashedStroke) {
        if (isVerticalLine) {
            p1.move(0, width);
            p2.move(0, -width);
        } else {
            p1.move(width, 0);
            p2.move(-width, 0);
        }
    }
    
    if (((int)width) % 2) {
        if (isVerticalLine) {
            // We're a vertical line.  Adjust our x.
            p1.move(0.5f, 0.0f);
            p2.move(0.5f, 0.0f);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.move(0.0f, 0.5f);
            p2.move(0.0f, 0.5f);
        }
    }
    
    int patWidth = 0;
    switch (strokeStyle()) {
        case NoStroke:
        case SolidStroke:
            break;
        case DottedStroke:
            patWidth = (int)width;
            break;
        case DashedStroke:
            patWidth = 3 * (int)width;
            break;
    }

    CGContextRef context = platformContext();
    
    if (shouldAntialias()) {
        // force antialiasing on for line patterns as they don't look good with it turned off (5459772)
        CGContextSetShouldAntialias(context, patWidth ? true : antialias);
    }

    if (patWidth) {
        CGContextSaveGState(context);

        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        setCGFillColor(context, strokeColor());  // The save/restore make it safe to mutate the fill color here without setting it back to the old color.
        if (isVerticalLine) {
            CGContextFillRect(context, FloatRect(p1.x() - width / 2, p1.y() - width, width, width));
            CGContextFillRect(context, FloatRect(p2.x() - width / 2, p2.y(), width, width));
        } else {
            CGContextFillRect(context, FloatRect(p1.x() - width, p1.y() - width / 2, width, width));
            CGContextFillRect(context, FloatRect(p2.x(), p2.y() - width / 2, width, width));
        }

        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = (isVerticalLine ? (point2.y() - point1.y()) : (point2.x() - point1.x())) - 2*(int)width;
        int remainder = distance % patWidth;
        int coverage = distance - remainder;
        int numSegments = coverage / patWidth;

        float patternOffset = 0.0f;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0f;
        else {
            bool evenNumberOfSegments = numSegments % 2 == 0;
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder / 2;
                } else
                    patternOffset = patWidth / 2;
            } else {
                if (remainder)
                    patternOffset = (patWidth - remainder)/2;
            }
        }
        
        const CGFloat dottedLine[2] = { patWidth, patWidth };
        CGContextSetLineDash(context, patternOffset, dottedLine, 2);
    }

    CGContextBeginPath(context);
    CGContextMoveToPoint(context, p1.x(), p1.y());
    CGContextAddLineToPoint(context, p2.x(), p2.y());

    CGContextStrokePath(context);

    if (patWidth)
        CGContextRestoreGState(context);
        
    if (shouldAntialias())
        CGContextSetShouldAntialias(context, true);
}

void GraphicsContext::drawJoinedLines(CGPoint points [], unsigned count, bool antialias, CGLineCap lineCap)
{
    if (paintingDisabled() || !count)
        return;

    CGContextRef context = platformContext();
    float width = CGContextGetLineWidth(context);

    CGContextSaveGState(context);
    
    CGContextSetShouldAntialias(context, antialias);

    CGContextSetLineWidth(context, width < 1 ? 1 : width);

    CGContextBeginPath(context);
    
    CGContextSetLineCap(context, lineCap);
    
    CGContextMoveToPoint(context, points[0].x, points[0].y);
    
    for (unsigned i = 1; i < count; ++i)
        CGContextAddLineToPoint(context, points[i].x, points[i].y);

    CGContextStrokePath(context);

    CGContextRestoreGState(context);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    // FIXME: CG added CGContextAddEllipseinRect in Tiger, so we should be able to quite easily draw an ellipse.
    // This code can only handle circles, not ellipses. But khtml only
    // uses it for circles.
    ASSERT(rect.width() == rect.height());

    if (paintingDisabled())
        return;
        
    CGContextRef context = platformContext();
    CGContextBeginPath(context);
    float r = (float)rect.width() / 2;
    CGContextAddArc(context, rect.x() + r, rect.y() + r, r, 0.0f, 2.0f * piFloat, 0);
    CGContextClosePath(context);

    drawPath();
}

void GraphicsContext::drawEllipse(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    CGContextRef context(platformContext());

    CGContextSaveGState(context);

    setCGFillColor(context, fillColor());
    setCGStrokeColor(context, strokeColor());

    CGContextSetLineWidth(context, strokeThickness());
    
    CGContextBeginPath(context);
    CGContextAddEllipseInRect(context, rect);

    CGContextFillPath(context);
    CGContextStrokePath(context);
    
    CGContextRestoreGState(context);
}

void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{ 
    if (paintingDisabled() || strokeStyle() == NoStroke || strokeThickness() <= 0.0f || !strokeColor().alpha())
        return;
    
    CGContextRef context = platformContext();
    CGContextSaveGState(context);
    CGContextBeginPath(context);
    CGContextSetShouldAntialias(context, false);
    
    int x = rect.x();
    int y = rect.y();
    float w = (float)rect.width();
    float h = (float)rect.height();
    float scaleFactor = h / w;
    float reverseScaleFactor = w / h;
    
    if (w != h)
        scale(FloatSize(1, scaleFactor));
    
    float hRadius = w / 2;
    float vRadius = h / 2;
    float fa = startAngle;
    float falen =  fa + angleSpan;
    float start = -fa * piFloat / 180.0f;
    float end = -falen * piFloat / 180.0f;
    CGContextAddArc(context, x + hRadius, (y + vRadius) * reverseScaleFactor, hRadius, start, end, true);

    if (w != h)
        scale(FloatSize(1, reverseScaleFactor));
    
    
    float width = strokeThickness();
    int patWidth = 0;
    
    switch (strokeStyle()) {
        case DottedStroke:
            patWidth = (int)(width / 2);
            break;
        case DashedStroke:
            patWidth = 3 * (int)(width / 2);
            break;
        default:
            break;
    }
    
    if (patWidth) {
        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance;
        if (hRadius == vRadius)
            distance = static_cast<int>((piFloat * hRadius) / 2.0f);
        else // We are elliptical and will have to estimate the distance
            distance = static_cast<int>((piFloat * sqrtf((hRadius * hRadius + vRadius * vRadius) / 2.0f)) / 2.0f);
        
        int remainder = distance % patWidth;
        int coverage = distance - remainder;
        int numSegments = coverage / patWidth;

        float patternOffset = 0.0f;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0f;
        else {
            bool evenNumberOfSegments = numSegments % 2 == 0;
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder / 2.0f;
                } else
                    patternOffset = patWidth / 2.0f;
            } else {
                if (remainder)
                    patternOffset = (patWidth - remainder) / 2.0f;
            }
        }
    
        const CGFloat dottedLine[2] = { patWidth, patWidth };
        CGContextSetLineDash(context, patternOffset, dottedLine, 2);
    }

    CGContextStrokePath(context);
    
    CGContextRestoreGState(context);
}

void GraphicsContext::drawConvexPolygon(size_t npoints, const FloatPoint* points, bool antialiased)
{
    if (paintingDisabled() || !fillColor().alpha() && (strokeThickness() <= 0 || strokeStyle() == NoStroke))
        return;

    if (npoints <= 1)
        return;

    CGContextRef context = platformContext();

    if (antialiased != shouldAntialias())
        CGContextSetShouldAntialias(context, antialiased);
    
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, points[0].x(), points[0].y());
    for (size_t i = 1; i < npoints; i++)
        CGContextAddLineToPoint(context, points[i].x(), points[i].y());
    CGContextClosePath(context);

    drawPath();
    
    if (antialiased != shouldAntialias())
        CGContextSetShouldAntialias(context, shouldAntialias());
}

void GraphicsContext::applyStrokePattern()
{
    CGContextRef cgContext = platformContext();
    
    CGPatternRef platformPattern = m_common->state.strokePattern.get()->createPlatformPattern(getCTM());
    if (!platformPattern)
        return;

    CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(0);
    CGContextSetStrokeColorSpace(cgContext, patternSpace);
    CGColorSpaceRelease(patternSpace);

    const CGFloat patternAlpha = 1;
    CGContextSetStrokePattern(cgContext, platformPattern, &patternAlpha);
    CGPatternRelease(platformPattern);
}

void GraphicsContext::applyFillPattern()
{
    CGContextRef cgContext = platformContext();

    CGPatternRef platformPattern = m_common->state.fillPattern.get()->createPlatformPattern(getCTM());
    if (!platformPattern)
        return;

    CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(0);
    CGContextSetFillColorSpace(cgContext, patternSpace);
    CGColorSpaceRelease(patternSpace);

    const CGFloat patternAlpha = 1;
    CGContextSetFillPattern(cgContext, platformPattern, &patternAlpha);
    CGPatternRelease(platformPattern);
}

static inline bool calculateDrawingMode(const GraphicsContextState& state, CGPathDrawingMode& mode)
{
    bool shouldFill = state.fillColorSpace == PatternColorSpace || state.fillColor.alpha();
    bool shouldStroke = state.strokeColorSpace == PatternColorSpace || (state.strokeStyle != NoStroke && state.strokeColor.alpha());
    bool useEOFill = state.fillRule == RULE_EVENODD;

    if (shouldFill) {
        if (shouldStroke) {
            if (useEOFill)
                mode = kCGPathEOFillStroke;
            else
                mode = kCGPathFillStroke;
        } else { // fill, no stroke
            if (useEOFill)
                mode = kCGPathEOFill;
            else
                mode = kCGPathFill;
        }
    } else {
        // Setting mode to kCGPathStroke even if shouldStroke is false. In that case, we return false and mode will not be used,
        // but the compiler will not compain about an uninitialized variable.
        mode = kCGPathStroke;
    }

    return shouldFill || shouldStroke;
}

void GraphicsContext::drawPath()
{
    if (paintingDisabled())
        return;

    CGContextRef context = platformContext();
    const GraphicsContextState& state = m_common->state;

    if (state.fillColorSpace == GradientColorSpace || state.strokeColorSpace == GradientColorSpace) {
        // We don't have any optimized way to fill & stroke a path using gradients
        fillPath();
        strokePath();
        return;
    }
    
    if (state.fillColorSpace == PatternColorSpace)
        applyFillPattern();
    if (state.strokeColorSpace == PatternColorSpace)
        applyStrokePattern();

    CGPathDrawingMode drawingMode;
    if (calculateDrawingMode(state, drawingMode))
        CGContextDrawPath(context, drawingMode);
}

static inline void fillPathWithFillRule(CGContextRef context, WindRule fillRule)
{
    if (fillRule == RULE_EVENODD)
        CGContextEOFillPath(context);
    else
        CGContextFillPath(context);
}

void GraphicsContext::fillPath()
{
    if (paintingDisabled())
        return;

    CGContextRef context = platformContext();
    switch (m_common->state.fillColorSpace) {
    case SolidColorSpace:
        if (fillColor().alpha())
            fillPathWithFillRule(context, fillRule());
        break;
    case PatternColorSpace:
        applyFillPattern();
        fillPathWithFillRule(context, fillRule());
        break;
    case GradientColorSpace:
        CGContextSaveGState(context);
        if (fillRule() == RULE_EVENODD)
            CGContextEOClip(context);
        else
            CGContextClip(context);
        CGContextDrawShading(context, m_common->state.fillGradient->platformGradient());
        CGContextRestoreGState(context);
        break;
    }
}

void GraphicsContext::strokePath()
{
    if (paintingDisabled())
        return;

    CGContextRef context = platformContext();
    switch (m_common->state.strokeColorSpace) {
    case SolidColorSpace:
        if (strokeColor().alpha())
            CGContextStrokePath(context);
        break;
    case PatternColorSpace:
        applyStrokePattern();
        CGContextStrokePath(context);
        break;
    case GradientColorSpace:
        CGContextSaveGState(context);
        CGContextReplacePathWithStrokedPath(context);
        CGContextClip(context);
        CGContextDrawShading(context, m_common->state.strokeGradient->platformGradient());
        CGContextRestoreGState(context);
        break;
    }
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    switch (m_common->state.fillColorSpace) {
    case SolidColorSpace:
        if (fillColor().alpha())
            CGContextFillRect(context, rect);
        break;
    case PatternColorSpace:
        applyFillPattern();
        CGContextFillRect(context, rect);
        break;
    case GradientColorSpace:
        CGContextSaveGState(context);
        CGContextClipToRect(context, rect);
        CGContextDrawShading(context, m_common->state.fillGradient->platformGradient());
        CGContextRestoreGState(context);
        break;
    }
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;
    if (color.alpha()) {
        CGContextRef context = platformContext();
        Color oldFillColor = fillColor();
        if (oldFillColor != color)
            setCGFillColor(context, color);
        CGContextFillRect(context, rect);
        if (oldFillColor != color)
            setCGFillColor(context, oldFillColor);
    }
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color)
{
    if (paintingDisabled() || !color.alpha())
        return;

    CGContextRef context = platformContext();
    Color oldFillColor = fillColor();
    if (oldFillColor != color)
        setCGFillColor(context, color);

    addPath(Path::createRoundedRectangle(rect, topLeft, topRight, bottomLeft, bottomRight));
    fillPath();

    if (oldFillColor != color)
        setCGFillColor(context, oldFillColor);
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;
    CGContextClipToRect(platformContext(), rect);
    m_data->clip(rect);
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;
        
    CGRect rects[2] = { CGContextGetClipBoundingBox(platformContext()), rect };
    CGContextBeginPath(platformContext());
    CGContextAddRects(platformContext(), rects, 2);
    CGContextEOClip(platformContext());
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;
        
    CGContextBeginPath(platformContext());
    CGContextAddRect(platformContext(), CGContextGetClipBoundingBox(platformContext()));
    CGContextAddEllipseInRect(platformContext(), rect);
    CGContextEOClip(platformContext());
}

void GraphicsContext::clipPath(WindRule clipRule)
{
    if (paintingDisabled())
        return;

    CGContextRef context = platformContext();

    if (!CGContextIsPathEmpty(context)) {
        if (clipRule == RULE_EVENODD)
            CGContextEOClip(context);
        else
            CGContextClip(context);
    }
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;

    clip(rect);
    CGContextRef context = platformContext();
    
    // Add outer ellipse
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.y(), rect.width(), rect.height()));
    // Add inner ellipse.
    CGContextAddEllipseInRect(context, CGRectMake(rect.x() + thickness, rect.y() + thickness,
        rect.width() - (thickness * 2), rect.height() - (thickness * 2)));
    
    CGContextEOClip(context);
}

void GraphicsContext::clipToImageBuffer(const FloatRect& rect, const ImageBuffer* imageBuffer)
{
    if (paintingDisabled())
        return;
    
    CGContextTranslateCTM(platformContext(), rect.x(), rect.y() + rect.height());
    CGContextScaleCTM(platformContext(), 1, -1);
    CGContextClipToMask(platformContext(), FloatRect(FloatPoint(), rect.size()), imageBuffer->image()->getCGImageRef());
    CGContextScaleCTM(platformContext(), 1, -1);
    CGContextTranslateCTM(platformContext(), -rect.x(), -rect.y() - rect.height());
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextSaveGState(context);
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);
    m_data->beginTransparencyLayer();
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
    m_data->endTransparencyLayer();
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::setPlatformShadow(const IntSize& size, int blur, const Color& color)
{
    if (paintingDisabled())
        return;
    CGFloat width = size.width();
    CGFloat height = size.height();
    CGFloat blurRadius = blur;
    CGContextRef context = platformContext();

    if (!m_common->state.shadowsIgnoreTransforms) {
        CGAffineTransform transform = CGContextGetCTM(context);

        CGFloat A = transform.a * transform.a + transform.b * transform.b;
        CGFloat B = transform.a * transform.c + transform.b * transform.d;
        CGFloat C = B;
        CGFloat D = transform.c * transform.c + transform.d * transform.d;

        CGFloat smallEigenvalue = narrowPrecisionToCGFloat(sqrt(0.5 * ((A + D) - sqrt(4 * B * C + (A - D) * (A - D)))));

        // Extreme "blur" values can make text drawing crash or take crazy long times, so clamp
        blurRadius = min(blur * smallEigenvalue, narrowPrecisionToCGFloat(1000.0));

        CGSize sizeInDeviceSpace = CGSizeApplyAffineTransform(size, transform);

        width = sizeInDeviceSpace.width;
        height = sizeInDeviceSpace.height;

    }

    // Work around <rdar://problem/5539388> by ensuring that the offsets will get truncated
    // to the desired integer.
    static const CGFloat extraShadowOffset = narrowPrecisionToCGFloat(1.0 / 128);
    if (width > 0)
        width += extraShadowOffset;
    else if (width < 0)
        width -= extraShadowOffset;

    if (height > 0)
        height += extraShadowOffset;
    else if (height < 0)
        height -= extraShadowOffset;

    // Check for an invalid color, as this means that the color was not set for the shadow
    // and we should therefore just use the default shadow color.
    if (!color.isValid())
        CGContextSetShadow(context, CGSizeMake(width, height), blurRadius);
    else {
        CGColorRef colorCG = cgColor(color);
        CGContextSetShadowWithColor(context,
                                    CGSizeMake(width, height),
                                    blurRadius, 
                                    colorCG);
        CGColorRelease(colorCG);
    }
}

void GraphicsContext::clearPlatformShadow()
{
    if (paintingDisabled())
        return;
    CGContextSetShadowWithColor(platformContext(), CGSizeZero, 0, 0);
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;
    CGContextSetMiterLimit(platformContext(), limit);
}

void GraphicsContext::setAlpha(float alpha)
{
    if (paintingDisabled())
        return;
    CGContextSetAlpha(platformContext(), alpha);
}

void GraphicsContext::clearRect(const FloatRect& r)
{
    if (paintingDisabled())
        return;
    CGContextClearRect(platformContext(), r);
}

void GraphicsContext::strokeRect(const FloatRect& r, float lineWidth)
{
    if (paintingDisabled())
        return;

    CGContextRef context = platformContext();
    switch (m_common->state.strokeColorSpace) {
    case SolidColorSpace:
        if (strokeColor().alpha())
            CGContextStrokeRectWithWidth(context, r, lineWidth);
        break;
    case PatternColorSpace:
        applyStrokePattern();
        CGContextStrokeRectWithWidth(context, r, lineWidth);
        break;
    case GradientColorSpace:
        CGContextSaveGState(context);
        setStrokeThickness(lineWidth);
        CGContextAddRect(context, r);
        CGContextReplacePathWithStrokedPath(context);
        CGContextClip(context);
        CGContextDrawShading(context, m_common->state.strokeGradient->platformGradient());
        CGContextRestoreGState(context);
        break;
    }
}

void GraphicsContext::setLineCap(LineCap cap)
{
    if (paintingDisabled())
        return;
    switch (cap) {
        case ButtCap:
            CGContextSetLineCap(platformContext(), kCGLineCapButt);
            break;
        case RoundCap:
            CGContextSetLineCap(platformContext(), kCGLineCapRound);
            break;
        case SquareCap:
            CGContextSetLineCap(platformContext(), kCGLineCapSquare);
            break;
    }
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    CGContextSetLineDash(platformContext(), dashOffset, dashes.data(), dashes.size());
}

void GraphicsContext::setLineJoin(LineJoin join)
{
    if (paintingDisabled())
        return;
    switch (join) {
        case MiterJoin:
            CGContextSetLineJoin(platformContext(), kCGLineJoinMiter);
            break;
        case RoundJoin:
            CGContextSetLineJoin(platformContext(), kCGLineJoinRound);
            break;
        case BevelJoin:
            CGContextSetLineJoin(platformContext(), kCGLineJoinBevel);
            break;
    }
}

void GraphicsContext::beginPath()
{
    CGContextBeginPath(platformContext());
}

void GraphicsContext::addPath(const Path& path)
{
    CGContextAddPath(platformContext(), path.platformPath());
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextBeginPath(context);
    CGContextAddPath(context, path.platformPath());
    CGContextClip(context);
    m_data->clip(path);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;
        
    CGContextBeginPath(platformContext());
    CGContextAddRect(platformContext(), CGContextGetClipBoundingBox(platformContext()));
    CGContextAddPath(platformContext(), path.platformPath());
    CGContextEOClip(platformContext());
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    CGContextScaleCTM(platformContext(), size.width(), size.height());
    m_data->scale(size);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::rotate(float angle)
{
    if (paintingDisabled())
        return;
    CGContextRotateCTM(platformContext(), angle);
    m_data->rotate(angle);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;
    CGContextTranslateCTM(platformContext(), x, y);
    m_data->translate(x, y);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::concatCTM(const TransformationMatrix& transform)
{
    if (paintingDisabled())
        return;
    CGContextConcatCTM(platformContext(), transform);
    m_data->concatCTM(transform);
    m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

TransformationMatrix GraphicsContext::getCTM() const
{
    CGAffineTransform t = CGContextGetCTM(platformContext());
    return TransformationMatrix(t.a, t.b, t.c, t.d, t.tx, t.ty);
}

static void InterpolateLinearGradient(void *info, const CGFloat *inData, CGFloat *outData)
{
    float   alpha = inData[0],
            inverse = 1.0f - alpha;

    outData[0] = inverse * static_cast<IPhoneGradientRef>(info)->start[0] + alpha * static_cast<IPhoneGradientRef>(info)->end[0];
    outData[1] = inverse * static_cast<IPhoneGradientRef>(info)->start[1] + alpha * static_cast<IPhoneGradientRef>(info)->end[1];
    outData[2] = inverse * static_cast<IPhoneGradientRef>(info)->start[2] + alpha * static_cast<IPhoneGradientRef>(info)->end[2];
    outData[3] = inverse * static_cast<IPhoneGradientRef>(info)->start[3] + alpha * static_cast<IPhoneGradientRef>(info)->end[3];
}

static void InterpolateExponentialGradient(void *info, const CGFloat *inData, CGFloat *outData)
{
    float a = inData[0];
    
    for(int i = 0; i < 4; ++i) {
        float   end = logf(max(static_cast<IPhoneGradientRef>(info)->end[i], 0.01f));
        float start = logf(max(static_cast<IPhoneGradientRef>(info)->start[i], 0.01f));
        
        outData[i] = expf(start - (end + start) * a);
    }
}

static CGFunctionRef GetSharedFunctionRef(IPhoneGradientRef aGradient, Interpolation anInterpolation)
{
    CGFunctionRef function = NULL;

    static HashMap<IPhoneGradientRef, CGFunctionRef> * linearFunctionRefs = NULL;
    static HashMap<IPhoneGradientRef, CGFunctionRef> * exponentialFunctionRefs = NULL;
    
    if (anInterpolation == LinearInterpolation) {
    
        if (!linearFunctionRefs) linearFunctionRefs = new HashMap<IPhoneGradientRef, CGFunctionRef>;
        else function = linearFunctionRefs->get(aGradient);
    
        if (!function) {
            static struct CGFunctionCallbacks linearFunctionCallbacks =  { 0, InterpolateLinearGradient, NULL };
        
            linearFunctionRefs->set(aGradient, function = CGFunctionCreate(aGradient, 1, NULL, 4, NULL, &linearFunctionCallbacks));
        }
    
        return function;
    }
    else {
    
        if (!exponentialFunctionRefs) exponentialFunctionRefs = new HashMap<IPhoneGradientRef, CGFunctionRef>;
        else function = exponentialFunctionRefs->get(aGradient);
        
        if (!function) {
            static struct CGFunctionCallbacks exponentialFunctionCallbacks =  { 0, InterpolateExponentialGradient, NULL };
        
            exponentialFunctionRefs->set(aGradient, function = CGFunctionCreate(aGradient, 1, NULL, 4, NULL, &exponentialFunctionCallbacks));
        }
    
        return function;
    }
}

static CGColorSpaceRef GetSharedColorSpace()
{
    CGColorSpaceRef sharedColorSpace = NULL;
    
    if (!sharedColorSpace) sharedColorSpace = CGColorSpaceCreateDeviceRGB();
    
    return sharedColorSpace;
}

void GraphicsContext::drawAxialGradient(IPhoneGradientRef aGradient, FloatPoint startPoint, FloatPoint stopPoint, Interpolation anInterpolation)
{
    CGShadingRef shading = CGShadingCreateAxial(GetSharedColorSpace(), startPoint, stopPoint, GetSharedFunctionRef(aGradient, anInterpolation), false, false);
    
    CGContextDrawShading(platformContext(), shading);
 
    CFRelease(shading);
}

void GraphicsContext::drawRadialGradient(IPhoneGradientRef aGradient, FloatPoint startPoint, float startRadius, FloatPoint stopPoint, float stopRadius, Interpolation anInterpolation)
{
    CGShadingRef shading = CGShadingCreateRadial(GetSharedColorSpace(), startPoint, startRadius, stopPoint, stopRadius, GetSharedFunctionRef(aGradient, anInterpolation), false, false);
    
    CGContextDrawShading(platformContext(), shading);  
    
    CFRelease(shading);
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect)
{
    // It is not enough just to round to pixels in device space. The rotation part of the 
    // affine transform matrix to device space can mess with this conversion if we have a
    // rotating image like the hands of the world clock widget. We just need the scale, so 
    // we get the affine transform matrix and extract the scale.

    if (m_data->m_userToDeviceTransformKnownToBeIdentity)
        return rect;

    CGAffineTransform deviceMatrix = CGContextGetUserSpaceToDeviceSpaceTransform(platformContext());
    if (CGAffineTransformIsIdentity(deviceMatrix)) {
        m_data->m_userToDeviceTransformKnownToBeIdentity = true;
        return rect;
    }

    float deviceScaleX = sqrtf(deviceMatrix.a * deviceMatrix.a + deviceMatrix.b * deviceMatrix.b);
    float deviceScaleY = sqrtf(deviceMatrix.c * deviceMatrix.c + deviceMatrix.d * deviceMatrix.d);

    CGPoint deviceOrigin = CGPointMake(rect.x() * deviceScaleX, rect.y() * deviceScaleY);
    CGPoint deviceLowerRight = CGPointMake((rect.x() + rect.width()) * deviceScaleX,
        (rect.y() + rect.height()) * deviceScaleY);

    deviceOrigin.x = roundf(deviceOrigin.x);
    deviceOrigin.y = roundf(deviceOrigin.y);
    deviceLowerRight.x = roundf(deviceLowerRight.x);
    deviceLowerRight.y = roundf(deviceLowerRight.y);
    
    // Don't let the height or width round to 0 unless either was originally 0
    if (deviceOrigin.y == deviceLowerRight.y && rect.height() != 0)
        deviceLowerRight.y += 1;
    if (deviceOrigin.x == deviceLowerRight.x && rect.width() != 0)
        deviceLowerRight.x += 1;

    FloatPoint roundedOrigin = FloatPoint(deviceOrigin.x / deviceScaleX, deviceOrigin.y / deviceScaleY);
    FloatPoint roundedLowerRight = FloatPoint(deviceLowerRight.x / deviceScaleX, deviceLowerRight.y / deviceScaleY);
    return FloatRect(roundedOrigin, roundedLowerRight - roundedOrigin);
}

static CGPoint roundPointToDevicePixels(CGContextRef context, CGPoint p, int offset)
{
    // Extract the x-scaling from the device space matrix. Since we always
    // scale the page uniformly, this scale is sufficient to calculate
    // the right values for adjusting the point to device space.
    CGAffineTransform deviceMatrix = CGContextGetUserSpaceToDeviceSpaceTransform(context);
    float s = deviceMatrix.a;
    float x = roundf(p.x * s);
    float y = ceilf((p.y * s) + (offset * s));
    
    // don't offset line from bottom of text if scale is less than OffsetUnderlineScale
    static const float OffsetUnderlineScale = 0.4f;
    float dy = s < OffsetUnderlineScale ? 0 : 1;
    
    return CGPointMake(x / s, (y + dy) / s);
}

void GraphicsContext::drawLineForText(const IntPoint& point, int width, bool printing)
{
    if (paintingDisabled())
        return;

    if (width <= 0)
        return;

    UNUSED_PARAM(printing);

    CGContextRef context = platformContext();

    CGContextSaveGState(context);

    CGContextSetLineWidth(context, 1);
    CGContextSetShouldAntialias(context, false);

    // This code always draws a one-pixel line, which tends to visually
    // overwhelm text at small scales. To counter this effect, an alpha
    // is applied to the underline color. 
    CGAffineTransform t = CGContextGetCTM(context);
    float scale = t.a;
    Color color(strokeColor());
    if (scale < 1.0) {
        static const float MinUnderlineAlpha = 0.4f;
        float shade = scale > MinUnderlineAlpha ? scale : MinUnderlineAlpha;
        int alpha = color.alpha() * shade;
        color = Color(color.red(), color.green(), color.blue(), alpha);
    }
    setCGFillColor(context, color);

    CGPoint origin = roundPointToDevicePixels(context, CGPointMake(point.x(), point.y()), 0);
    CGRect rect = CGRectMake(origin.x, origin.y, width, 1); // always draw with 1 thickness
    CGContextFillRect(context, rect);

    CGContextRestoreGState(context);
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    UNUSED_PARAM(link);
    UNUSED_PARAM(destRect);
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality mode)
{
    if (paintingDisabled())
        return;
    
    CGInterpolationQuality quality = kCGInterpolationDefault;
    switch (mode) {
        case InterpolationDefault:
            quality = kCGInterpolationDefault;
            break;
        case InterpolationNone:
            quality = kCGInterpolationNone;
            break;
        case InterpolationLow:
            quality = kCGInterpolationLow;
            break;

        // Fall through to InterpolationHigh if kCGInterpolationMedium is not available
        case InterpolationMedium:
#if HAVE(CG_INTERPOLATION_MEDIUM)
            quality = kCGInterpolationMedium;
            break;
#endif
        case InterpolationHigh:
            quality = kCGInterpolationHigh;
            break;
    }
    CGContextSetInterpolationQuality(platformContext(), quality);
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    if (paintingDisabled())
        return InterpolationDefault;

    CGInterpolationQuality quality = CGContextGetInterpolationQuality(platformContext());
    switch (quality) {
        case kCGInterpolationDefault:
            return InterpolationDefault;
        case kCGInterpolationNone:
            return InterpolationNone;
        case kCGInterpolationLow:
            return InterpolationLow;
#if HAVE(CG_INTERPOLATION_MEDIUM)
        case kCGInterpolationMedium:
            return InterpolationMedium;
#endif
        case kCGInterpolationHigh:
            return InterpolationHigh;
    }
    return InterpolationDefault;
}

void GraphicsContext::setPlatformTextDrawingMode(int mode)
{
    if (paintingDisabled())
        return;

    // Wow, wish CG had used bits here.
    CGContextRef context = platformContext();
    switch (mode) {
        case cTextInvisible: // Invisible
            CGContextSetTextDrawingMode(context, kCGTextInvisible);
            break;
        case cTextFill: // Fill
            CGContextSetTextDrawingMode(context, kCGTextFill);
            break;
        case cTextStroke: // Stroke
            CGContextSetTextDrawingMode(context, kCGTextStroke);
            break;
        case 3: // Fill | Stroke
            CGContextSetTextDrawingMode(context, kCGTextFillStroke);
            break;
        case cTextClip: // Clip
            CGContextSetTextDrawingMode(context, kCGTextClip);
            break;
        case 5: // Fill | Clip
            CGContextSetTextDrawingMode(context, kCGTextFillClip);
            break;
        case 6: // Stroke | Clip
            CGContextSetTextDrawingMode(context, kCGTextStrokeClip);
            break;
        case 7: // Fill | Stroke | Clip
            CGContextSetTextDrawingMode(context, kCGTextFillStrokeClip);
            break;
        default:
            break;
    }
}

void GraphicsContext::setPlatformStrokeColor(const Color& color)
{
    if (paintingDisabled())
        return;
    setCGStrokeColor(platformContext(), color);
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    if (paintingDisabled())
        return;
    CGContextSetLineWidth(platformContext(), thickness);
}

void GraphicsContext::setPlatformFillColor(const Color& color)
{
    if (paintingDisabled())
        return;
    setCGFillColor(platformContext(), color);
}

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;
    CGContextSetShouldAntialias(platformContext(), enable);
}

#ifndef BUILDING_ON_TIGER // Tiger's setCompositeOperation() is defined in GraphicsContextMac.mm.
void GraphicsContext::setCompositeOperation(CompositeOperator mode)
{   
    if (paintingDisabled())
        return;

    CGBlendMode target = kCGBlendModeNormal;   
    switch (mode) {
        case CompositeClear:
            target = kCGBlendModeClear;
            break;
        case CompositeCopy:
            target = kCGBlendModeCopy;
            break;
        case CompositeSourceOver:
            //kCGBlendModeNormal
            break;
        case CompositeSourceIn:
            target = kCGBlendModeSourceIn;
            break;
        case CompositeSourceOut:
            target = kCGBlendModeSourceOut;
            break;
        case CompositeSourceAtop:
            target = kCGBlendModeSourceAtop; 
            break;
        case CompositeDestinationOver:
            target = kCGBlendModeDestinationOver;
            break;
        case CompositeDestinationIn:
            target = kCGBlendModeDestinationIn;
            break;
        case CompositeDestinationOut:
            target = kCGBlendModeDestinationOut;
            break;
        case CompositeDestinationAtop:
            target = kCGBlendModeDestinationAtop;
            break;
        case CompositeXOR:
            target = kCGBlendModeXOR;
            break;
        case CompositePlusDarker:
            target = kCGBlendModePlusDarker;
            break;
        case CompositeHighlight:
            // currently unsupported
            break;
        case CompositePlusLighter:
            target = kCGBlendModePlusLighter;
            break;
    }
    CGContextSetBlendMode(platformContext(), target);
}
#endif

}

