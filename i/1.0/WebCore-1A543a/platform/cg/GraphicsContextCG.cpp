/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "GraphicsContext.h"

#include "Path.h"

using namespace std;

namespace WebCore {

// NSColor, NSBezierPath, and NSGraphicsContext
// calls in this file are all exception-safe, so we don't block
// exceptions for those.

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

void GraphicsContext::savePlatformState()
{
    CGContextSaveGState(platformContext());
}

void GraphicsContext::restorePlatformState()
{
    CGContextRestoreGState(platformContext());
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    CGContextRef context = platformContext();

    if (fillColor().alpha()) {
        setCGFillColor(context, fillColor());
        CGContextFillRect(context, rect);
    }

    if (pen().style() != Pen::Pen::NoPen) {
        setCGFillColor(context, pen().color());
        
        GraphicsContext p(context);
        CGRect ir= p.roundToDevicePixels(FloatRect(rect));
        
        CGRect rects[4] = {
            CGRectMake(ir.origin.x, ir.origin.y, ir.size.width, 1),
            CGRectMake(ir.origin.x, ir.origin.y+ir.size.height - 1, ir.size.width, 1),
            CGRectMake(ir.origin.x, ir.origin.y + 1, 1, ir.size.height - 2),
            CGRectMake(ir.origin.x+ir.size.width - 1, ir.origin.y + 1, 1, ir.size.height - 2)
        };
        CGContextFillRects(context, rects, 4);
    }
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2, bool antialias)
{
    if (paintingDisabled())
        return;

    Pen::PenStyle penStyle = pen().style();
    if (penStyle == Pen::Pen::NoPen)
        return;
    float width = pen().width();
    if (width < 1)
        width = 1;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());
    
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == Pen::DotLine || penStyle == Pen::DashLine) {
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
            p1.move(0.5, 0);
            p2.move(0.5, 0);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.move(0, 0.5);
            p2.move(0, 0.5);
        }
    }
    
    int patWidth = 0;
    switch (penStyle) {
        case Pen::NoPen:
        case Pen::SolidLine:
            break;
        case Pen::DotLine:
            patWidth = (int)width;
            break;
        case Pen::DashLine:
            patWidth = 3 * (int)width;
            break;
    }

    CGContextRef context = platformContext();

    CGContextSaveGState(context);

    setCGStrokeColor(context, pen().color());
    CGContextSetShouldAntialias(context, antialias);
    if (patWidth) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        setCGFillColor(context, pen().color());
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

        float patternOffset = 0;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0;
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
    
    CGContextSetLineWidth(context, width);

    CGContextBeginPath(context);
    CGContextMoveToPoint(context, p1.x(), p1.y());
    CGContextAddLineToPoint(context, p2.x(), p2.y());

    CGContextStrokePath(context);

    CGContextRestoreGState(context);
}


void GraphicsContext::drawJoinedLines(CGPoint points [], unsigned count, bool antialias, CGLineCap lineCap)
{
    if (paintingDisabled() || !count)
        return;

    float width = pen().width();
    CGContextRef context = platformContext();

    CGContextSaveGState(context);

    setCGStrokeColor(context, pen().color());
    
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
    CGContextAddArc(context, rect.x() + r, rect.y() + r, r, 0, 2*M_PI, true);
    CGContextClosePath(context);

    if (fillColor().alpha()) {
        setCGFillColor(context, fillColor());
        if (pen().style() != Pen::NoPen) {
            // stroke and fill
            setCGStrokeColor(context, pen().color());
            unsigned penWidth = pen().width();
            if (penWidth == 0) 
                penWidth++;
            CGContextSetLineWidth(context, penWidth);
            CGContextDrawPath(context, kCGPathFillStroke);
        } else
            CGContextFillPath(context);
    } else if (pen().style() != Pen::NoPen) {
        setCGStrokeColor(context, pen().color());
        unsigned penWidth = pen().width();
        if (penWidth == 0) 
            penWidth++;
        CGContextSetLineWidth(context, penWidth);
        CGContextStrokePath(context);
    }
}

void GraphicsContext::drawEllipse(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    CGContextRef context(platformContext());

    CGContextSaveGState(context);

    setCGFillColor(context, fillColor());
    setCGStrokeColor(context, pen().color());

    CGContextSetLineWidth(context, pen().width());
    
    CGContextBeginPath(context);
    CGContextAddEllipseInRect(context, rect);

    CGContextFillPath(context);
    CGContextStrokePath(context);
    
    CGContextRestoreGState(context);
}

