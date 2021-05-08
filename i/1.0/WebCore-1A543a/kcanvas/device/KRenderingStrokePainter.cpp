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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT
#include "KRenderingPaintServer.h"
#include "KRenderingStrokePainter.h"

namespace WebCore {

class KRenderingStrokePainter::Private
{
public:
    Private()
    {
        dirty = true;
        opacity = 1.0;
        miterLimit = 4;
        dashOffset = 0.0;
        strokeWidth = 1.0;
        capStyle = CAP_BUTT;
        joinStyle = JOIN_MITER;
    }

    bool dirty;
    float opacity;

    float strokeWidth;
    unsigned int miterLimit;

    KCCapStyle capStyle;
    KCJoinStyle joinStyle;

    float dashOffset;
    KCDashArray dashArray;
};

KRenderingStrokePainter::KRenderingStrokePainter() : d(new Private())
{
}

KRenderingStrokePainter::~KRenderingStrokePainter()
{
    delete d;
}

float KRenderingStrokePainter::strokeWidth() const
{
    return d->strokeWidth;
}

void KRenderingStrokePainter::setStrokeWidth(float width)
{
    setDirty();
    d->strokeWidth = width;
}

unsigned int KRenderingStrokePainter::strokeMiterLimit() const
{
    return d->miterLimit;
}

void KRenderingStrokePainter::setStrokeMiterLimit(unsigned int limit)
{
    setDirty();
    d->miterLimit = limit;
}

KCCapStyle KRenderingStrokePainter::strokeCapStyle() const
{
    return d->capStyle;
}

void KRenderingStrokePainter::setStrokeCapStyle(KCCapStyle style)
{
    setDirty();
    d->capStyle = style;
}

KCJoinStyle KRenderingStrokePainter::strokeJoinStyle() const
{
    return d->joinStyle;
}

void KRenderingStrokePainter::setStrokeJoinStyle(KCJoinStyle style)
{
    setDirty();
    d->joinStyle = style;
}

float KRenderingStrokePainter::dashOffset() const
{
    return d->dashOffset;
}

void KRenderingStrokePainter::setDashOffset(float offset)
{
    setDirty();
    d->dashOffset = offset;
}

KCDashArray &KRenderingStrokePainter::dashArray() const
{
    return d->dashArray;
}

void KRenderingStrokePainter::setDashArray(const KCDashArray &dashArray)
{
    setDirty();
    d->dashArray = dashArray;
}

float KRenderingStrokePainter::opacity() const
{
    return d->opacity;
}

void KRenderingStrokePainter::setOpacity(float opacity)
{
    setDirty();
    d->opacity = opacity;
}

bool KRenderingStrokePainter::dirty() const
{
    return d->dirty;
}

void KRenderingStrokePainter::setDirty(bool dirty)
{
    d->dirty = dirty;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

