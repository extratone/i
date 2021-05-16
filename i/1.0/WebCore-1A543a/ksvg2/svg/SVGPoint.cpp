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
#include "SVGPoint.h"

#include "SVGMatrix.h"
#include "SVGStyledElement.h"
#include "IntPoint.h"

using namespace WebCore;

SVGPoint::SVGPoint(const SVGStyledElement *context)
{
    m_x = 0.0;
    m_y = 0.0;
    m_context = context;
}

SVGPoint::SVGPoint(float x, float y, const SVGStyledElement *context)
{
    m_x = x;
    m_y = y;
    m_context = context;
}

SVGPoint::SVGPoint(const IntPoint &p, const SVGStyledElement *context)
{
    m_x = p.x();
    m_y = p.y();
    m_context = context;
}

SVGPoint::~SVGPoint()
{
}

float SVGPoint::x() const
{
    return m_x;
}

float SVGPoint::y() const
{
    return m_y;
}

void SVGPoint::setX(float x)
{
    m_x = x;

    if(m_context)
        m_context->notifyAttributeChange();
}

void SVGPoint::setY(float y)
{
    m_y = y;

    if(m_context)
        m_context->notifyAttributeChange();
}

SVGPoint *SVGPoint::matrixTransform(SVGMatrix * /* matrix */)
{
    // TODO: implement me!
    return 0;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