void GraphicsContext::drawArc(const IntRect& rect, float thickness, int startAngle, int angleSpan)
{ 
    if (paintingDisabled())
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
    float start = -fa * M_PI/180;
    float end = -falen * M_PI/180;
    CGContextAddArc(context, x + hRadius, (y + vRadius) * reverseScaleFactor, hRadius, start, end, true);

    if (w != h)
        scale(FloatSize(1, reverseScaleFactor));
    
    if (pen().style() == Pen::NoPen) {
        setCGStrokeColor(context, fillColor());
        CGContextSetLineWidth(context, thickness);
        CGContextStrokePath(context);
    } else {
        Pen::PenStyle penStyle = pen().style();
        float width = pen().width();
        if (width < 1)
            width = 1;
        int patWidth = 0;
        
        switch (penStyle) {
            case Pen::NoPen:
            case Pen::SolidLine:
                break;
            case Pen::DotLine:
                patWidth = (int)(width / 2);
                break;
            case Pen::DashLine:
                patWidth = 3 * (int)(width / 2);
                break;
        }

        CGContextSaveGState(context);
        setCGStrokeColor(context, pen().color());

        if (patWidth) {
            // Example: 80 pixels with a width of 30 pixels.
            // Remainder is 20.  The maximum pixels of line we could paint
            // will be 50 pixels.
            int distance;
            if (hRadius == vRadius)
                distance = (int)(M_PI * hRadius) / 2;
            else // We are elliptical and will have to estimate the distance
                distance = (int)(M_PI * sqrt((hRadius * hRadius + vRadius * vRadius) / 2)) / 2;
            
            int remainder = distance % patWidth;
            int coverage = distance - remainder;
            int numSegments = coverage / patWidth;

            float patternOffset = 0;
            // Special case 1px dotted borders for speed.
            if (patWidth == 1)
                patternOffset = 1.0;
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
                        patternOffset = (patWidth - remainder) / 2;
                }
            }
        
            const CGFloat dottedLine[2] = { patWidth, patWidth };
            CGContextSetLineDash(context, patternOffset, dottedLine, 2);
        }
    
        CGContextSetLineWidth(context, width);
        CGContextStrokePath(context);
        CGContextRestoreGState(context);
    }
    
    CGContextRestoreGState(context);
}

void GraphicsContext::drawConvexPolygon(size_t npoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (npoints <= 1)
        return;

    CGContextRef context = platformContext();

    CGContextSaveGState(context);

    CGContextSetShouldAntialias(context, shouldAntialias);
    
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, points[0].x(), points[0].y());
    for (size_t i = 1; i < npoints; i++)
        CGContextAddLineToPoint(context, points[i].x(), points[i].y());
    CGContextClosePath(context);

    if (fillColor().alpha()) {
        setCGFillColor(context, fillColor());
        CGContextEOFillPath(context);
    }

    if (pen().style() != Pen::NoPen) {
        setCGStrokeColor(context, pen().color());
        CGContextSetLineWidth(context, pen().width());
        CGContextStrokePath(context);
    }

    CGContextRestoreGState(context);
}

void GraphicsContext::fillRect(const IntRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;
    if (color.alpha()) {
        CGContextRef context = platformContext();
        setCGFillColor(context, color);
        CGContextFillRect(context, rect);
    }
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;
    if (color.alpha()) {
        CGContextRef context = platformContext();
        setCGFillColor(context, color);
        CGContextFillRect(context, rect);
    }
}

void GraphicsContext::addClip(const IntRect& rect)
{
    if (paintingDisabled())
        return;
    CGContextClipToRect(platformContext(), rect);
}

