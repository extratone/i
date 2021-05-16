/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "BitmapImage.h"

#if PLATFORM(CG)

#include "TransformationMatrix.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "PDFDocumentImage.h"
#include "PlatformString.h"
#include <CoreGraphics/CoreGraphics.h>

#if PLATFORM(MAC) || PLATFORM(CHROMIUM)
#include "WebCoreSystemInterface.h"
#endif

#if PLATFORM(WIN)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

namespace WebCore {

bool FrameData::clear(bool clearMetadata)
{
    if (clearMetadata)
        m_haveMetadata = false;

#if ENABLE(RESPECT_EXIF_ORIENTATION)
    m_orientation = 0;
#endif
    m_bytes = 0;
    m_scale = 1.0f;
    m_haveInfo = false;

    if (m_frame) {
        CGImageRelease(m_frame);
        m_frame = 0;
        return true;
    }
    return false;
}

// ================================================
// Image Class
// ================================================

BitmapImage::BitmapImage(CGImageRef cgImage, ImageObserver* observer)
    : Image(observer)
    , m_currentFrame(0)
    , m_frames(0)
    , m_frameTimer(0)
    , m_repetitionCount(cAnimationNone)
    , m_repetitionCountStatus(Unknown)
    , m_repetitionsComplete(0)
    , m_isSolidColor(false)
    , m_animationFinished(true)
    , m_allDataReceived(true)
    , m_haveSize(true)
    , m_sizeAvailable(true)
    , m_decodedSize(0)
    , m_haveFrameCount(true)
    , m_frameCount(1)
{
    initPlatformData();
    
    CGFloat width = CGImageGetWidth(cgImage);
    CGFloat height = CGImageGetHeight(cgImage);
    m_decodedSize = width * height * 4;
    m_size = IntSize(width, height);

    m_frames.grow(1);
    m_frames[0].m_frame = cgImage;
    m_frames[0].m_hasAlpha = true;
    m_frames[0].m_haveMetadata = true;
    m_frames[0].m_scale = 1.0f;
    checkForSolidColor();
}

// Drawing Routines

void BitmapImage::checkForSolidColor()
{
    if (frameCount() > 1)
        m_isSolidColor = false;
    else {
        // checkForSolidColor() is called from frameAtIndex() and recursing back would mess up the decoded size count.
        if (!m_frames.size())
            return;
        CGImageRef image = m_frames[0].m_frame;

        // Currently we only check for solid color in the important special case of a 1x1 image.
        if (image && CGImageGetWidth(image) == 1 && CGImageGetHeight(image) == 1) {
            // <rdar://problem/5106514> Floating-point contexts are not supported on iPhone
            unsigned char pixel[4] = {0, 0, 0, 0}; // RGBA
            CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
            CGContextRef bmap = CGBitmapContextCreate(pixel, 1, 1, 8, sizeof(pixel), space, kCGImageAlphaPremultipliedLast);
            if (bmap) {
                GraphicsContext(bmap).setCompositeOperation(CompositeCopy);
                CGRect dst = { {0, 0}, {1, 1} };
                CGContextDrawImage(bmap, dst, image);
                if (pixel[3] == 0)
                    m_solidColor = Color(0, 0, 0, 0);
                else
                    m_solidColor = Color((int)pixel[0], (int)pixel[1], (int)pixel[2], (int)pixel[3]);
                m_isSolidColor = true;
                CFRelease(bmap);
            } 
            CFRelease(space);
        }
    }
}

CGImageRef BitmapImage::getCGImageRef()
{
    return frameAtIndex(0);
}

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator compositeOp)
{
    startAnimation(false);
    
    CGRect transformedDstRect = CGRectApplyAffineTransform(destRect, CGContextGetCTM(ctxt->platformContext()));
    CGImageRef image = frameAtIndex(m_currentFrame, std::min(1.0f, std::max(transformedDstRect.size.width  / srcRect.width(),
                                                                            transformedDstRect.size.height / srcRect.height())));
    if (!image) // If it's too early we won't have an image yet.
        return;
    
    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, destRect, solidColor(), compositeOp);
        return;
    }

    float currHeight = CGImageGetHeight(image);
