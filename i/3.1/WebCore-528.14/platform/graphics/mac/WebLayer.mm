/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#if USE(ACCELERATED_COMPOSITING)

#import "WebLayer.h"

#import "GraphicsContext.h"
#import "GraphicsLayer.h"
#import <QuartzCore/QuartzCore.h>
#import "WebCoreTextRenderer.h"
#import <wtf/UnusedParam.h>

#import "WKGraphics.h"
#import "WAKWindow.h"
#import "WebCoreThread.h"
#import <QuartzCore/QuartzCorePrivate.h>

using namespace WebCore;

@interface WebLayer(Private)
- (void)drawScaledContentsInContext:(CGContextRef)context;
@end

@implementation WebLayer

+ (void)drawContents:(WebCore::GraphicsLayer*)layerContents ofLayer:(CALayer*)layer intoContext:(CGContextRef)context
{
    UNUSED_PARAM(layer);

    WKSetCurrentGraphicsContext(context);

    CGContextSaveGState(context);

    CGContextSetShouldAntialias(context, NO);
    
    if (layerContents && layerContents->client()) {
        WKFontAntialiasingStateSaver fontAntialiasingState(true);
        fontAntialiasingState.setup([WAKWindow hasLandscapeOrientation]);
        GraphicsContext graphicsContext(context);

        // It's important to get the clip from the context, because it may be significantly
        // smaller than the layer bounds (e.g. tiled layers)
        CGRect clipBounds = CGContextGetClipBoundingBox(context);
        IntRect clip(enclosingIntRect(clipBounds));
        layerContents->paintGraphicsLayerContents(graphicsContext, clip);

        fontAntialiasingState.restore();
    }
#ifndef NDEBUG
    else {
        ASSERT_NOT_REACHED();

        // FIXME: ideally we'd avoid calling -setNeedsDisplay on a layer that is a plain color,
        // so CA never makes backing store for it (which is what -setNeedsDisplay will do above).
        CGContextSetRGBFillColor(context, 0.0f, 1.0f, 0.0f, 1.0f);
        CGRect aBounds = [layer bounds];
        CGContextFillRect(context, aBounds);
    }
#endif

    CGContextRestoreGState(context);

#ifdef SUPPORT_DEBUG_INDICATORS
    if (layerContents && layerContents->showRepaintCounter()) {
        bool isTiledLayer = [layer isKindOfClass:[CATiledLayer class]];

        char text[16]; // that's a lot of repaints
        snprintf(text, sizeof(text), "%d", layerContents->incrementRepaintCount());

        CGAffineTransform a = CGContextGetCTM(context);
        
        CGContextSaveGState(context);
        if (isTiledLayer)
            CGContextSetRGBFillColor(context, 0.0f, 1.0f, 0.0f, 0.8f);
        else
            CGContextSetRGBFillColor(context, 1.0f, 0.0f, 0.0f, 0.8f);
        
        CGRect aBounds = [layer bounds];

        aBounds.size.width = 10 + 12 * strlen(text);
        aBounds.size.height = 25;
        CGContextFillRect(context, aBounds);
        
        CGContextSetRGBFillColor(context, 0.0f, 0.0f, 0.0f, 1.0f);

        CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1.0f, -1.0f));
        CGContextSelectFont(context, "Helvetica", 25, kCGEncodingMacRoman);
        CGContextShowTextAtPoint(context, aBounds.origin.x + 3.0f, aBounds.origin.y + 20.0f, text, strlen(text));
        
        CGContextRestoreGState(context);        
    }
#endif // SUPPORT_DEBUG_INDICATORS
}

// Disable default animations
- (id<CAAction>)actionForKey:(NSString *)key
{
    UNUSED_PARAM(key);
    return nil;
}

- (id)init
{
    if ((self = [super init]))
        m_contentsScale = 1.0f;
    
    return self;
}

// Implement this so presentationLayer can get our custom attributes
- (id)initWithLayer:(id)layer
{
    if ((self = [super initWithLayer:layer])) {
        m_layerOwner = [(WebLayer*)layer layerOwner];
        m_contentsScale = [layer contentsScale];
    }
    
    return self;
}

- (void)setNeedsDisplay
{
    if (m_layerOwner && m_layerOwner->client() && m_layerOwner->drawsContent())
        [super setNeedsDisplay];
}

- (void)setNeedsDisplayInRect:(CGRect)dirtyRect
{
    if (m_layerOwner && m_layerOwner->client() && m_layerOwner->drawsContent()) {
        [CATransaction lock];

        id layerContents = [self contents];
        if (layerContents && CFGetTypeID(layerContents) == CABackingStoreGetTypeID()) {
            CABackingStoreRef backingStore = (CABackingStoreRef)layerContents;
            
            if (CGRectIsInfinite(dirtyRect))
                CABackingStoreInvalidate(backingStore, 0);
            else {
                CGRect layerBounds = [self bounds];
                CGAffineTransform layerToBackingTransform;

                // CA layers default to flipped on iPhone
                int32_t height = ceilf(layerBounds.size.height * m_contentsScale);

                layerToBackingTransform = CGAffineTransformMakeTranslation(0.0f, height);
                layerToBackingTransform = CGAffineTransformScale(layerToBackingTransform, 1.0f, -1.0f);
                layerToBackingTransform = CGAffineTransformScale(layerToBackingTransform, m_contentsScale, m_contentsScale);

                CGRect backingDirtyRect = CGRectApplyAffineTransform(dirtyRect, layerToBackingTransform);
                CABackingStoreInvalidate(backingStore, &backingDirtyRect);
            
#ifdef SUPPORT_DEBUG_INDICATORS
                if (m_layerOwner->showRepaintCounter()) {
                    CGRect counterRect = CGRectMake(layerBounds.origin.x, layerBounds.origin.y, 46, 25);
                    counterRect = CGRectApplyAffineTransform(counterRect, layerToBackingTransform);
                    CABackingStoreInvalidate(backingStore, &counterRect);
                }
#endif // SUPPORT_DEBUG_INDICATORS
            }
        }

        // just sets the 'display needed' flag on the layer
        [super setNeedsDisplayInRect:CGRectNull];
        [CATransaction unlock];
    }
}