void GraphicsContext::addRoundedRectClip(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
    const IntSize& bottomLeft, const IntSize& bottomRight)
{
    if (paintingDisabled())
        return;

    // Need sufficient width and height to contain these curves.  Sanity check our top/bottom
    // values and our width/height values to make sure the curves can all fit.
    int requiredWidth = max(topLeft.width() + topRight.width(), bottomLeft.width() + bottomRight.width());
    if (requiredWidth > rect.width())
        return;
    int requiredHeight = max(topLeft.height() + bottomLeft.height(), topRight.height() + bottomRight.height());
    if (requiredHeight > rect.height())
        return;
 
    // Clip to our rect.
    addClip(rect);

    // OK, the curves can fit.
    CGContextRef context = platformContext();
    
    // Add the four ellipses to the path.  Technically this really isn't good enough, since we could end up
    // not clipping the other 3/4 of the ellipse we don't care about.  We're relying on the fact that for
    // normal use cases these ellipses won't overlap one another (or when they do the curvature of one will
    // be subsumed by the other).
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.y(), topLeft.width() * 2, topLeft.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.right() - topRight.width() * 2, rect.y(),
                                                  topRight.width() * 2, topRight.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.bottom() - bottomLeft.height() * 2,
                                                  bottomLeft.width() * 2, bottomLeft.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.right() - bottomRight.width() * 2,
                                                  rect.bottom() - bottomRight.height() * 2,
                                                  bottomRight.width() * 2, bottomRight.height() * 2));
    
    // Now add five rects (one for each edge rect in between the rounded corners and one for the interior).
    CGContextAddRect(context, CGRectMake(rect.x() + topLeft.width(), rect.y(),
                                         rect.width() - topLeft.width() - topRight.width(),
                                         max(topLeft.height(), topRight.height())));
    CGContextAddRect(context, CGRectMake(rect.x() + bottomLeft.width(), 
                                         rect.bottom() - max(bottomLeft.height(), bottomRight.height()),
                                         rect.width() - bottomLeft.width() - bottomRight.width(),
                                         max(bottomLeft.height(), bottomRight.height())));
    CGContextAddRect(context, CGRectMake(rect.x(), rect.y() + topLeft.height(),
                                         max(topLeft.width(), bottomLeft.width()), rect.height() - topLeft.height() - bottomLeft.height()));
    CGContextAddRect(context, CGRectMake(rect.right() - max(topRight.width(), bottomRight.width()),
                                         rect.y() + topRight.height(),
                                         max(topRight.width(), bottomRight.width()), rect.height() - topRight.height() - bottomRight.height()));
    CGContextAddRect(context, CGRectMake(rect.x() + max(topLeft.width(), bottomLeft.width()),
                                         rect.y() + max(topLeft.height(), topRight.height()),
                                         rect.width() - max(topLeft.width(), bottomLeft.width()) - max(topRight.width(), bottomRight.width()),
                                         rect.height() - max(topLeft.height(), topRight.height()) - max(bottomLeft.height(), bottomRight.height())));
    CGContextClip(context);
}


void GraphicsContext::addRoundedRectClip(const FloatRect& rect, const FloatSize& topLeft, const FloatSize& topRight,
    const FloatSize& bottomLeft, const FloatSize& bottomRight)
{
    if (paintingDisabled())
        return;

    // Need sufficient width and height to contain these curves.  Sanity check our top/bottom
    // values and our width/height values to make sure the curves can all fit.
    int requiredWidth = max(topLeft.width() + topRight.width(), bottomLeft.width() + bottomRight.width());
    if (requiredWidth > rect.width())
        return;
    int requiredHeight = max(topLeft.height() + bottomLeft.height(), topRight.height() + bottomRight.height());
    if (requiredHeight > rect.height())
        return;
 
    // Clip to our rect.
    clip(rect);

    // OK, the curves can fit.
    CGContextRef context = platformContext();
    
    // Add the four ellipses to the path.  Technically this really isn't good enough, since we could end up
    // not clipping the other 3/4 of the ellipse we don't care about.  We're relying on the fact that for
    // normal use cases these ellipses won't overlap one another (or when they do the curvature of one will
    // be subsumed by the other).
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.y(), topLeft.width() * 2, topLeft.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.right() - topRight.width() * 2, rect.y(),
                                                  topRight.width() * 2, topRight.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.bottom() - bottomLeft.height() * 2,
                                                  bottomLeft.width() * 2, bottomLeft.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.right() - bottomRight.width() * 2,
                                                  rect.bottom() - bottomRight.height() * 2,
                                                  bottomRight.width() * 2, bottomRight.height() * 2));
    
    // Now add five rects (one for each edge rect in between the rounded corners and one for the interior).
    CGContextAddRect(context, CGRectMake(rect.x() + topLeft.width(), rect.y(),
                                         rect.width() - topLeft.width() - topRight.width(),
                                         max(topLeft.height(), topRight.height())));
    CGContextAddRect(context, CGRectMake(rect.x() + bottomLeft.width(), 
                                         rect.bottom() - max(bottomLeft.height(), bottomRight.height()),
                                         rect.width() - bottomLeft.width() - bottomRight.width(),
                                         max(bottomLeft.height(), bottomRight.height())));
    CGContextAddRect(context, CGRectMake(rect.x(), rect.y() + topLeft.height(),
                                         max(topLeft.width(), bottomLeft.width()), rect.height() - topLeft.height() - bottomLeft.height()));
    CGContextAddRect(context, CGRectMake(rect.right() - max(topRight.width(), bottomRight.width()),
                                         rect.y() + topRight.height(),
                                         max(topRight.width(), bottomRight.width()), rect.height() - topRight.height() - bottomRight.height()));
    CGContextAddRect(context, CGRectMake(rect.x() + max(topLeft.width(), bottomLeft.width()),
                                         rect.y() + max(topLeft.height(), topRight.height()),
                                         rect.width() - max(topLeft.width(), bottomLeft.width()) - max(topRight.width(), bottomRight.width()),
                                         rect.height() - max(topLeft.height(), topRight.height()) - max(bottomLeft.height(), bottomRight.height())));
    CGContextClip(context);
}


