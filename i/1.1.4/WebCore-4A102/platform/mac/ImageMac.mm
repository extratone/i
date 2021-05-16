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

#import "config.h"
#import "Image.h"

#import "FloatRect.h"
#import "FoundationExtras.h"
#import "GraphicsContext.h"
#import "PDFDocumentImage.h"
#import "PlatformString.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreSystemInterface.h"

#import <WebCore/WKGraphics.h>

namespace WebCore {

void FrameData::clear()
{
    if (m_frame) {
        CGImageRelease(m_frame);
        m_frame = 0;
		m_bytes = 0;
		m_scale = 0.0;
		m_haveInfo = false;
        m_duration = 0.;
        m_hasAlpha = true;
    }
}

// ================================================
// Image Class
// ================================================

void Image::initNativeData()
{
    m_tiffRep = 0;
    m_isPDF = false;
    m_PDFDoc = 0;
}

void Image::destroyNativeData()
{
    delete m_PDFDoc;
}

void Image::invalidateNativeData()
{
    if (m_frames.size() != 1)
        return;


    if (m_tiffRep) {
        CFRelease(m_tiffRep);
        m_tiffRep = 0;
    }
}

Image* Image::loadResource(const char *name)
{
    NSBundle *bundle = [NSBundle bundleForClass:[WebCoreFrameBridge class]];
    NSString *imagePath = [bundle pathForResource:[NSString stringWithUTF8String:name] ofType:@"png"];
    NSData *namedImageData = [NSData dataWithContentsOfFile:imagePath];
    if (namedImageData) {
        Image* image = new Image;
        image->setNativeData((CFDataRef)namedImageData, true);
        return image;
    }
    return 0;
}

bool Image::supportsType(const String& type)
{
    // FIXME: Would be better if this was looking in a set rather than an NSArray.
    // FIXME: Would be better not to convert to an NSString just to check if a type is supported.
    return [[WebCoreFrameBridge supportedImageResourceMIMETypes] containsObject:type];
}

// Drawing Routines

void Image::checkForSolidColor()
{
    if (frameCount() > 1)
        m_isSolidColor = false;
    else {
        CGImageRef image;
        
        // Currently we only check for solid color in the important special case of a 1x1 image.
        if ((m_source.size() == IntSize(1, 1)) && (image = frameAtIndex(0))) {
            unsigned char pixel[4] = {0, 0, 0, 0}; // RGBA
            CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
            CGContextRef bmap = CGBitmapContextCreate(&pixel, 1, 1, 8, sizeof(pixel), space, kCGImageAlphaPremultipliedLast);
            if (bmap) {
                GraphicsContext(bmap).setCompositeOperation(CompositeCopy);
                CGRect dst = { {0, 0}, {1, 1} };
                CGContextDrawImage(bmap, dst, image);
                m_solidColor = Color((int)pixel[0], (int)pixel[1], (int)pixel[2], (int)pixel[3]);
                m_isSolidColor = true;
                CFRelease(bmap);
            } 
            CFRelease(space);
        }
    }
}

CFDataRef Image::getTIFFRepresentation()
{
    if (m_tiffRep)
        return m_tiffRep;
    
    unsigned numFrames = frameCount();
    CFMutableDataRef data = CFDataCreateMutable(0, 0);
    // FIXME:  Use type kCGImageTypeIdentifierTIFF constant once is becomes available in the API
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(data, CFSTR("public.tiff"), numFrames, 0);
    if (!destination)
        return 0;

    for (unsigned i = 0; i < numFrames; ++i ) {
        CGImageRef cgImage = frameAtIndex(i);
        if (!cgImage) {
            CFRelease(destination);
            return 0;    
        }
        CGImageDestinationAddImage(destination, cgImage, 0);
    }
    CGImageDestinationFinalize(destination);
    CFRelease(destination);

    m_tiffRep = data;
    return m_tiffRep;
}


CGImageRef Image::getCGImageRef()
{
    return frameAtIndex(0);
}

void Image::draw(GraphicsContext* ctxt, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator compositeOp)
{
    if (m_isPDF) {
        if (m_PDFDoc)
            m_PDFDoc->draw(ctxt, srcRect, dstRect, compositeOp);
        return;
    } 
    
    if (!m_source.initialized())
        return;
    
    CGRect fr = ctxt->roundToDevicePixels(srcRect);
    CGRect ir = ctxt->roundToDevicePixels(dstRect);
	CGRect dr = CGRectApplyAffineTransform(ir, CGContextGetCTM(ctxt->platformContext()));

    CGImageRef image = frameAtIndex(m_currentFrame, std::min(1.0f, std::max(dr.size.width  / fr.size.width,
                                                                            dr.size.height / fr.size.height)));

    if (!image) // If it's too early we won't have an image yet.
        return;

    if (m_isSolidColor && m_currentFrame == 0) {
        if (m_solidColor.alpha() > 0) {
            ctxt->setCompositeOperation(!m_solidColor.hasAlpha() && compositeOp == CompositeSourceOver ? CompositeCopy : compositeOp);
            ctxt->fillRect(ir, m_solidColor);
        }
        return;
    }

    CGContextRef context = ctxt->platformContext();
    ctxt->save();

    // Get the height (in adjusted, i.e. scaled, coords) of the portion of the image
    // that is currently decoded.  This could be less that the actual height.
    CGSize selfSize = size();                                                            // full image size, in pixels
    float curHeight = CGImageGetHeight(image) * selfSize.width / CGImageGetWidth(image); // height of loaded portion, in pixels
    
    CGSize adjustedSize = selfSize;
    if (curHeight < selfSize.height) {
        adjustedSize.height *= curHeight / selfSize.height;

        // Is the amount of available bands less than what we need to draw?  If so,
        // we may have to clip 'fr' if it goes outside the available bounds.
        if (CGRectGetMaxY(fr) > adjustedSize.height) {
            float frHeight = adjustedSize.height - fr.origin.y; // clip fr to available bounds
            if (frHeight <= 0)
                return;                                             // clipped out entirely
            ir.size.height *= (frHeight / fr.size.height);    // scale ir proportionally to fr
            fr.size.height = frHeight;
        }
    }

    // Flip the coords.
    ctxt->setCompositeOperation(compositeOp);
    CGContextTranslateCTM(context, ir.origin.x, ir.origin.y);
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -ir.size.height);
    
