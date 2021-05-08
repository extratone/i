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

#import "config.h"
#import "GraphicsContext.h"

#import "../cg/GraphicsContextPlatformPrivateCG.h"
#import <wtf/StdLibExtras.h>

#import "WKGraphics.h"
#import <wtf/UnusedParam.h>

#import "WebCoreSystemInterface.h"

// FIXME: More of this should use CoreGraphics instead of AppKit.
// FIXME: More of this should move into GraphicsContextCG.cpp.

namespace WebCore {

// NSColor, NSBezierPath, and NSGraphicsContext
// calls in this file are all exception-safe, so we don't block
// exceptions for those.

void GraphicsContext::drawFocusRing(const Color& color)
{
    UNUSED_PARAM(color);
}

#ifdef BUILDING_ON_TIGER // Post-Tiger's setCompositeOperation() is defined in GraphicsContextCG.cpp.
void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [[NSGraphicsContext graphicsContextWithGraphicsPort:platformContext() flipped:YES]
        setCompositingOperation:(NSCompositingOperation)op];
    [pool drain];
}
#endif

static CGPatternRef createSpellingPattern(bool& usingDot)
{
    CGImageRef image = WKGraphicsCreateImageFromBundleWithName("SpellingDot");
    ASSERT(image); // if image is not available, we want to know
    CGPatternRef spellingPattern = WKCreatePatternFromCGImage(image);
    CGImageRelease(image);
    usingDot = true;
    return spellingPattern;
}
    
void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint& point, int width, bool grammar)
{
    if (paintingDisabled())
        return;
        
    // These are the same for misspelling or bad grammar
    int patternHeight = cMisspellingLineThickness;
    int patternWidth = cMisspellingLinePatternWidth;
 
    UNUSED_PARAM(grammar);
    bool usingDot = false;
    DEFINE_STATIC_LOCAL(RetainPtr<CGPattern>, spellingPattern, (createSpellingPattern(usingDot)));

    // Make sure to draw only complete dots.
    // NOTE: Code here used to shift the underline to the left and increase the width
    // to make sure everything gets underlined, but that results in drawing out of
    // bounds (e.g. when at the edge of a view) and could make it appear that the
    // space between adjacent misspelled words was underlined.
    if (usingDot) {
        // allow slightly more considering that the pattern ends with a transparent pixel
        int widthMod = width % patternWidth;
        if (patternWidth - widthMod > cMisspellingLinePatternGapWidth)
            width -= widthMod;
    }
    
    // FIXME: This code should not use NSGraphicsContext currentContext
    // In order to remove this requirement we will need to use CGPattern instead of NSColor
    // FIXME: This code should not be using wkSetPatternPhaseInUserSpace, as this approach is wrong
    // for transforms.

    // Draw underline
    CGContextRef context = WKGetCurrentGraphicsContext();
    CGContextSaveGState(context);

    WKSetPattern(context, spellingPattern.get(), YES, YES);

    wkSetPatternPhaseInUserSpace(context, point);

    WKRectFillUsingOperation(context, CGRectMake(point.x(), point.y(), width, patternHeight), kCGCompositeDover);
    
    CGContextRestoreGState(context);
}

void GraphicsContext::setPenFromCGColor (CGColorRef color)
{
    float r, g, b, a;
    GSColorGetRGBAComponents(color, &r, &g, &b, &a);
    setStrokeColor(makeRGBA((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255)));
}

}
