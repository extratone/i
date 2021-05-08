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
#include "Attr.h"

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGLineElement.h"
#include "SVGAnimatedLength.h"

#include <kcanvas/KCanvasCreator.h>

using namespace WebCore;

SVGLineElement::SVGLineElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired()
{
}

SVGLineElement::~SVGLineElement()
{
}

SVGAnimatedLength *SVGLineElement::x1() const
{
    return lazy_create<SVGAnimatedLength>(m_x1, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGLineElement::y1() const
{
    return lazy_create<SVGAnimatedLength>(m_y1, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLength *SVGLineElement::x2() const
{
    return lazy_create<SVGAnimatedLength>(m_x2, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGLineElement::y2() const
{
    return lazy_create<SVGAnimatedLength>(m_y2, this, LM_HEIGHT, viewportElement());
}

void SVGLineElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::x1Attr)
        x1()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::y1Attr)
        y1()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::x2Attr)
        x2()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::y2Attr)
        y2()->baseVal()->setValueAsString(value.impl());
    else
    {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

KCanvasPath* SVGLineElement::toPathData() const
{
    float _x1 = x1()->baseVal()->value(), _y1 = y1()->baseVal()->value();
    float _x2 = x2()->baseVal()->value(), _y2 = y2()->baseVal()->value();

    return KCanvasCreator::self()->createLine(_x1, _y1, _x2, _y2);
}

const SVGStyledElement *SVGLineElement::pushAttributeContext(const SVGStyledElement *context)
{
    // All attribute's contexts are equal (so just take the one from 'x1').
    const SVGStyledElement *restore = x1()->baseVal()->context();

    x1()->baseVal()->setContext(context);
    y1()->baseVal()->setContext(context);
    x2()->baseVal()->setContext(context);
    y2()->baseVal()->setContext(context);
    
    SVGStyledElement::pushAttributeContext(context);
    return restore;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

