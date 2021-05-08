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

#ifndef SVGUnitTypes_h
#define SVGUnitTypes_h

#if ENABLE(SVG)

#include <wtf/RefCounted.h>

namespace WebCore {

class SVGUnitTypes : public RefCounted<SVGUnitTypes> {
public:
    enum SVGUnitType {
        SVG_UNIT_TYPE_UNKNOWN               = 0,
        SVG_UNIT_TYPE_USERSPACEONUSE        = 1,
        SVG_UNIT_TYPE_OBJECTBOUNDINGBOX     = 2
    };

private:
    SVGUnitTypes() { }
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGUnitTypes_h

// vim:ts=4:noet
