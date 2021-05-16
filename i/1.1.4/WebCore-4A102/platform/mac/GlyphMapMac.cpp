/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GlyphMap.h"
#include "FontData.h"
#include "WebCoreSystemInterface.h"

namespace WebCore
{

bool GlyphMap::fillPage(GlyphPage* page, UChar* buffer, unsigned bufferLength, const FontData* fontData)
{
    CGGlyph glyphs[cGlyphPageSize * 2];
        
    // We pass in either 256 or 512  UTF-16 characters
    // 256 for U+FFFF and less
    // 512 (double character surrogates) for U+1000 and above
    // It is indeed possible to get back 512 glyphs back from the API, so the glyph buffer we pass in must be 512
    // If we get back more than 256 glyphs though we'll ignore all the ones after 256, this should not happen 
    // as the only time we pass in 512 characters is when they are surrogates.
    GSFontGetGlyphsForUnichars(fontData->platformData().font, buffer, glyphs, bufferLength);
	
    // Loop through at most 256 glyphs
    for (unsigned i= 0; i < cGlyphPageSize; i++)
        page->setGlyphDataForIndex(i, glyphs[i], fontData);

    return true;
}

}