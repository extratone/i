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

#ifndef WebLayer_h
#define WebLayer_h

#if USE(ACCELERATED_COMPOSITING)

#import <QuartzCore/QuartzCore.h>

namespace WebCore {
    class GraphicsLayer;
}

// Category implemented by WebLayer and WebTiledLayer.
@interface CALayer(WebLayerAdditions)

- (void)setLayerOwner:(WebCore::GraphicsLayer*)layer;
- (WebCore::GraphicsLayer*)layerOwner;

- (float)contentsScale;
- (void)setContentsScale:(float)scale;

@end

@interface WebLayer : CALayer 
{
    WebCore::GraphicsLayer* m_layerOwner;
    float m_contentsScale;
}

// Class method allows us to share implementation across TiledLayerMac and WebLayer
+ (void)drawContents:(WebCore::GraphicsLayer*)layerContents ofLayer:(CALayer*)layer intoContext:(CGContextRef)context;

@end

#endif // USE(ACCELERATED_COMPOSITING)

#endif // WebLayer_h
