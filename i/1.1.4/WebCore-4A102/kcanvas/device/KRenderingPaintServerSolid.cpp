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
#include "KRenderingPaintServerSolid.h"
#include "TextStream.h"
#include "KCanvasTreeDebug.h"

namespace WebCore {

class KRenderingPaintServerSolid::Private
{
public:
    Private() { }
    ~Private() { }

    Color color;
};

KRenderingPaintServerSolid::KRenderingPaintServerSolid() : KRenderingPaintServer(), d(new Private())
{
}

KRenderingPaintServerSolid::~KRenderingPaintServerSolid()
{
    delete d;
}

Color KRenderingPaintServerSolid::color() const
{
    return d->color;
}

void KRenderingPaintServerSolid::setColor(const Color &color)
{
    d->color = color;
}

KCPaintServerType KRenderingPaintServerSolid::type() const
{
    return PS_SOLID;
}

TextStream &KRenderingPaintServerSolid::externalRepresentation(TextStream &ts) const
{
    ts << "[type=SOLID]"
        << " [color="<< color() << "]";
    return ts;
}

TextStream& operator<<(TextStream& ts, const KRenderingPaintServer& ps)
{
    return ps.externalRepresentation(ts);
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