static void backing_store_callback(CGContextRef inContext, void *info)
{
    WebLayer* self = reinterpret_cast<WebLayer*>(info);
    
    [self drawScaledContentsInContext:inContext];
}

- (void)display
{
    CGRect layerBounds = [self bounds];
    if (layerBounds.size.width <= 0.0f || layerBounds.size.height <= 0.0f)
        return;

    // This method is almost always called without the web thread lock held,
    // in which case we need to grab the lock. It can be called with the
    // web thread lock held via LCLayer::setParentLayer, which does a 
    // synchronous commit, and is always called on the web thread. In
    // that case the flag is set, indicating that we don't need to grab the
    // web thread lock. This is all because the web thread lock is not re-entrant
    // on the web thread, for policy reasons.
    bool locked = false;
    if (!WebThreadIsCurrent()  /* || !LCLayer::gInSynchronousCommit */) {
        WebThreadLock();
        locked = true;
    }

    ASSERT(WebThreadIsLocked());

    [CATransaction lock];

    id contents = [self contents];
    
    CABackingStoreRef backingStore;

    if (contents && CFGetTypeID(contents) == CABackingStoreGetTypeID()) {
        backingStore = (CABackingStoreRef)CFRetain(contents);
    } else {
        backingStore = CABackingStoreCreate();
        CABackingStoreInvalidate(backingStore, NULL);
    }
    
    [CATransaction unlock];
    
    if (backingStore) {
        // FIXME cap to max size
        int32_t width  = ceilf(layerBounds.size.width * m_contentsScale);
        int32_t height = ceilf(layerBounds.size.height * m_contentsScale);
        
        uint32_t flags;
        if ([self isOpaque])
            flags = kCABackingStoreOpaque;
        else
            flags = kCABackingStoreCleared;
        
        CABackingStoreUpdate(backingStore, width, height, flags, backing_store_callback, self);
        
        if ((id)backingStore != contents)
            [self setContents:(id)backingStore];
        
        [self setContentsChanged];
        CFRelease(backingStore);
    }
    else {
#ifndef NDEBUG
        NSLog(@"*** failed to create backing store for layer %@", self);
#endif
    }
    
    if (locked)
        WebThreadUnlock();
}

- (void)drawScaledContentsInContext:(CGContextRef)inContext
{
    // CA layers default to flipped on iPhone
    CGRect layerBounds = [self bounds];
    int32_t height = ceilf(layerBounds.size.height * m_contentsScale);

    CGContextTranslateCTM(inContext, 0.0f, height);
    CGContextScaleCTM(inContext, 1.0f, -1.0f);

    CGContextScaleCTM(inContext, m_contentsScale, m_contentsScale);

    [self drawInContext:inContext];
}

- (void)drawInContext:(CGContextRef)context
{
    [WebLayer drawContents:m_layerOwner ofLayer:self intoContext:context];
}

@end // implementation WebLayer

#pragma mark -

@implementation WebLayer(WebLayerAdditions)

- (void)setLayerOwner:(GraphicsLayer*)aLayer
{
    m_layerOwner = aLayer;
}

- (GraphicsLayer*)layerOwner
{
    return m_layerOwner;
}

- (float)contentsScale
{
    return m_contentsScale;
}

- (void)setContentsScale:(float)scale
{
    if (scale != m_contentsScale)
    {
        m_contentsScale = scale;
        
        [self setContents:nil];
        [self setNeedsDisplay];
    }
}

@end

#pragma mark -

#ifndef NDEBUG

@implementation CALayer(ExtendedDescription)

- (NSString*)_descriptionWithPrefix:(NSString*)inPrefix
{
    CGRect aBounds = [self bounds];
    CGPoint aPos = [self position];
    CATransform3D t = [self transform];

    NSString* selfString = [NSString stringWithFormat:@"%@<%@ 0x%08x> \"%@\" bounds(%.1f, %.1f, %.1f, %.1f) pos(%.1f, %.1f), sublayers=%d masking=%d",
            inPrefix,
            [self class],
            self,
            [self name],
            aBounds.origin.x, aBounds.origin.y, aBounds.size.width, aBounds.size.height, 
            aPos.x, aPos.y,
            [[self sublayers] count],
            [self masksToBounds]];
    
    NSMutableString* curDesc = [NSMutableString stringWithString:selfString];
    
    if ([[self sublayers] count] > 0)
        [curDesc appendString:@"\n"];

    NSString* sublayerPrefix = [inPrefix stringByAppendingString:@"\t"];

    NSEnumerator* sublayersEnum = [[self sublayers] objectEnumerator];
    CALayer* curLayer;
    while ((curLayer = [sublayersEnum nextObject]))
        [curDesc appendString:[curLayer _descriptionWithPrefix:sublayerPrefix]];

    if ([[self sublayers] count] == 0)
        [curDesc appendString:@"\n"];

    return curDesc;
}

- (NSString*)extendedDescription
{
    return [self _descriptionWithPrefix:@""];
}

@end  // implementation WebLayer(ExtendedDescription)

#endif // NDEBUG

#endif // USE(ACCELERATED_COMPOSITING)