    // Translated to origin, now draw at 0,0.
    ir.origin.x = ir.origin.y = 0;
    
    // If we're drawing a sub portion of the image then create
    // a image for the sub portion and draw that.
    // Test using example site at http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html
    if (fr.size.width != adjustedSize.width || fr.size.height != adjustedSize.height) {
        // Convert ft to image pixel coords:
        float xscale = adjustedSize.width / selfSize.width;
        float yscale = adjustedSize.height / curHeight;     // yes, curHeight, not selfSize.height!
        fr.origin.x /= xscale;
        fr.origin.y /= yscale;
        fr.size.width /= xscale;
        fr.size.height /= yscale;
        
        image = CGImageCreateWithImageInRect(image, fr);
        if (image) {
            CGContextDrawImage(context, ir, image);
            CFRelease(image);
        }
    } else // Draw the whole image.
        CGContextDrawImage(context, ir, image);

    ctxt->restore();
    
    startAnimation();

}

static void drawPattern(void* info, CGContextRef context)
{
    Image* data = (Image*)info;
    CGImageRef image = data->frameAtIndex(data->currentFrame());
    float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);
// If we are scaling set the interpolation to high to match view display.
//    CGAffineTransform m = CGContextGetCTM(context);
//    if (m.d != 1)
//        CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
    
    CGContextScaleCTM (context, 1, -1);
    CGContextTranslateCTM (context, 0, -h);
    
    CGContextDrawImage(context, GraphicsContext(context).roundToDevicePixels(FloatRect
        (0, data->size().height() - h, w, h)), image);    
}

static const CGPatternCallbacks patternCallbacks = { 0, drawPattern, NULL };

