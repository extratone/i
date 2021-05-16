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

#include "config.h"
#if SVG_SUPPORT
#include "DeprecatedStringList.h"

#include "Attr.h"

#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFETurbulenceElement.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedInteger.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGFETurbulenceElement::SVGFETurbulenceElement(const QualifiedName& tagName, Document *doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFETurbulenceElement::~SVGFETurbulenceElement()
{
    delete m_filterEffect;
}

SVGAnimatedNumber *SVGFETurbulenceElement::baseFrequencyX() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_baseFrequencyX, dummy);
}

SVGAnimatedNumber *SVGFETurbulenceElement::baseFrequencyY() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_baseFrequencyY, dummy);
}

SVGAnimatedNumber *SVGFETurbulenceElement::seed() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_seed, dummy);
}

SVGAnimatedInteger *SVGFETurbulenceElement::numOctaves() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedInteger>(m_numOctaves, dummy);
}

SVGAnimatedEnumeration *SVGFETurbulenceElement::stitchTiles() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedEnumeration>(m_stitchTiles, dummy);
}

SVGAnimatedEnumeration *SVGFETurbulenceElement::type() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedEnumeration>(m_type, dummy);
}

void SVGFETurbulenceElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::typeAttr)
    {
        if(value == "fractalNoise")
            type()->setBaseVal(SVG_TURBULENCE_TYPE_FRACTALNOISE);
        else if(value == "turbulence")
            type()->setBaseVal(SVG_TURBULENCE_TYPE_TURBULENCE);
    }
    else if (attr->name() == SVGNames::stitchTilesAttr)
    {
        if(value == "stitch")
            stitchTiles()->setBaseVal(SVG_STITCHTYPE_STITCH);
        else if(value == "nostitch")
            stitchTiles()->setBaseVal(SVG_STITCHTYPE_NOSTITCH);
    }
    else if (attr->name() == SVGNames::baseFrequencyAttr)
    {
        DeprecatedStringList numbers = DeprecatedStringList::split(' ', value.deprecatedString());
        baseFrequencyX()->setBaseVal(numbers[0].toDouble());
        if(numbers.count() == 1)
            baseFrequencyY()->setBaseVal(numbers[0].toDouble());
        else
            baseFrequencyY()->setBaseVal(numbers[1].toDouble());
    }
    else if (attr->name() == SVGNames::seedAttr)
        seed()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::numOctavesAttr)
        numOctaves()->setBaseVal(value.deprecatedString().toUInt());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

KCanvasFETurbulence *SVGFETurbulenceElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFETurbulence *>(renderingDevice()->createFilterEffect(FE_TURBULENCE));
    if (!m_filterEffect)
        return 0;
    
    m_filterEffect->setType((KCTurbulanceType)(type()->baseVal() - 1));
    setStandardAttributes(m_filterEffect);
    m_filterEffect->setBaseFrequencyX(baseFrequencyX()->baseVal());
    m_filterEffect->setBaseFrequencyY(baseFrequencyY()->baseVal());
    m_filterEffect->setNumOctaves(numOctaves()->baseVal());
    m_filterEffect->setSeed(seed()->baseVal());
    m_filterEffect->setStitchTiles(stitchTiles()->baseVal() == SVG_STITCHTYPE_STITCH);
    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

