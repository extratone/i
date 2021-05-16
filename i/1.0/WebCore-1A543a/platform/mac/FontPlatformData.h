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

#ifndef FontPlatformData_H
#define FontPlatformData_H

#include "StringImpl.h"

#import <GraphicsServices/GSFont.h>

#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif

#include <CoreFoundation/CFBase.h>
#include <objc/objc-auto.h>

namespace WebCore {

struct FontPlatformData {
    class Deleted {};

    FontPlatformData(Deleted)
	: font((GSFontRef)-1), syntheticBold(false), syntheticOblique(false)
    {}

	FontPlatformData(GSFontRef f = 0, bool b = false, bool o = false)
    : font(f), syntheticBold(b), syntheticOblique(o)
    {
        if (f) CFRetain(f); // fix for <rdar://problems/4223619>
    }

    FontPlatformData(const FontPlatformData& f)
    {
        font = (f.font && f.font != (GSFontRef)-1) ? (GSFontRef)CFRetain(f.font) : f.font;
        syntheticBold = f.syntheticBold;
        syntheticOblique = f.syntheticOblique;
    }

    ~FontPlatformData()
    {
        if (font && font != (GSFontRef)-1) CFRelease(font);
    }

	GSFontRef font;
    bool syntheticBold;
    bool syntheticOblique;

    unsigned hash() const
    { 
        uintptr_t hashCodes[2] = { (uintptr_t)font, syntheticBold << 1 | syntheticOblique };
        return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), sizeof(hashCodes) / sizeof(UChar));
    }

    bool operator==(const FontPlatformData& other) const
    { 
        return font == other.font && syntheticBold == other.syntheticBold && syntheticOblique == other.syntheticOblique;
    }
};

}

#endif
