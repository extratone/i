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

#ifndef SVGImageElement_h
#define SVGImageElement_h

#if ENABLE(SVG)
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGImageLoader.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"
#include "SVGPreserveAspectRatio.h"

namespace WebCore {

    class SVGLength;

    class SVGImageElement : public SVGStyledTransformableElement,
                            public SVGTests,
                            public SVGLangSpace,
                            public SVGExternalResourcesRequired,
                            public SVGURIReference {
    public:
        SVGImageElement(const QualifiedName&, Document*);
        virtual ~SVGImageElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void svgAttributeChanged(const QualifiedName&);

        virtual void attach();
        virtual void insertedIntoDocument();

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
 
        virtual const QualifiedName& imageSourceAttributeName() const;       
        virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    protected:
        virtual bool haveLoadedRequiredResources();
        virtual bool hasRelativeValues() const;
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGImageElement, SVGNames::imageTagString, SVGNames::xAttrString, SVGLength, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGImageElement, SVGNames::imageTagString, SVGNames::yAttrString, SVGLength, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGImageElement, SVGNames::imageTagString, SVGNames::widthAttrString, SVGLength, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGImageElement, SVGNames::imageTagString, SVGNames::heightAttrString, SVGLength, Height, height)
        ANIMATED_PROPERTY_DECLARATIONS(SVGImageElement, SVGNames::imageTagString, SVGNames::preserveAspectRatioAttrString, SVGPreserveAspectRatio, PreserveAspectRatio, preserveAspectRatio)

        SVGImageLoader m_imageLoader;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
