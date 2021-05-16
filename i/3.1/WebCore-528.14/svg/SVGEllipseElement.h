/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#ifndef SVGEllipseElement_h
#define SVGEllipseElement_h

#if ENABLE(SVG)
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"

namespace WebCore {

    class SVGEllipseElement : public SVGStyledTransformableElement,
                              public SVGTests,
                              public SVGLangSpace,
                              public SVGExternalResourcesRequired {
    public:
        SVGEllipseElement(const QualifiedName&, Document*);
        virtual ~SVGEllipseElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void svgAttributeChanged(const QualifiedName&);

        virtual Path toPathData() const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }
        virtual bool hasRelativeValues() const;

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGEllipseElement, SVGNames::ellipseTagString, SVGNames::cxAttrString, SVGLength, Cx, cx)
        ANIMATED_PROPERTY_DECLARATIONS(SVGEllipseElement, SVGNames::ellipseTagString, SVGNames::cyAttrString, SVGLength, Cy, cy)
        ANIMATED_PROPERTY_DECLARATIONS(SVGEllipseElement, SVGNames::ellipseTagString, SVGNames::rxAttrString, SVGLength, Rx, rx)
        ANIMATED_PROPERTY_DECLARATIONS(SVGEllipseElement, SVGNames::ellipseTagString, SVGNames::ryAttrString, SVGLength, Ry, ry)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
