/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
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
#include "Font.h"

#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "SimpleFontData.h"

namespace WebCore {

void Font::drawGlyphs(GraphicsContext* context, const SimpleFontData* font, const GlyphBuffer& glyphBuffer,
                      int from, int numGlyphs, const FloatPoint& point) const
{
    cairo_t* cr = context->platformContext();
    cairo_save(cr);

    font->setFont(cr);

    GlyphBufferGlyph* glyphs = (GlyphBufferGlyph*)glyphBuffer.glyphs(from);

    float offset = point.x();
    for (int i = 0; i < numGlyphs; i++) {
        glyphs[i].x = offset;
        glyphs[i].y = point.y();
        offset += glyphBuffer.advanceAt(from + i);
    }

    Color fillColor = context->fillColor();

    // Text shadow, inspired by FontMac
    IntSize shadowSize;
    int shadowBlur = 0;
    Color shadowColor;
    bool hasShadow = context->textDrawingMode() == cTextFill &&
        context->getShadow(shadowSize, shadowBlur, shadowColor);

    // TODO: Blur support
    if (hasShadow) {
        // Disable graphics context shadows (not yet implemented) and paint them manually
        context->clearShadow();
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
        cairo_save(cr);

        float red, green, blue, alpha;
        shadowFillColor.getRGBA(red, green, blue, alpha);
        cairo_set_source_rgba(cr, red, green, blue, alpha);

        cairo_translate(cr, shadowSize.width(), shadowSize.height());
        cairo_show_glyphs(cr, glyphs, numGlyphs);

        cairo_restore(cr);
    }

    if (context->textDrawingMode() & cTextFill) {
        float red, green, blue, alpha;
        fillColor.getRGBA(red, green, blue, alpha);
        cairo_set_source_rgba(cr, red, green, blue, alpha);

        cairo_show_glyphs(cr, glyphs, numGlyphs);
    }

    if (context->textDrawingMode() & cTextStroke) {
        Color strokeColor = context->strokeColor();
        float red, green, blue, alpha;
        strokeColor.getRGBA(red, green, blue, alpha);
        cairo_set_source_rgba(cr, red, green, blue, alpha);       
        cairo_glyph_path(cr, glyphs, numGlyphs);
        cairo_set_line_width(cr, context->strokeThickness());
        cairo_stroke(cr);
    }

    // Re-enable the platform shadow we disabled earlier
    if (hasShadow)
        context->setShadow(shadowSize, shadowBlur, shadowColor);

    cairo_restore(cr);
}

}