void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;

    addClip(rect);
    CGContextRef context = platformContext();
    
    // Add outer ellipse
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.y(), rect.width(), rect.height()));
    // Add inner ellipse.
    CGContextAddEllipseInRect(context, CGRectMake(rect.x() + thickness, rect.y() + thickness,
        rect.width() - (thickness * 2), rect.height() - (thickness * 2)));
    
    CGContextEOClip(context);
}

#if SVG_SUPPORT
KRenderingDeviceContext* GraphicsContext::createRenderingDeviceContext()
{
    return new KRenderingDeviceContextQuartz(platformContext());
}
#endif

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextSaveGState(context);
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
}

void GraphicsContext::setShadow(const IntSize& size, int blur, const Color& color)
{
    if (paintingDisabled())
        return;
    // Check for an invalid color, as this means that the color was not set for the shadow
    // and we should therefore just use the default shadow color.
    CGContextRef context = platformContext();
    if (!color.isValid())
        CGContextSetShadow(context, CGSizeMake(size.width(), -size.height()), blur); // y is flipped.
    else {
        CGColorRef colorCG = cgColor(color);
        CGContextSetShadowWithColor(context,
                                    CGSizeMake(size.width(), -size.height()), // y is flipped.
                                    blur, 
                                    colorCG);
        CGColorRelease(colorCG);
    }
}

void GraphicsContext::clearShadow()
{
    if (paintingDisabled())
        return;
    CGContextSetShadowWithColor(platformContext(), CGSizeZero, 0, 0);
}

void GraphicsContext::setLineWidth(float width)
{
    if (paintingDisabled())
        return;
    CGContextSetLineWidth(platformContext(), width);
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
    CGContextStrokeRectWithWidth(platformContext(), r, lineWidth);
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

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextBeginPath(context);
    CGContextAddPath(context, path.platformPath());
    CGContextClip(context);
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;
    CGContextClipToRect(platformContext(), rect);
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    CGContextScaleCTM(platformContext(), size.width(), size.height());
}

void GraphicsContext::rotate(float angle)
{
    if (paintingDisabled())
        return;
    CGContextRotateCTM(platformContext(), angle);
}

void GraphicsContext::translate(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    CGContextTranslateCTM(platformContext(), size.width(), size.height());
}


static void InterpolateLinearGradient(void *info, const CGFloat *inData, CGFloat *outData)
{
    float   alpha = inData[0],
            inverse = 1.0 - alpha;

    outData[0] = inverse * static_cast<GradientRef>(info)->start[0] + alpha * static_cast<GradientRef>(info)->end[0];
    outData[1] = inverse * static_cast<GradientRef>(info)->start[1] + alpha * static_cast<GradientRef>(info)->end[1];
    outData[2] = inverse * static_cast<GradientRef>(info)->start[2] + alpha * static_cast<GradientRef>(info)->end[2];
    outData[3] = inverse * static_cast<GradientRef>(info)->start[3] + alpha * static_cast<GradientRef>(info)->end[3];
}

