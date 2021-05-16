/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

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

#ifndef KCanvasImage_H
#define KCanvasImage_H
#if SVG_SUPPORT

#include <kcanvas/KCanvasResources.h>

namespace WebCore {
class Image;

class KCanvasImage : public KCanvasResource
{
public:
    KCanvasImage() { };
    virtual ~KCanvasImage() { };

    virtual void init(const Image &) = 0;
    virtual void init(IntSize) = 0;

    virtual IntSize size() = 0;
};

}

#endif // SVG_SUPPORT
#endif

