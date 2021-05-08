/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-6 Apple Computer, Inc.
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
 * along with this library; see the file COPYING.LIother.m_  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USm_
 *
 */

#ifndef FONT_DESCRIPTION_H
#define FONT_DESCRIPTION_H

#include "FontFamily.h"

namespace WebCore {

const unsigned cNormalWeight = 50;
const unsigned cBoldWeight = 63;

class FontDescription {
public:
    enum GenericFamilyType { NoFamily, StandardFamily, SerifFamily, SansSerifFamily, 
                             MonospaceFamily, CursiveFamily, FantasyFamily };

    FontDescription()
        : m_specifiedSize(0), m_computedSize(0), 
          m_italic(false), m_smallCaps(false), m_isAbsoluteSize(false), m_weight(cNormalWeight), 
          m_genericFamily(NoFamily), m_usePrinterFont(false)
          {}
    
    bool operator==(const FontDescription&) const;
    bool operator!=(const FontDescription& other) const { return !(*this == other); }
    
    const FontFamily& family() const { return m_familyList; }
    FontFamily& firstFamily() { return m_familyList; }
    float specifiedSize() const { return m_specifiedSize; }
    float computedSize() const { return m_computedSize; }
    bool italic() const { return m_italic; }
    bool bold() const { return weight() == cBoldWeight; }
    int computedPixelSize() const { return int(m_computedSize + 0.5f); }
    bool smallCaps() const { return m_smallCaps; }
    bool isAbsoluteSize() const { return m_isAbsoluteSize; }
    unsigned weight() const { return m_weight; }
    GenericFamilyType genericFamily() const { return static_cast<GenericFamilyType>(m_genericFamily); }
    bool usePrinterFont() const { return m_usePrinterFont; }

    void setFamily(const FontFamily& family) { m_familyList = family; }
    void setComputedSize(float s) { m_computedSize = s; }
    void setSpecifiedSize(float s) { m_specifiedSize = s; }
    void setItalic(bool i) { m_italic = i; }
    void setSmallCaps(bool c) { m_smallCaps = c; }
    void setIsAbsoluteSize(bool s) { m_isAbsoluteSize = s; }
    void setWeight(unsigned w) { m_weight = w; }
    void setGenericFamily(GenericFamilyType genericFamily) { m_genericFamily = genericFamily; }
    void setUsePrinterFont(bool p) { m_usePrinterFont = p; }

    bool equalForTextAutoSizing (const FontDescription& other) const {
        return m_familyList == other.m_familyList
            && m_specifiedSize == other.m_specifiedSize
            && m_smallCaps == other.m_smallCaps
            && m_isAbsoluteSize == other.m_isAbsoluteSize
            && m_genericFamily == other.m_genericFamily
            && m_usePrinterFont == other.m_usePrinterFont;
    }

private:
    FontFamily m_familyList; // The list of font families to be used.

    float m_specifiedSize;   // Specified CSS value. Independent of rendering issues such as integer
                             // rounding, minimum font sizes, and zooming.
    float m_computedSize;    // Computed size adjusted for the minimum font size and the zoom factor.  

    bool m_italic : 1;
    bool m_smallCaps : 1;
    bool m_isAbsoluteSize : 1;   // Whether or not CSS specified an explicit size
                                 // (logical sizes like "medium" don't count).
    unsigned m_weight : 8;
    unsigned m_genericFamily : 3; // GenericFamilyType
    bool m_usePrinterFont : 1;
};

inline bool FontDescription::operator==(const FontDescription& other) const
{
    return m_familyList == other.m_familyList
        && m_specifiedSize == other.m_specifiedSize
        && m_computedSize == other.m_computedSize
        && m_italic == other.m_italic
        && m_smallCaps == other.m_smallCaps
        && m_isAbsoluteSize == other.m_isAbsoluteSize
        && m_weight == other.m_weight
        && m_genericFamily == other.m_genericFamily
        && m_usePrinterFont == other.m_usePrinterFont;
}

}

#endif
