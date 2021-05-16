/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGFEComposite_h
#define SVGFEComposite_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "FilterEffect.h"
#include "PlatformString.h"

namespace WebCore {

    enum CompositeOperationType {
        FECOMPOSITE_OPERATOR_UNKNOWN    = 0, 
        FECOMPOSITE_OPERATOR_OVER       = 1,
        FECOMPOSITE_OPERATOR_IN         = 2,
        FECOMPOSITE_OPERATOR_OUT        = 3,
        FECOMPOSITE_OPERATOR_ATOP       = 4,
        FECOMPOSITE_OPERATOR_XOR        = 5,
        FECOMPOSITE_OPERATOR_ARITHMETIC = 6
    };

    class FEComposite : public FilterEffect {
    public:
        static PassRefPtr<FEComposite> create(FilterEffect*, FilterEffect*, const CompositeOperationType&,
                const float&, const float&, const float&, const float&);

        CompositeOperationType operation() const;
        void setOperation(CompositeOperationType);

        float k1() const;
        void setK1(float);

        float k2() const;
        void setK2(float);

        float k3() const;
        void setK3(float);

        float k4() const;
        void setK4(float);
        
        virtual void apply();
        virtual void dump();

    private:
        FEComposite(FilterEffect*, FilterEffect*, const CompositeOperationType&,
                const float&, const float&, const float&, const float&);

        RefPtr<FilterEffect> m_in;
        RefPtr<FilterEffect> m_in2;
        CompositeOperationType m_type;
        float m_k1;
        float m_k2;
        float m_k3;
        float m_k4;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFEComposite_h
