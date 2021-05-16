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
#include "SVGZoomEvent.h"

using namespace WebCore;

SVGZoomEvent::SVGZoomEvent()
    : m_newScale(0.0)
    , m_previousScale(0.0)
{
}

SVGZoomEvent::~SVGZoomEvent()
{
}

FloatRect SVGZoomEvent::zoomRectScreen() const
{
    return m_zoomRectScreen;
}

float SVGZoomEvent::previousScale() const
{
    return m_previousScale;
}

void SVGZoomEvent::setPreviousScale(float scale)
{
    m_previousScale = scale;
}

FloatPoint SVGZoomEvent::previousTranslate() const
{
    return m_previousTranslate;
}

float SVGZoomEvent::newScale() const
{
    return m_newScale;
}

void SVGZoomEvent::setNewScale(float scale)
{
    m_newScale = scale;
}

FloatPoint SVGZoomEvent::newTranslate() const
{
    return m_newTranslate;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