#if ENABLE(RESPECT_EXIF_ORIENTATION)
    int exifOrientation = frameOrientationAtIndex(m_currentFrame);
    if (exifOrientation && !(exifOrientation & 0x01))
        currHeight = CGImageGetWidth(image);
#endif
    // Unapply the scaling since we are getting this from a scaled bitmap.
    currHeight /= m_frames[m_currentFrame].m_scale;
    if (currHeight <= srcRect.y())
        return;

    CGContextRef context = ctxt->platformContext();
    ctxt->save();

    // Anti-aliasing is on by default on the iPhone. Need to turn it off when drawing images.
    CGContextSetShouldAntialias(context, false);
    
    bool shouldUseSubimage = false;

    // If the source rect is a subportion of the image, then we compute an inflated destination rect that will hold the entire image
    // and then set a clip to the portion that we want to display.
    FloatRect adjustedDestRect = destRect;
    FloatSize selfSize = currentFrameSize();
    if (srcRect.size() != selfSize) {
        CGInterpolationQuality interpolationQuality = CGContextGetInterpolationQuality(context);
        // When the image is scaled using high-quality interpolation, we create a temporary CGImage
        // containing only the portion we want to display. We need to do this because high-quality
        // interpolation smoothes sharp edges, causing pixels from outside the source rect to bleed
        // into the destination rect. See <rdar://problem/6112909>.
        shouldUseSubimage = (interpolationQuality == kCGInterpolationHigh || interpolationQuality == kCGInterpolationDefault) && srcRect.size() != destRect.size();
        if (shouldUseSubimage) {
            image = CGImageCreateWithImageInRect(image, srcRect);
            if (currHeight < srcRect.bottom()) {
                ASSERT(CGImageGetHeight(image) == currHeight - CGRectIntegral(srcRect).origin.y);
                adjustedDestRect.setHeight(destRect.height() / srcRect.height() * CGImageGetHeight(image));
            }
        } else {
            float xScale = srcRect.width() / destRect.width();
            float yScale = srcRect.height() / destRect.height();

            adjustedDestRect.setLocation(FloatPoint(destRect.x() - srcRect.x() / xScale, destRect.y() - srcRect.y() / yScale));
            adjustedDestRect.setSize(FloatSize(selfSize.width() / xScale, selfSize.height() / yScale));

            CGContextClipToRect(context, destRect);
        }
    }

    // If the image is only partially loaded, then shrink the destination rect that we're drawing into accordingly.
    if (!shouldUseSubimage && currHeight < selfSize.height())
        adjustedDestRect.setHeight(adjustedDestRect.height() * currHeight / selfSize.height());
    
    // Align to pixel boundaries
    adjustedDestRect = ctxt->roundToDevicePixels(adjustedDestRect);

#if ENABLE(RESPECT_EXIF_ORIENTATION)
    // Rotate the image based on the orientation
    float w = adjustedDestRect.width();
    float h = adjustedDestRect.height();
    CGAffineTransform transform;
    switch (exifOrientation) {
        case BitmapImage::ImageEXIFOrientationBottomRight:
            transform = CGAffineTransformMake(-1,  0,  0, -1,  w, h); 
            break;
        case BitmapImage::ImageEXIFOrientationLeftBottom:
            transform = CGAffineTransformMake( 0,  1, -1,  0,  w, 0);
            break;
        case BitmapImage::ImageEXIFOrientationRightTop:
            transform = CGAffineTransformMake( 0, -1,  1,  0,  0, h); 
            break;
        case BitmapImage::ImageEXIFOrientationTopRight:
            transform = CGAffineTransformMake(-1,  0,  0,  1,  w, 0); 
            break;
        case BitmapImage::ImageEXIFOrientationBottomLeft:
            transform = CGAffineTransformMake( 1,  0,  0, -1,  0, h); 
            break;
        case BitmapImage::ImageEXIFOrientationLeftTop:
            transform = CGAffineTransformMake( 0, -1, -1,  0,  w, h);
            break;
        case BitmapImage::ImageEXIFOrientationRightBottom:
            transform = CGAffineTransformMake( 0,  1,  1,  0,  0, 0); 
            break;
        default: 
            transform = CGAffineTransformIdentity;
            break;
    }
