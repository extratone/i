/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#import "FrameSnapshottingMac.h"

#import "BlockExceptions.h"
#import "Document.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "Range.h"
#import "RenderView.h"

#if PLATFORM(IOS)
#import "WAKView.h"
#import "WKGraphics.h"
#import "WebCoreThread.h"
#import <wtf/ObjcRuntimeExtras.h>

@interface WAKView (WebCoreHTMLDocumentView)
- (void)drawSingleRect:(CGRect)rect;
@end
#endif

namespace WebCore {

#if !PLATFORM(IOS)
NSImage* imageFromRect(Frame* frame, NSRect rect)
{
    PaintBehavior oldBehavior = frame->view()->paintBehavior();
    frame->view()->setPaintBehavior(oldBehavior | PaintBehaviorFlattenCompositingLayers);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    NSImage* resultImage = [[[NSImage alloc] initWithSize:rect.size] autorelease];
    
    if (rect.size.width != 0 && rect.size.height != 0) {
        [resultImage setFlipped:YES];
        [resultImage lockFocus];

        GraphicsContext graphicsContext((CGContextRef)[[NSGraphicsContext currentContext] graphicsPort]);        
        graphicsContext.save();
        graphicsContext.translate(-rect.origin.x, -rect.origin.y);
        frame->view()->paintContents(&graphicsContext, IntRect(rect));
        graphicsContext.restore();

        [resultImage unlockFocus];
        [resultImage setFlipped:NO];
    }
    
    frame->view()->setPaintBehavior(oldBehavior);
    return resultImage;
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    frame->view()->setPaintBehavior(oldBehavior);
    return nil;
}

NSImage* selectionImage(Frame* frame, bool forceBlackText)
{
    frame->view()->setPaintBehavior(PaintBehaviorSelectionOnly | (forceBlackText ? PaintBehaviorForceBlackText : 0));
    frame->document()->updateLayout();
    NSImage* result = imageFromRect(frame, frame->selection()->bounds());
    frame->view()->setPaintBehavior(PaintBehaviorNormal);
    return result;
}

NSImage *rangeImage(Frame* frame, Range* range, bool forceBlackText)
{
#else
CGImageRef rangeImage(Frame* frame, Range* range, bool forceBlackText)
{
    ASSERT(!WebThreadIsEnabled() || WebThreadIsLocked());
#endif // !PLATFORM(IOS)
    frame->view()->setPaintBehavior(PaintBehaviorSelectionOnly | (forceBlackText ? PaintBehaviorForceBlackText : 0));
    frame->document()->updateLayout();
    RenderView* view = frame->contentRenderer();
    if (!view)
        return nil;

    Position start = range->startPosition();
    Position candidate = start.downstream();
    if (candidate.deprecatedNode() && candidate.deprecatedNode()->renderer())
        start = candidate;

    Position end = range->endPosition();
    candidate = end.upstream();
    if (candidate.deprecatedNode() && candidate.deprecatedNode()->renderer())
        end = candidate;

    if (start.isNull() || end.isNull() || start == end)
        return nil;

    RenderObject* savedStartRenderer;
    int savedStartOffset;
    RenderObject* savedEndRenderer;
    int savedEndOffset;
    view->getSelection(savedStartRenderer, savedStartOffset, savedEndRenderer, savedEndOffset);

    RenderObject* startRenderer = start.deprecatedNode()->renderer();
    if (!startRenderer)
        return nil;

    RenderObject* endRenderer = end.deprecatedNode()->renderer();
    if (!endRenderer)
        return nil;

    view->setSelection(startRenderer, start.deprecatedEditingOffset(), endRenderer, end.deprecatedEditingOffset(), RenderView::RepaintNothing);
#if !PLATFORM(IOS)
    NSImage* result = imageFromRect(frame, view->selectionBounds());
#else
    CGImageRef result = imageFromRect(frame, view->selectionBounds());
#endif
    view->setSelection(savedStartRenderer, savedStartOffset, savedEndRenderer, savedEndOffset, RenderView::RepaintNothing);

    frame->view()->setPaintBehavior(PaintBehaviorNormal);
    return result;
}


#if !PLATFORM(IOS)
NSImage* snapshotDragImage(Frame* frame, Node* node, NSRect* imageRect, NSRect* elementRect)
{
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nil;
    
    renderer->updateDragState(true);    // mark dragged nodes (so they pick up the right CSS)
    frame->document()->updateLayout();  // forces style recalc - needed since changing the drag state might
                                        // imply new styles, plus JS could have changed other things


    // Document::updateLayout may have blown away the original RenderObject.
    renderer = node->renderer();
    if (!renderer)
        return nil;

    LayoutRect topLevelRect;
    NSRect paintingRect = pixelSnappedIntRect(renderer->paintingRootRect(topLevelRect));

    frame->view()->setNodeToDraw(node); // invoke special sub-tree drawing mode
    NSImage* result = imageFromRect(frame, paintingRect);
    renderer->updateDragState(false);
    frame->document()->updateLayout();
    frame->view()->setNodeToDraw(0);

    if (elementRect)
        *elementRect = pixelSnappedIntRect(topLevelRect);
    if (imageRect)
        *imageRect = paintingRect;
    return result;
}
#endif // !PLATFORM(IOS)

#if PLATFORM(IOS)
CGImageRef imageFromRect(Frame* frame, CGRect rect, bool allowDownsampling)
{
    WAKView* view = frame->view()->documentView();
    if (!view)
        return nil;
    if (![view respondsToSelector:@selector(drawSingleRect:)])
        return nil;

    PaintBehavior oldPaintBehavior = frame->view()->paintBehavior();
    frame->view()->setPaintBehavior(oldPaintBehavior | PaintBehaviorFlattenCompositingLayers);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    CGRect bounds = [view bounds];

    float scale = frame->documentScale() * frame->deviceScaleFactor();

    if (allowDownsampling) {
        // Adjust the scale until the image width and height is within acceptable bounds.
        // FIXME: This value was chosen arbitrarily.
        const float maximumPixels = 960.0f * 640.0f;
        CGFloat maximumScale = sqrtf(maximumPixels / (rect.size.width * rect.size.height));
        scale = MIN(maximumScale, scale);
    }

    // Round image rect size in window coordinate space to avoid pixel cracks at HiDPI (4622794)
    rect = [view convertRect:rect toView:nil];
    rect.size.height = roundf(rect.size.height);
    rect.size.width = roundf(rect.size.width);
    rect = [view convertRect:rect fromView:nil];
    if (rect.size.width == 0 || rect.size.height == 0)
        return nil;

    size_t width = static_cast<size_t>(rect.size.width * scale);
    size_t height = static_cast<size_t>(rect.size.height * scale);
    size_t bitsPerComponent = 8;
    size_t bitsPerPixel = 4 * bitsPerComponent;
    size_t bytesPerRow = ((bitsPerPixel + 7) / 8) * width;
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGContextRef> context(AdoptCF, CGBitmapContextCreate(NULL, width, height, bitsPerComponent, bytesPerRow, colorSpace.get(), kCGImageAlphaPremultipliedLast));
    if (!context)
        return nil;

    CGContextRef oldContext = WKGetCurrentGraphicsContext();

    CGContextRef contextRef = context.get();
    WKSetCurrentGraphicsContext(contextRef);

    CGContextClearRect(contextRef, CGRectMake(0, 0, width, height));
    CGContextSaveGState(contextRef);
    CGContextScaleCTM(contextRef, scale, scale);
    CGContextSetBaseCTM(contextRef, CGAffineTransformMakeScale(scale, scale));
    CGContextTranslateCTM(contextRef, bounds.origin.x - rect.origin.x,  bounds.origin.y - rect.origin.y);

    [view drawSingleRect:rect];

    CGContextRestoreGState(contextRef);

    CGImageRef resultImage = CGBitmapContextCreateImage(contextRef);

    WKSetCurrentGraphicsContext(oldContext);

    frame->view()->setPaintBehavior(oldPaintBehavior);
    return (CGImageRef)HardAutorelease(resultImage);

    END_BLOCK_OBJC_EXCEPTIONS;

    frame->view()->setPaintBehavior(oldPaintBehavior);
    return nil;
}

CGImageRef selectionImage(Frame* frame, bool forceBlackText)
{
    ASSERT(!WebThreadIsEnabled() || WebThreadIsLocked());
    frame->view()->setPaintBehavior(PaintBehaviorSelectionOnly | (forceBlackText ? PaintBehaviorForceBlackText : 0));
    frame->document()->updateLayout();
    CGImageRef result = imageFromRect(frame, frame->selection()->bounds());
    frame->view()->setPaintBehavior(PaintBehaviorNormal);
    return result;
}

static bool isImageClear(CGImageRef image)
{
    // Determine whether anything was actually drawn into the image by drawing
    // it into a one-pixel bitmap context and checking if that pixel is clear.
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    uint32_t data = 0;
    RetainPtr<CGContextRef> context(AdoptCF, CGBitmapContextCreate(&data, 1, 1, 8, sizeof(data), colorSpace.get(), kCGImageAlphaPremultipliedLast));
    if (!context)
        return false;

    CGContextDrawImage(context.get(), CGRectMake(0, 0, 1, 1), image);
    // If the image had something in it, 'data' will be nonzero.
    return !data;
}

CGImageRef nodeImage(Frame* frame, Node* node, NodeImageFlags flags)
{
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nil;

    frame->document()->updateLayout(); // forces style recalc

    IntRect topLevelRect;
    CGRect paintingRect = renderer->absoluteBoundingBoxRect(true);

    frame->view()->setNodeToDraw(node); // invoke special sub-tree drawing mode
    CGImageRef result = imageFromRect(frame, paintingRect, flags & AllowDownsampling);
    frame->view()->setNodeToDraw(0);

    // Check if we need to redraw because the result image is clear.  If so,
    // redraw all page content (not just the node in question) contained within
    // the node bounds in an attempt to get some kind of non-clear image to
    // return.
    if ((flags & DrawContentBehindTransparentNodes) && isImageClear(result))
        return imageFromRect(frame, paintingRect, flags & AllowDownsampling);

    return result;
}
#endif // !PLATFORM(IOS)

} // namespace WebCore
