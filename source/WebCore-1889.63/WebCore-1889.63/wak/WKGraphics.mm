//
//  WKGraphics.c
//
//  Copyright (C) 2005, 2006, 2007, 2009 Apple Inc.  All rights reserved.
//

#import "config.h"
#import "WKGraphics.h"

#import "WebCoreSystemInterface.h"
#import "Font.h"
#import "WebCoreThread.h"
#import <ImageIO/ImageIO.h>
#import <wtf/StdLibExtras.h>

using namespace WebCore;

static inline void _FillRectsUsingOperation(CGContextRef context, const CGRect* rects, int count, CGCompositeOperation op)
{
    int i;
    CGRect *integralRects = reinterpret_cast<CGRect *>(alloca(sizeof(CGRect) * count));
    
    assert (integralRects);
    
    for (i = 0; i < count; i++) {
        integralRects[i] = CGRectApplyAffineTransform (rects[i], CGContextGetCTM(context));
        integralRects[i] = CGRectIntegral (integralRects[i]);
        integralRects[i] = CGRectApplyAffineTransform (integralRects[i], CGAffineTransformInvert(CGContextGetCTM(context)));
    }
    CGCompositeOperation oldOp = CGContextGetCompositeOperation(context);
    CGContextSetCompositeOperation(context, op);
    CGContextFillRects(context, integralRects, count);
    CGContextSetCompositeOperation(context, oldOp);
}

static inline void _FillRectUsingOperation(CGContextRef context, CGRect rect, CGCompositeOperation op)
{
    if (rect.size.width > 0 && rect.size.height > 0) {
        _FillRectsUsingOperation (context, &rect, 1, op);
    }
}

void WKDrawFramedRect(CGContextRef context, CGRect aRect)
{
    WKDrawFramedRectWithWidthUsingOperation (context, aRect, 1.0f, kCGCompositeCopy);
}

void WKDrawFramedRectWithWidthUsingOperation(CGContextRef context, CGRect aRect, float frameWidth, CGCompositeOperation op)
{
    if (CGRectIsEmpty(aRect))
    {
	CGContextSaveGState(context);
        
        CGRect rect[4];
        
        // special case the framing 0 width rectangle. use very thin line (close to 1/72 point)
        // with antialiasing turned off. we need to transform this size by the inverse of the CTM to
        // guarantee it's close to 1/72 in the device (untransformed space)
        //
        float width  = frameWidth;
        float height = frameWidth;
        bool zeroWidth = (frameWidth == 0.0);

        if (zeroWidth) {
            CGAffineTransform currentCTM = CGContextGetCTM(context);
            CGSize size = CGSizeApplyAffineTransform(CGSizeMake(1.0f / 71.0f, 1.0f / 71.0f), CGAffineTransformInvert(currentCTM));
            width  = size.width;
            height = size.height;

            CGContextSetShouldAntialias (context, false);
        }

        // calculate top, right, bottom, and left sides of rectangle (non-overlapping)
        //
        rect[0] = CGRectMake(aRect.origin.x, aRect.origin.y, aRect.size.width, height);
        rect[1] = CGRectMake(aRect.origin.x, aRect.origin.y + aRect.size.height - height, aRect.size.width, height);
        rect[2] = CGRectMake(aRect.origin.x, aRect.origin.y + height, width, aRect.size.height - 2 * height);
        rect[3] = CGRectMake(aRect.origin.x + aRect.size.width - width, aRect.origin.y + height, width, aRect.size.height - 2 * height);

        _FillRectsUsingOperation(context, rect, 4, op);
        
        CGContextRestoreGState(context);
    }
}

void WKRectFill(CGContextRef context, CGRect aRect)
{
    if (aRect.size.width > 0 && aRect.size.height > 0) {
	CGContextSaveGState(context);
	_FillRectUsingOperation(context, aRect, kCGCompositeCopy);
	CGContextRestoreGState(context);
    }
}

void WKRectFillList(CGContextRef context, const CGRect *rects, int count)
{
    _FillRectsUsingOperation(context, rects, count, kCGCompositeCopy);
}

void WKRectFillUsingOperation(CGContextRef context, CGRect aRect, CGCompositeOperation op)
{
    if (aRect.size.width > 0 && aRect.size.height > 0.0) {
	CGContextSaveGState(context);
	_FillRectUsingOperation(context, aRect, op);
	CGContextRestoreGState(context);
    }
}

void WKRectFillListUsingOperation(CGContextRef context, const CGRect *rects, int count, CGCompositeOperation op)
{
    _FillRectsUsingOperation(context, rects, count, op);
}

void WKSetCurrentGraphicsContext(CGContextRef context)
{
    WebThreadContext* threadContext =  WebThreadCurrentContext();
    threadContext->currentCGContext = context;
}