void Image::drawTiled(GraphicsContext* ctxt, const FloatRect& destRect, const FloatPoint& srcPoint,
                      const FloatSize& tileSize, CompositeOperator op)
{    
    CGImageRef image = frameAtIndex(m_currentFrame);
    if (!image)
        return;

    if (m_isSolidColor && m_currentFrame == 0) {
        if (m_solidColor.alpha() > 0) {
            ctxt->setCompositeOperation(!m_solidColor.hasAlpha() && op == CompositeSourceOver ? CompositeCopy : op);
            ctxt->fillRect(destRect, m_solidColor);
        }
        return;
    }

    CGPoint scaledPoint = srcPoint;
    CGSize intrinsicTileSize = size();
    CGSize scaledTileSize = intrinsicTileSize;

    // If tileSize is not equal to the intrinsic size of the image, set patternTransform
    // to the appropriate scalar matrix, scale the source point, and set the size of the
    // scaled tile. 
    float scaleX = 1.0;
    float scaleY = 1.0;
    CGAffineTransform patternTransform = CGAffineTransformIdentity;
    if (tileSize.width() != intrinsicTileSize.width || tileSize.height() != intrinsicTileSize.height) {
        scaleX = tileSize.width() / intrinsicTileSize.width;
        scaleY = tileSize.height() / intrinsicTileSize.height;
        patternTransform = CGAffineTransformMakeScale(scaleX, scaleY);
        scaledTileSize = tileSize;
    }

    // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
    NSRect oneTileRect;
    oneTileRect.origin.x = destRect.x() + fmodf(fmodf(-scaledPoint.x, scaledTileSize.width) - 
                            scaledTileSize.width, scaledTileSize.width);
    oneTileRect.origin.y = destRect.y() + fmodf(fmodf(-scaledPoint.y, scaledTileSize.height) - 
                            scaledTileSize.height, scaledTileSize.height);
    oneTileRect.size.width = scaledTileSize.width;
    oneTileRect.size.height = scaledTileSize.height;

    // If the single image draw covers the whole area, then just draw once.
    if (NSContainsRect(oneTileRect, destRect)) {
        CGRect fromRect;
        fromRect.origin.x = (destRect.x() - oneTileRect.origin.x) / scaleX;
        fromRect.origin.y = (destRect.y() - oneTileRect.origin.y) / scaleY;
        fromRect.size.width = destRect.width() / scaleX;
        fromRect.size.height = destRect.height() / scaleY;

        draw(ctxt, destRect, fromRect, op);
        return;
    }

    // If the tile is greater than the screen size, tile it without using CGPattern
    // since CGPattern will end up using a lot of memory when the tile size is large (4691859).
    CGSize screenSize = GSMainScreenSize();
    if (tileSize.width() * tileSize.height() > screenSize.width * screenSize.height) {
        float fromY = (destRect.y() - oneTileRect.origin.y) / scaleY;
        float toY = oneTileRect.origin.y;
        while (toY < CGRectGetMaxY(destRect)) {
            float fromX = (destRect.x() - oneTileRect.origin.x) / scaleX;
            float toX = oneTileRect.origin.x;
            while (toX < CGRectGetMaxX(destRect)) {
                CGRect fromRect = CGRectMake(fromX, fromY, intrinsicTileSize.width, intrinsicTileSize.height);
                CGRect toRect = CGRectMake(toX, toY, oneTileRect.size.width, oneTileRect.size.height);
                draw(ctxt, toRect, fromRect, op);
                toX += oneTileRect.size.width;
                fromX = 0;
            }
            toY += oneTileRect.size.height;
            fromY = 0;
        }
        return;
    }

    CGAffineTransform currentTransform = CGContextGetCTM(ctxt->platformContext());
    
    // Use the current transform, except the translation.
    currentTransform.tx = 0;
    currentTransform.ty = 0;
    
    CGPatternRef pattern = CGPatternCreate(this, CGRectMake (0, 0, tileSize.width(), tileSize.height()), currentTransform, tileSize.width(), tileSize.height(),
                                           kCGPatternTilingConstantSpacing, true, &patternCallbacks);    
    
    if (pattern) {
        CGContextRef context = ctxt->platformContext();

        ctxt->save();

        wkSetPatternPhaseInUserSpace(context, CGPointMake(oneTileRect.origin.x, oneTileRect.origin.y));

        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
        CGContextSetFillColorSpace(context, patternSpace);
        CGColorSpaceRelease(patternSpace);

        CGFloat patternAlpha = 1;
        CGContextSetFillPattern(context, pattern, &patternAlpha);

        ctxt->setCompositeOperation(op);

        CGContextFillRect(context, destRect);

        ctxt->restore();

        CGPatternRelease(pattern);
    }
    
    startAnimation();
}