static void InterpolateExponentialGradient(void *info, const CGFloat *inData, CGFloat *outData)
{
    int i = 0;
    float a = inData[0];
    
    for(; i < 4; ++i) {
        float   end = log(MAX(static_cast<GradientRef>(info)->end[i], 0.01)),
                start = log(MAX(static_cast<GradientRef>(info)->start[i], 0.01));
                
        outData[i] = exp(start - (end + start) * a);
    }
}

static CGFunctionRef GetSharedFunctionRef(GradientRef aGradient, Interpolation anInterpolation)
{
    CGFunctionRef function = NULL;
        
    static HashMap<GradientRef, CGFunctionRef> * linearFunctionRefs = NULL;
    static HashMap<GradientRef, CGFunctionRef> * exponentialFunctionRefs = NULL;
    
    if (anInterpolation == LinearInterpolation) {
    
        if (!linearFunctionRefs) linearFunctionRefs = new HashMap<GradientRef, CGFunctionRef>;
        else function = linearFunctionRefs->get(aGradient);
    
        if (!function) {
            static struct CGFunctionCallbacks linearFunctionCallbacks =  { 0, InterpolateLinearGradient, NULL };
        
            linearFunctionRefs->set(aGradient, function = CGFunctionCreate(aGradient, 1, NULL, 4, NULL, &linearFunctionCallbacks));
        }
    
        return function;
    }
    else {
    
        if (!exponentialFunctionRefs) exponentialFunctionRefs = new HashMap<GradientRef, CGFunctionRef>;
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

void GraphicsContext::drawAxialGradient(GradientRef aGradient, FloatPoint startPoint, FloatPoint stopPoint, Interpolation anInterpolation)
{
    CGShadingRef shading = CGShadingCreateAxial(GetSharedColorSpace(), startPoint, stopPoint, GetSharedFunctionRef(aGradient, anInterpolation), false, false);
    
    CGContextDrawShading(platformContext(), shading);
 
    CFRelease(shading);
}

void GraphicsContext::drawRadialGradient(GradientRef aGradient, FloatPoint startPoint, float startRadius, FloatPoint stopPoint, float stopRadius, Interpolation anInterpolation)
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
    CGAffineTransform deviceMatrix = CGContextGetUserSpaceToDeviceSpaceTransform(platformContext());
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
    static const float OffsetUnderlineScale = 0.4;
    float dy = s < OffsetUnderlineScale ? 0 : 1;
    
    return CGPointMake(x / s, (y + dy) / s);
}

void GraphicsContext::drawLineForText(const IntPoint& point, int yOffset, int width, bool printing)
{
    if (paintingDisabled())
        return;
    
    // Note: This function assumes that point.x and point.y are integers (and that's currently always the case).
    float x = point.x();
    float y = point.y() + yOffset;

    // Leave 1.0 in user space between the baseline of the text and the top of the underline.
    // FIXME: Is this the right distance for space above the underline? Even for thick underlines on large sized text?
    y += 1;

    float thickness = pen().width();
    if (printing) {
        // When printing, use a minimum thickness of 0.5 in user space.
        // See bugzilla bug 4255 for details of why 0.5 is the right minimum thickness to use while printing.
        if (thickness < 0.5)
            thickness = 0.5;

        // When printing, use antialiasing instead of putting things on integral pixel boundaries.
    } else {
    }


    CGContextRef context = platformContext();

    CGContextSaveGState(context);

    CGContextSetLineWidth(context, 1);
    CGContextSetShouldAntialias(context, 0);

    // This code always draws a one-pixel line, which tends to visually
    // overwhelm text at small scales. To counter this effect, an alpha
    // is applied to the underline color. 
    CGAffineTransform t = CGContextGetCTM(context);
    float scale = t.a;
    Color color(pen().color());
    if (scale < 1.0) {
        static const float MinUnderlineAlpha = 0.4;
        float shade = scale > MinUnderlineAlpha ? scale : MinUnderlineAlpha;
        int alpha = color.alpha() * shade;
        color = Color(color.red(), color.green(), color.blue(), alpha);
    }
    setCGFillColor(context, color);

    CGPoint origin = roundPointToDevicePixels(context, CGPointMake(x, point.y()), yOffset);
    CGRect rect = CGRectMake(origin.x, origin.y, width, 1); // always draw with 1 thickness
    CGContextFillRect(context, rect);

    CGContextRestoreGState(context);



}

}