CGContextRef WKGetCurrentGraphicsContext(void)
{
    WebThreadContext* threadContext =  WebThreadCurrentContext();
    return threadContext->currentCGContext;
}

static NSString *imageResourcePath(const char* imageFile, bool is2x)
{
    NSString *fileName = is2x ? [NSString stringWithFormat:@"%s@2x", imageFile] : [NSString stringWithUTF8String:imageFile];
#if PLATFORM(IOS_SIMULATOR)
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebCore"];
    return [bundle pathForResource:fileName ofType:@"png"];
#else
    // Workaround for <rdar://problem/7780665> CFBundleCopyResourceURL takes a long time on iPhone 3G.
    NSString *imageDirectory = @"/System/Library/PrivateFrameworks/WebCore.framework";
    return [NSString stringWithFormat:@"%@/%@.png", imageDirectory, fileName];
#endif
}

CGImageRef WKGraphicsCreateImageFromBundleWithName (const char *image_file)
{
    if (!image_file)
        return NULL;
    
    CGImageRef image = NULL;
    NSData *imageData = nil;
    
    if (wkGetScreenScaleFactor() == 2.0f) {
        NSString* full2xPath = imageResourcePath(image_file, true);
        imageData = [NSData dataWithContentsOfFile:full2xPath];
    }
    if (!imageData) {
        // We got here either because we didn't request hi-dpi or the @2x file doesn't exist.
        NSString* full1xPath = imageResourcePath(image_file, false);
        imageData = [NSData dataWithContentsOfFile:full1xPath];
    }
    
    if (imageData) {
        RetainPtr<CGDataProviderRef> dataProvider(AdoptCF, CGDataProviderCreateWithCFData((CFDataRef)imageData));
        image = CGImageCreateWithPNGDataProvider(dataProvider.get(), NULL, NO, kCGRenderingIntentDefault);
    }

    return image;
}

static void WKDrawPatternBitmap(void *info, CGContextRef c) 
{
    CGImageRef image = (CGImageRef)info;
    CGFloat scale = wkGetScreenScaleFactor();
    CGContextDrawImage(c, CGRectMake(0, 0, CGImageGetWidth(image) / scale, CGImageGetHeight(image) / scale), image);    
}

static void WKReleasePatternBitmap(void *info) 
{
    CGImageRelease(reinterpret_cast<CGImageRef>(info));
}

static const CGPatternCallbacks WKPatternBitmapCallbacks = 
{
    0, WKDrawPatternBitmap, WKReleasePatternBitmap
};

CGPatternRef WKCreatePatternFromCGImage(CGImageRef imageRef)
{
    // retain image since it's freed by our callback
    CGImageRetain(imageRef);

    CGFloat scale = wkGetScreenScaleFactor();
    return CGPatternCreate((void*)imageRef, CGRectMake(0, 0, CGImageGetWidth(imageRef) / scale, CGImageGetHeight(imageRef) / scale), CGAffineTransformIdentity, CGImageGetWidth(imageRef) / scale, CGImageGetHeight(imageRef) / scale, kCGPatternTilingConstantSpacing, 1 /*isColored*/, &WKPatternBitmapCallbacks);
}

void WKSetPattern(CGContextRef context, CGPatternRef pattern, bool fill, bool stroke) 
{
    if (pattern == NULL)
        return;

    CGFloat patternAlpha = 1;
    CGColorSpaceRef colorspace = CGColorSpaceCreatePattern(NULL);
    if (fill) {
        CGContextSetFillColorSpace(context, colorspace);
        CGContextSetFillPattern(context, pattern, &patternAlpha);
    }
    if (stroke) {
        CGContextSetStrokeColorSpace(context, colorspace);
        CGContextSetStrokePattern(context, pattern, &patternAlpha);
    }
    CGColorSpaceRelease(colorspace);
}

void WKFontAntialiasingStateSaver::setup(bool isLandscapeOrientation)
{
#if !PLATFORM(IOS_SIMULATOR)
    m_oldAntialiasingStyle = CGContextGetFontAntialiasingStyle(m_context);

    if (m_useOrientationDependentFontAntialiasing)
        CGContextSetFontAntialiasingStyle(m_context, isLandscapeOrientation ? kCGFontAntialiasingStyleFilterLight : kCGFontAntialiasingStyleUnfiltered);
#else
    UNUSED_PARAM(isLandscapeOrientation);
#endif
}

void WKFontAntialiasingStateSaver::restore()
{
#if !PLATFORM(IOS_SIMULATOR)
    if (m_useOrientationDependentFontAntialiasing)
        CGContextSetFontAntialiasingStyle(m_context, m_oldAntialiasingStyle);
#endif
}