// FIXME: Merge with the other drawTiled eventually, since we need a combination of both for some things.
void Image::drawTiled(GraphicsContext* ctxt, const FloatRect& dstRect, const FloatRect& srcRect, TileRule hRule,
                      TileRule vRule, CompositeOperator op)
{    
    CGImageRef image = frameAtIndex(m_currentFrame);
    if (!image)
        return;

    if (m_isSolidColor && m_currentFrame == 0) {
        if (m_solidColor.alpha() > 0) {
            ctxt->setCompositeOperation(!m_solidColor.hasAlpha() && op == CompositeSourceOver ? CompositeCopy : op);
            ctxt->fillRect(dstRect, m_solidColor);
        }
        return;
    }

    ctxt->save();

    CGSize tileSize = srcRect.size();
    CGRect ir = dstRect;
    CGRect fr = srcRect;

    // Now scale the slice in the appropriate direction using an affine transform that we will pass into
    // the pattern.
    float scaleX = 1.0f, scaleY = 1.0f;

    if (hRule == Image::StretchTile)
        scaleX = ir.size.width / fr.size.width;
    if (vRule == Image::StretchTile)
        scaleY = ir.size.height / fr.size.height;
    
    if (hRule == Image::RepeatTile)
        scaleX = scaleY;
    if (vRule == Image::RepeatTile)
        scaleY = scaleX;
        
    if (hRule == Image::RoundTile) {
        // Complicated math ensues.
        float imageWidth = fr.size.width * scaleY;
        float newWidth = ir.size.width / ceilf(ir.size.width / imageWidth);
        scaleX = newWidth / fr.size.width;
    }
    
    if (vRule == Image::RoundTile) {
        // More complicated math ensues.
        float imageHeight = fr.size.height * scaleX;
        float newHeight = ir.size.height / ceilf(ir.size.height / imageHeight);
        scaleY = newHeight / fr.size.height;
    }
    
    CGAffineTransform patternTransform = CGAffineTransformMakeScale(scaleX, scaleY);

    // Possible optimization:  We may want to cache the CGPatternRef    
    CGAffineTransform currentTransform = CGContextGetCTM(ctxt->platformContext());
    
    // Use the current transform, except the translation.
    currentTransform.tx = 0;
    currentTransform.ty = 0;
    
    CGPatternRef pattern = CGPatternCreate(this, CGRectMake (0, 0, tileSize.width, tileSize.height), currentTransform, tileSize.width, tileSize.height,
                                           kCGPatternTilingConstantSpacing, true, &patternCallbacks);
    
    if (pattern) {
        CGContextRef context = ctxt->platformContext();
    
        // We want to construct the phase such that the pattern is centered (when stretch is not
        // set for a particular rule).
        float hPhase = scaleX * fr.origin.x;
        float vPhase = scaleY * (tileSize.height - fr.origin.y);
        if (hRule == Image::RepeatTile)
            hPhase -= fmodf(ir.size.width, scaleX * tileSize.width) / 2.0f;
        if (vRule == Image::RepeatTile)
            vPhase -= fmodf(ir.size.height, scaleY * tileSize.height) / 2.0f;
        
        wkSetPatternPhaseInUserSpace(context, CGPointMake(ir.origin.x - hPhase, ir.origin.y - vPhase));

        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
        CGContextSetFillColorSpace(context, patternSpace);
        CGColorSpaceRelease(patternSpace);

        CGFloat patternAlpha = 1;
        CGContextSetFillPattern(context, pattern, &patternAlpha);

        ctxt->setCompositeOperation(op);
        
        CGContextFillRect(context, ir);

        CGPatternRelease(pattern);
    }

    ctxt->restore();

    startAnimation();
}

}
