/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef HTMLGradient_h
#define HTMLGradient_h

#include "FloatPoint.h"
#include "Shared.h"
#include <wtf/Vector.h>

#if __APPLE__
typedef struct CGShading* CGShadingRef;
#endif

namespace WebCore {

    class String;

    class CanvasGradient : public Shared<CanvasGradient> {
    public:
        CanvasGradient(const FloatPoint& p0, const FloatPoint& p1);
        CanvasGradient(const FloatPoint& p0, float r0, const FloatPoint& p1, float r1);
        ~CanvasGradient();

        void addColorStop(float, const String& color);

        void getColor(float value, float* r, float* g, float* b, float* a);

#if __APPLE__
        CGShadingRef platformShading();
#endif

        struct ColorStop {
            float stop;
            float red;
            float green;
            float blue;
            float alpha;
            
            ColorStop() : stop(0), red(0), green(0), blue(0), alpha(0) { }
            ColorStop(float s, float r, float g, float b, float a) : stop(s), red(r), green(g), blue(b), alpha(a) { }
        };

    private:
        int findStop(float value) const;

        bool m_radial;
        FloatPoint m_p0, m_p1;
        float m_r0, m_r1;
        mutable Vector<ColorStop> m_stops;
        mutable bool m_stopsSorted;
        mutable int m_lastStop;

#if __APPLE__
        CGShadingRef m_shading;
#endif
    };

} //namespace

#endif
