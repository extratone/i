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

#ifndef KSVG_SVGAnimatedIntegerImpl_H
#define KSVG_SVGAnimatedIntegerImpl_H
#if SVG_SUPPORT

#include "Shared.h"

namespace WebCore
{
    class SVGStyledElement;

    class SVGAnimatedInteger : public Shared<SVGAnimatedInteger>
    {
    public:
        SVGAnimatedInteger(const SVGStyledElement *context);
        virtual ~SVGAnimatedInteger();

        long baseVal() const;
        void setBaseVal(long baseVal);

        long animVal() const;
        void setAnimVal(long animVal);

    private:
        long m_baseVal;
        long m_animVal;

        const SVGStyledElement *m_context;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
