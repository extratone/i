/*
 * Copyright (C) 2011, Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TileGridTile_h
#define TileGridTile_h

#if PLATFORM(IOS)

#include "IntRect.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

@class TileLayer;

namespace WebCore {
    
class TileGrid;

// Refcount the tiles so they work nicely in vector and we know when to remove the tile layer from the parent.
class TileGridTile : public RefCounted<TileGridTile> {
public:
    static PassRefPtr<TileGridTile> create(TileGrid* grid, const IntRect& rect) { return adoptRef<TileGridTile>(new TileGridTile(grid, rect)); }
    ~TileGridTile();
    
    TileLayer* tileLayer() const { return m_tileLayer.get(); }
    void invalidateRect(const IntRect& rectInSurface);
    IntRect rect() const { return m_rect; }
    void setRect(const IntRect& tileRect);
    void showBorder(bool);
    
private:
    TileGridTile(TileGrid*, const IntRect&);
    
    TileGrid* m_tileGrid;
    RetainPtr<TileLayer> m_tileLayer;
    IntRect m_rect;
};

}

#endif
#endif
