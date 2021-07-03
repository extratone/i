/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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
 */

#ifndef SVGPathSegLinetoHorizontal_h
#define SVGPathSegLinetoHorizontal_h

#if ENABLE(SVG)
#include "SVGPathSegWithContext.h"

namespace WebCore {

class SVGPathSegLinetoHorizontal : public SVGPathSegWithContext {
public:
    SVGPathSegLinetoHorizontal(SVGPathElement* element, SVGPathSegRole role, float x)
        : SVGPathSegWithContext(element, role)
        , m_x(x)
    {
    }

    float x() const { return m_x; }
    void setX(float x)
    {
        m_x = x;
        commitChange();
    }

private:
    float m_x;
};

class SVGPathSegLinetoHorizontalAbs : public SVGPathSegLinetoHorizontal {
public:
    static PassRefPtr<SVGPathSegLinetoHorizontalAbs> create(SVGPathElement* element, SVGPathSegRole role, float x)
    {
        return adoptRef(new SVGPathSegLinetoHorizontalAbs(element, role, x));
    }

private:
    SVGPathSegLinetoHorizontalAbs(SVGPathElement* element, SVGPathSegRole role, float x)
        : SVGPathSegLinetoHorizontal(element, role, x)
    {
    }

    virtual unsigned short pathSegType() const { return PATHSEG_LINETO_HORIZONTAL_ABS; }
    virtual String pathSegTypeAsLetter() const { return "H"; }
};

class SVGPathSegLinetoHorizontalRel : public SVGPathSegLinetoHorizontal {
public:
    static PassRefPtr<SVGPathSegLinetoHorizontalRel> create(SVGPathElement* element, SVGPathSegRole role, float x)
    {
        return adoptRef(new SVGPathSegLinetoHorizontalRel(element, role, x));
    }

private:
    SVGPathSegLinetoHorizontalRel(SVGPathElement* element, SVGPathSegRole role, float x)
        : SVGPathSegLinetoHorizontal(element, role, x)
    {
    }

    virtual unsigned short pathSegType() const { return PATHSEG_LINETO_HORIZONTAL_REL; }
    virtual String pathSegTypeAsLetter() const { return "h"; }
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
