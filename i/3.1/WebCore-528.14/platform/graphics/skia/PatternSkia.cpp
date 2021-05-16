/*
 * Copyright (C) 2008 Google Inc. All rights reserved.
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
#include "Pattern.h"

#include "Image.h"
#include "NativeImageSkia.h"
#include "TransformationMatrix.h"

#include "SkShader.h"
#include "SkCanvas.h"

namespace WebCore {

PlatformPatternPtr Pattern::createPlatformPattern(const TransformationMatrix& patternTransform) const
{
    // Note: patternTransform is ignored since it seems to be applied elsewhere
    // (when the pattern is used?). Applying it to the pattern (i.e.
    // shader->setLocalMatrix) results in a double transformation. This can be
    // seen, for instance, as an extra offset in:
    // LayoutTests/fast/canvas/patternfill-repeat.html
    // and expanded scale and skew in:
    // LayoutTests/svg/W3C-SVG-1.1/pservers-grad-06-b.svg

    SkBitmap* bm = m_tileImage->nativeImageForCurrentFrame();
    if (m_repeatX && m_repeatY)
        return SkShader::CreateBitmapShader(*bm, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);

    // Skia does not have a "draw the tile only once" option. Clamp_TileMode
    // repeats the last line of the image after drawing one tile. To avoid
    // filling the space with arbitrary pixels, this workaround forces the
    // image to have a line of transparent pixels on the "repeated" edge(s),
    // thus causing extra space to be transparent filled.
    SkShader::TileMode tileModeX = m_repeatX ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
    SkShader::TileMode tileModeY = m_repeatY ? SkShader::kRepeat_TileMode : SkShader::kClamp_TileMode;
    int expandW = m_repeatX ? 0 : 1;
    int expandH = m_repeatY ? 0 : 1;

    // Create a transparent bitmap 1 pixel wider and/or taller than the
    // original, then copy the orignal into it.
    // FIXME: Is there a better way to pad (not scale) an image in skia?
    SkBitmap bm2;
    bm2.setConfig(bm->config(), bm->width() + expandW, bm->height() + expandH);
    bm2.allocPixels();
    bm2.eraseARGB(0x00, 0x00, 0x00, 0x00);
    SkCanvas canvas(bm2);
    canvas.drawBitmap(*bm, 0, 0);
    return SkShader::CreateBitmapShader(bm2, tileModeX, tileModeY);
}

} // namespace WebCore