#endif

    // Flip the coords.
    ctxt->setCompositeOperation(compositeOp);
    CGContextTranslateCTM(context, adjustedDestRect.x(), adjustedDestRect.bottom());
    CGContextScaleCTM(context, 1, -1);
#if ENABLE(RESPECT_EXIF_ORIENTATION)
    if (exifOrientation > BitmapImage::ImageEXIFOrientationTopLeft) {
        CGContextConcatCTM(context, transform);
        if (!(exifOrientation & 0x01)) {
            // The destination rect will have it's width and height already reveresed for the orientation of
            // the image, as it was needed for page layout, so we need to reverse it back here.
            adjustedDestRect = FloatRect(adjustedDestRect.x(), adjustedDestRect.y(), adjustedDestRect.height(), adjustedDestRect.width());
        }
    }
#endif
    adjustedDestRect.setLocation(FloatPoint());

    // Draw the image.
    CGContextDrawImage(context, adjustedDestRect, image);

    if (shouldUseSubimage)
        CGImageRelease(image);

    ctxt->restore();

    if (imageObserver())
        imageObserver()->didDraw(this);
}

void Image::drawPatternCallback(void* info, CGContextRef context)
{
    CGImageRef image = (CGImageRef)info;
    float h = CGImageGetHeight(image);
    CGContextScaleCTM (context, 1, -1);
    CGContextTranslateCTM (context, 0, -h);
    CGContextDrawImage(context, GraphicsContext(context).roundToDevicePixels(FloatRect(0, 0, CGImageGetWidth(image), h)), image);
}

