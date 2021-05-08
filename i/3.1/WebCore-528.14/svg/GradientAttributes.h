/*
    Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef GradientAttributes_h
#define GradientAttributes_h

#if ENABLE(SVG)

namespace WebCore
{
    struct GradientAttributes {
        GradientAttributes()
            : m_spreadMethod(SpreadMethodPad)
            , m_boundingBoxMode(true)
            , m_spreadMethodSet(false)
            , m_boundingBoxModeSet(false)
            , m_gradientTransformSet(false)
            , m_stopsSet(false)
        {
        }

        GradientSpreadMethod spreadMethod() const { return m_spreadMethod; }
        bool boundingBoxMode() const { return m_boundingBoxMode; }
        TransformationMatrix gradientTransform() const { return m_gradientTransform; }
        const Vector<SVGGradientStop>& stops() const { return m_stops; }

        void setSpreadMethod(GradientSpreadMethod value) { m_spreadMethod = value; m_spreadMethodSet = true; }
        void setBoundingBoxMode(bool value) { m_boundingBoxMode = value; m_boundingBoxModeSet = true; }
        void setGradientTransform(const TransformationMatrix& value) { m_gradientTransform = value; m_gradientTransformSet = true; }
        void setStops(const Vector<SVGGradientStop>& value) { m_stops = value; m_stopsSet = true; } 

        bool hasSpreadMethod() const { return m_spreadMethodSet; }
        bool hasBoundingBoxMode() const { return m_boundingBoxModeSet; }
        bool hasGradientTransform() const { return m_gradientTransformSet; }
        bool hasStops() const { return m_stopsSet; }

    private:
        // Properties
        GradientSpreadMethod m_spreadMethod;
        bool m_boundingBoxMode;
        TransformationMatrix m_gradientTransform;
        Vector<SVGGradientStop> m_stops;

        // Property states
        bool m_spreadMethodSet : 1;
        bool m_boundingBoxModeSet : 1;
        bool m_gradientTransformSet : 1;
        bool m_stopsSet : 1;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
