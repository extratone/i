/*
 * Copyright (C) 2005, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
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
#include "SimpleFontData.h"

#include "Font.h"
#include "FontCache.h"
#if ENABLE(SVG_FONTS)
#include "SVGFontData.h"
#include "SVGFontFaceElement.h"
#endif

#include <wtf/MathExtras.h>
#include <wtf/UnusedParam.h>

namespace WebCore {
    
SimpleFontData::SimpleFontData(const FontPlatformData& f, bool customFont, bool loading, SVGFontData* svgFontData)
    : m_unitsPerEm(defaultUnitsPerEm)
    , m_font(f)
    , m_treatAsFixedPitch(false)
#if ENABLE(SVG_FONTS)
    , m_svgFontData(svgFontData)
#endif
    , m_isCustomFont(customFont)
    , m_isLoading(loading)
    , m_smallCapsFontData(0)
{
#if !ENABLE(SVG_FONTS)
    UNUSED_PARAM(svgFontData);
#else
    if (SVGFontFaceElement* svgFontFaceElement = svgFontData ? svgFontData->svgFontFaceElement() : 0) {
       m_unitsPerEm = svgFontFaceElement->unitsPerEm();

       double scale = f.size();
       if (m_unitsPerEm)
           scale /= m_unitsPerEm;

        m_ascent = static_cast<int>(svgFontFaceElement->ascent() * scale);
        m_descent = static_cast<int>(svgFontFaceElement->descent() * scale);
        m_xHeight = static_cast<int>(svgFontFaceElement->xHeight() * scale);
        m_lineGap = 0.1f * f.size();
        m_lineSpacing = m_ascent + m_descent + m_lineGap;
    
        m_spaceGlyph = 0;
        m_spaceWidth = 0;
        m_adjustedSpaceWidth = 0;
        determinePitch();
        m_missingGlyphData.fontData = this;
        m_missingGlyphData.glyph = 0;
        return;
    }
#endif
    if (f.m_isImageFont) {
        m_unitsPerEm = 1;
        
        double scale = f.size();
        if (m_unitsPerEm)
            scale /= m_unitsPerEm;
        
        m_ascent = static_cast<int>(scale / 3);  // should not be used for image glyphs
        m_descent = static_cast<int>(scale / 3); // should not be used for image glyphs
        m_xHeight = static_cast<int>(scale / 3); // should not be used for image glyphs
        m_lineGap = static_cast<int>(scale / 3); // should not be used for image glyphs
        m_lineSpacing = 0; // should not be used for image glyphs
        
        m_spaceGlyph = 0;
        m_spaceWidth = 0;
        m_adjustedSpaceWidth = 0;
        m_missingGlyphData.fontData = this;
        m_missingGlyphData.glyph = 0;
        return;
    }
    platformInit();
    platformGlyphInit();
}

#if !PLATFORM(QT)
void SimpleFontData::platformGlyphInit()
{
    GlyphPage* glyphPageZero = GlyphPageTreeNode::getRootChild(this, 0)->page();
    if (!glyphPageZero) {
        LOG_ERROR("Failed to get glyph page zero.");
        m_spaceGlyph = 0;
        m_spaceWidth = 0;
        m_adjustedSpaceWidth = 0;
        determinePitch();
        m_missingGlyphData.fontData = this;
        m_missingGlyphData.glyph = 0;
        return;
    }

    // Nasty hack to determine if we should round or ceil space widths.
    // If the font is monospace or fake monospace we ceil to ensure that 
    // every character and the space are the same width.  Otherwise we round.
    m_spaceGlyph = glyphPageZero->glyphDataForCharacter(' ').glyph;
    float width = widthForGlyph(m_spaceGlyph);
    m_spaceWidth = width;
    determinePitch();
    m_adjustedSpaceWidth = m_treatAsFixedPitch ? ceilf(width) : roundf(width);

    // Force the glyph for ZERO WIDTH SPACE to have zero width, unless it is shared with SPACE.
    // Helvetica is an example of a non-zero width ZERO WIDTH SPACE glyph.
    // See <http://bugs.webkit.org/show_bug.cgi?id=13178>
    // Ask for the glyph for 0 to avoid paging in ZERO WIDTH SPACE. Control characters, including 0,
    // are mapped to the ZERO WIDTH SPACE glyph.
    Glyph zeroWidthSpaceGlyph = glyphPageZero->glyphDataForCharacter(0).glyph;
    if (zeroWidthSpaceGlyph) {
        if (zeroWidthSpaceGlyph != m_spaceGlyph)
            m_glyphToWidthMap.setWidthForGlyph(zeroWidthSpaceGlyph, 0);
        else
            LOG_ERROR("Font maps SPACE and ZERO WIDTH SPACE to the same glyph. Glyph width not overridden.");
    }

    m_missingGlyphData.fontData = this;
    m_missingGlyphData.glyph = 0;
}
#endif

SimpleFontData::~SimpleFontData()
{
#if ENABLE(SVG_FONTS)
    if (!m_svgFontData || !m_svgFontData->svgFontFaceElement())
#endif
        platformDestroy();

    if (!isCustomFont()) {
        if (m_smallCapsFontData)
            fontCache()->releaseFontData(m_smallCapsFontData);
        GlyphPageTreeNode::pruneTreeFontData(this);
    }
}

#if !PLATFORM(QT)
float SimpleFontData::widthForGlyph(Glyph glyph) const
{
    float width = m_glyphToWidthMap.widthForGlyph(glyph);
    if (width != cGlyphWidthUnknown)
        return width;
    
    width = platformWidthForGlyph(glyph);
    m_glyphToWidthMap.setWidthForGlyph(glyph, width);
    
    return width;
}
#endif

const SimpleFontData* SimpleFontData::fontDataForCharacter(UChar32) const
{
    return this;
}

bool SimpleFontData::isSegmented() const
{
    return false;
}

} // namespace WebCore
