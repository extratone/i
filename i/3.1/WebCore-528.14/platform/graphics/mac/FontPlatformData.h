/*
 * This file is part of the internal font implementation.
 * It should not be included by source files outside it.
 *
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef FontPlatformData_h
#define FontPlatformData_h

#include "StringImpl.h"

#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif

#import <GraphicsServices/GSFont.h>

typedef struct CGFont* CGFontRef;
typedef UInt32 ATSUFontID;
#ifndef BUILDING_ON_TIGER
typedef const struct __CTFont* CTFontRef;
#endif

#include <CoreFoundation/CFBase.h>
#include <objc/objc-auto.h>
#include <wtf/RetainPtr.h>

namespace WebCore {


struct FontPlatformData {
    FontPlatformData(float size, bool syntheticBold, bool syntheticOblique)
        : m_syntheticBold(syntheticBold)
        , m_syntheticOblique(syntheticOblique)
        , m_gsFont(0)
        , m_isImageFont(false)
        , m_size(size)
        , m_font(0)
    {
    }

    FontPlatformData(GSFontRef = 0, bool syntheticBold = false, bool syntheticOblique = false);
    
    FontPlatformData(GSFontRef f, float s, bool b , bool o)
        : m_syntheticBold(b), m_syntheticOblique(o), m_gsFont(f), m_isImageFont(false), m_size(s), m_font(0)
    {
    }

    FontPlatformData(const FontPlatformData&);
    
    ~FontPlatformData();

    FontPlatformData(WTF::HashTableDeletedValueType) : m_font(hashTableDeletedFontValue()) { }
    bool isHashTableDeletedValue() const { return m_font == hashTableDeletedFontValue(); }

    float size() const { return m_size; }

    bool m_syntheticBold;
    bool m_syntheticOblique;

    GSFontRef m_gsFont;
    bool m_isImageFont;
    float m_size;

    unsigned hash() const
    {
        ASSERT(m_font != 0 || m_gsFont == 0 || m_isImageFont != 0);
        uintptr_t hashCodes[2] = { (uintptr_t)m_font, m_isImageFont << 2 | m_syntheticBold << 1 | m_syntheticOblique };
        return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), sizeof(hashCodes) / sizeof(UChar));
    }

    const FontPlatformData& operator=(const FontPlatformData& f);

    bool operator==(const FontPlatformData& other) const
    { 
        return m_font == other.m_font && m_syntheticBold == other.m_syntheticBold && m_syntheticOblique == other.m_syntheticOblique && 
               m_gsFont == other.m_gsFont && m_size == other.m_size && m_isImageFont == other.m_isImageFont;
    }

    GSFontRef font() const { return m_font; }
    void setFont(GSFontRef font);

    bool roundsGlyphAdvances() const { return false; }
#if USE(CORE_TEXT)
    bool allowsLigatures() const;
#else
    bool allowsLigatures() const { return false; }
#endif


private:
    static GSFontRef hashTableDeletedFontValue() { return reinterpret_cast<GSFontRef>(-1); }

    GSFontRef m_font;
};

}

#endif
