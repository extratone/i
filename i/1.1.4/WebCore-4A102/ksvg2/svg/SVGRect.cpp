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
#include "SVGHelper.h"
#include "SVGRect.h"
#include "SVGStyledElement.h"

using namespace WebCore;

SVGRect::SVGRect(const SVGStyledElement *context) : Shared<SVGRect>()
{
    m_context = context;
    m_x = m_y = m_width = m_height = 0.0;
}

SVGRect::~SVGRect()
{
}

double SVGRect::x() const
{
    return m_x;
}

void SVGRect::setX(double value)
{
    m_x = value;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGRect::y() const
{
    return m_y;
}

void SVGRect::setY(double value)
{
    m_y = value;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGRect::width() const
{
    return m_width;
}

void SVGRect::setWidth(double value)
{
    m_width = value;

    if(m_context)
        m_context->notifyAttributeChange();
}

double SVGRect::height() const
{
    return m_height;
}

void SVGRect::setHeight(double value)
{
    m_height = value;

    if(m_context)
        m_context->notifyAttributeChange();
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

