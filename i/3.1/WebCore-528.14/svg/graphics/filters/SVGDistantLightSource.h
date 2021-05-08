/*
    Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
                  2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGDistantLightSource_h
#define SVGDistantLightSource_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGLightSource.h"

namespace WebCore {

    class DistantLightSource : public LightSource {
    public:
        DistantLightSource(float azimuth, float elevation)
            : LightSource(LS_DISTANT)
            , m_azimuth(azimuth)
            , m_elevation(elevation)
        { }

        float azimuth() const { return m_azimuth; }
        float elevation() const { return m_elevation; }

        virtual TextStream& externalRepresentation(TextStream&) const;

    private:
        float m_azimuth;
        float m_elevation;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGDistantLightSource_h
