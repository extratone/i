/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef FONTDATA_H
#define FONTDATA_H

#include "FontPlatformData.h"
#include "GlyphMap.h"
#include "GlyphWidthMap.h"
#include <wtf/Noncopyable.h>

// FIXME: Temporary. Only needed to support API that's going to move.
#include <unicode/umachine.h>

namespace WebCore {

class FontDescription;
class FontPlatformData;
class WidthMap;

enum Pitch { UnknownPitch, FixedPitch, VariablePitch };

class FontData : Noncopyable
{
public:
    FontData(const FontPlatformData& f);
    ~FontData();

public:
    const FontPlatformData& platformData() const { return m_font; }
    FontData* smallCapsFontData(const FontDescription& fontDescription) const;

    // vertical metrics
    int ascent() const { return GSFontGetAscent(m_font.font); }
    int descent() const { return GSFontGetDescent(m_font.font); }
    int lineSpacing() const { return GSFontGetLineSpacing(m_font.font); }
    int lineGap() const { return GSFontGetLineGap(m_font.font); }
    float xHeight() const { return GSFontGetXHeight(m_font.font); }

    float widthForGlyph(Glyph) const;
    float platformWidthForGlyph(Glyph) const;

    bool containsCharacters(const UChar* characters, int length) const;

    void determinePitch();
    Pitch pitch() const { return m_treatAsFixedPitch ? FixedPitch : VariablePitch; }
    
    // FIXME: Should go away when we pull the glyph map out of the FontData.
    GlyphData glyphDataForCharacter(UChar32 c) const { return m_characterToGlyphMap.glyphDataForCharacter(c, this); }
    void setGlyphDataForCharacter(UChar32 c, Glyph glyph, const FontData* fontData) const { m_characterToGlyphMap.setGlyphDataForCharacter(c, glyph, fontData); }

#if __APPLE__
	GSFontRef getFont() const { return m_font.font; }
#endif

#if PLATFORM(WIN)
    void setIsMLangFont() { m_isMLangFont = true; }
#endif

#if PLATFORM(GDK)
    void setFont(cairo_t*) const;
    Glyph getGlyphIndex(UChar c) const { return m_font.index(c); }
#endif

private:
    void platformInit();
    void platformDestroy();

public:
    
    FontPlatformData m_font;
    mutable GlyphMap m_characterToGlyphMap;
    mutable GlyphWidthMap m_glyphToWidthMap;

    bool m_treatAsFixedPitch;
    CGGlyph m_spaceGlyph;
    float m_spaceWidth;
    float m_adjustedSpaceWidth;

    mutable FontData* m_smallCapsFontData;

#if __APPLE__
    GSFontRef m_smallCapsFont;
    float m_syntheticBoldOffset;
#endif

#if PLATFORM(WIN)
    bool m_isMLangFont;
#endif
};

}

#endif