void Image::drawPattern(GraphicsContext* ctxt, const FloatRect& tileRect, const TransformationMatrix& patternTransform,
                        const FloatPoint& phase, CompositeOperator op, const FloatRect& destRect)
{
    if (!nativeImageForCurrentFrame())
        return;

    ASSERT(patternTransform.isInvertible());
    if (!patternTransform.isInvertible())
        // Avoid a hang under CGContextDrawTiledImage on release builds.
        return;

    CGContextRef context = ctxt->platformContext();
    ctxt->save();
    CGContextClipToRect(context, destRect);
    ctxt->setCompositeOperation(op);
    CGContextTranslateCTM(context, destRect.x(), destRect.y() + destRect.height());
    CGContextScaleCTM(context, 1, -1);
    
    // Compute the scaled tile size.
    float scaledTileHeight = tileRect.height() * narrowPrecisionToFloat(patternTransform.d());
    
    // We have to adjust the phase to deal with the fact we're in Cartesian space now (with the bottom left corner of destRect being
    // the origin).
    float adjustedX = phase.x() - destRect.x() + tileRect.x() * narrowPrecisionToFloat(patternTransform.a()); // We translated the context so that destRect.x() is the origin, so subtract it out.
    float adjustedY = destRect.height() - (phase.y() - destRect.y() + tileRect.y() * narrowPrecisionToFloat(patternTransform.d()) + scaledTileHeight);

    CGImageRef tileImage = nativeImageForCurrentFrame();
    float h = CGImageGetHeight(tileImage);

    CGImageRef subImage;
    if (tileRect.size() == size())
        subImage = tileImage;
    else {
        // Copying a sub-image out of a partially-decoded image stops the decoding of the original image. It should never happen
        // because sub-images are only used for border-image, which only renders when the image is fully decoded.
        ASSERT(h == height());
        subImage = CGImageCreateWithImageInRect(tileImage, tileRect);
    }
    
#if !defined(BUILDING_ON_TIGER)
// FIXME: <rdar://problem/5660638>
    // Leopard has an optimized call for the tiling of image patterns, but we can only use it if the image has been decoded enough that
    // its buffer is the same size as the overall image.  Because a partially decoded CGImageRef with a smaller width or height than the
    // overall image buffer needs to tile with "gaps", we can't use the optimized tiling call in that case.
    // FIXME: Could create WebKitSystemInterface SPI for CGCreatePatternWithImage2 and probably make Tiger tile faster as well.
    // FIXME: We cannot use CGContextDrawTiledImage with scaled tiles on Leopard, because it suffers from rounding errors.  Snow Leopard is ok.
    float scaledTileWidth = tileRect.width() * narrowPrecisionToFloat(patternTransform.a());
    float w = CGImageGetWidth(tileImage);
#ifdef BUILDING_ON_LEOPARD
    if (w == size().width() && h == size().height() && scaledTileWidth == tileRect.width() && scaledTileHeight == tileRect.height())
#else
    if (w == size().width() && h == size().height())
#endif
        CGContextDrawTiledImage(context, FloatRect(adjustedX, adjustedY, scaledTileWidth, scaledTileHeight), subImage);
    else {
#endif

    // On Leopard, this code now only runs for partially decoded images whose buffers do not yet match the overall size of the image.
    // On Tiger this code runs all the time.  This code is suboptimal because the pattern does not reference the image directly, and the
    // pattern is destroyed before exiting the function.  This means any decoding the pattern does doesn't end up cached anywhere, so we
    // redecode every time we paint.
    static const CGPatternCallbacks patternCallbacks = { 0, drawPatternCallback, NULL };
    CGAffineTransform matrix = CGAffineTransformMake(narrowPrecisionToCGFloat(patternTransform.a()), 0, 0, narrowPrecisionToCGFloat(patternTransform.d()), adjustedX, adjustedY);
    matrix = CGAffineTransformConcat(matrix, CGContextGetCTM(context));
    // The top of a partially-decoded image is drawn at the bottom of the tile. Map it to the top.
    matrix = CGAffineTransformTranslate(matrix, 0, size().height() - h);

    matrix = CGAffineTransformScale(matrix, 1, -1);
    matrix = CGAffineTransformTranslate(matrix, 0, -h);
    CGPatternRef pattern = CGPatternCreate(subImage, CGRectMake(0, 0, tileRect.width(), tileRect.height()),
                                           matrix, tileRect.width(), tileRect.height(), 
                                           kCGPatternTilingConstantSpacing, true, &patternCallbacks);
    if (pattern == NULL) {
        if (subImage != tileImage)
            CGImageRelease(subImage);
        ctxt->restore();
        return;
    }

    CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
    
    CGFloat alpha = 1;
    CGColorRef color = CGColorCreateWithPattern(patternSpace, pattern, &alpha);
    CGContextSetFillColorSpace(context, patternSpace);
    CGColorSpaceRelease(patternSpace);
    CGPatternRelease(pattern);

    // FIXME: Really want a public API for this.  It is just CGContextSetBaseCTM(context, CGAffineTransformIdentiy).
    wkSetPatternBaseCTM(context, CGAffineTransformIdentity);
    CGContextSetPatternPhase(context, CGSizeZero);

    CGContextSetFillColorWithColor(context, color);
    CGContextFillRect(context, CGContextGetClipBoundingBox(context));
    
    CGColorRelease(color);
    
#if !defined(BUILDING_ON_TIGER)
    }
#endif

    if (subImage != tileImage)
        CGImageRelease(subImage);
    ctxt->restore();

    if (imageObserver())
        imageObserver()->didDraw(this);
}


}

#endif // PLATFORM(CG)
