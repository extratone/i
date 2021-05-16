/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGColorImpl_H
#define KSVG_SVGColorImpl_H
#if SVG_SUPPORT

#include "CSSValue.h"
#include "Color.h"
#include "PlatformString.h"

namespace WebCore
{
    class RGBColor;
    typedef int ExceptionCode;
    
    class SVGColor : public CSSValue
    {
    public:
        SVGColor();
        SVGColor(const String& rgbColor);
        SVGColor(unsigned short colorType);
        virtual ~SVGColor();
        
        enum SVGColorType {
            SVG_COLORTYPE_UNKNOWN                   = 0,
            SVG_COLORTYPE_RGBCOLOR                  = 1,
            SVG_COLORTYPE_RGBCOLOR_ICCCOLOR         = 2,
            SVG_COLORTYPE_CURRENTCOLOR              = 3
        };

        // 'SVGColor' functions
        unsigned short colorType() const;

        unsigned rgbColor() const;

        void setRGBColor(const String& rgbColor) { ExceptionCode ignored = 0; setRGBColor(rgbColor, ignored); }
        void setRGBColor(const String& rgbColor, ExceptionCode&);
        void setRGBColorICCColor(const String& rgbColor, const String& iccColor, ExceptionCode&);
        void setColor(unsigned short colorType, const String& rgbColor, const String& iccColor, ExceptionCode&);

        virtual String cssText() const;

        // Helpers
        const Color& color() const;

    private:
        Color m_color;
        unsigned short m_colorType;
        String m_rgbColor;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
