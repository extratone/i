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

#include "config.h"
#include "CanvasGradient.h"

#include "cssparser.h"

#if __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace WebCore {

CanvasGradient::CanvasGradient(const FloatPoint& p0, const FloatPoint& p1)
    : m_radial(false), m_p0(p0), m_p1(p1), m_stopsSorted(false), m_lastStop(0)
#if __APPLE__
    , m_shading(0)
#endif
{
}

CanvasGradient::CanvasGradient(const FloatPoint& p0, float r0, const FloatPoint& p1, float r1)
    : m_radial(true), m_p0(p0), m_p1(p1), m_r0(r0), m_r1(r1), m_stopsSorted(false), m_lastStop(0)
#if __APPLE__
    , m_shading(0)
#endif
{
}

CanvasGradient::~CanvasGradient()
{
#if __APPLE__
    CGShadingRelease(m_shading);
#endif
}

void CanvasGradient::addColorStop(float value, const String& color)
{
    RGBA32 rgba = CSSParser::parseColor(color);
    m_stops.append(ColorStop(value,
        ((rgba >> 16) & 0xFF) / 255.0,
        ((rgba >> 8) & 0xFF) / 255.0,
        (rgba & 0xFF) / 255.0,
        ((rgba >> 24) & 0xFF) / 255.0));

    m_stopsSorted = false;

#if __APPLE__
    CGShadingRelease(m_shading);
    m_shading = 0;
#endif
}

#if __APPLE__

static void gradientCallback(void* info, const CGFloat* in, CGFloat* out)
{
    float r, g, b, a;
    static_cast<CanvasGradient*>(info)->getColor(*in, &r, &g, &b, &a);
    out[0] = r;
    out[1] = g;
    out[2] = b;
    out[3] = a;
}

CGShadingRef CanvasGradient::platformShading()
{
    if (m_shading)
        return m_shading;

    const CGFloat intervalRanges[2] = { 0, 1 };
    const CGFloat colorComponentRanges[4 * 2] = { 0, 1, 0, 1, 0, 1, 0, 1 };
    const CGFunctionCallbacks gradientCallbacks = { 0, gradientCallback, 0 };
    CGFunctionRef colorFunction = CGFunctionCreate(this, 1, intervalRanges, 4, colorComponentRanges, &gradientCallbacks);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    if (m_radial)
        m_shading = CGShadingCreateRadial(colorSpace, m_p0, m_r0, m_p1, m_r1, colorFunction, true, true);
    else
        m_shading = CGShadingCreateAxial(colorSpace, m_p0, m_p1, colorFunction, true, true);

    CGColorSpaceRelease(colorSpace);
    CGFunctionRelease(colorFunction);

    return m_shading;
}

#endif

void CanvasGradient::getColor(float value, float* r, float* g, float* b, float* a)
{
    ASSERT(value >= 0);
    ASSERT(value <= 1);

    if (value <= 0) {
        *r = m_stops.first().red;
        *g = m_stops.first().green;
        *b = m_stops.first().blue;
        *a = m_stops.first().alpha;
        return;
    }
    if (value >= 1) {
        *r = m_stops.last().red;
        *g = m_stops.last().green;
        *b = m_stops.last().blue;
        *a = m_stops.last().alpha;
        return;
    }

    // Find stop before and stop after and interpolate.
    int stop = findStop(value);
    const ColorStop& lastStop = m_stops[stop];    
    const ColorStop& nextStop = m_stops[stop + 1];
    float stopFraction = (value - lastStop.stop) / (nextStop.stop - lastStop.stop);
    *r = lastStop.red + (nextStop.red - lastStop.red) * stopFraction;
    *g = lastStop.green + (nextStop.green - lastStop.green) * stopFraction;
    *b = lastStop.blue + (nextStop.blue - lastStop.blue) * stopFraction;
    *a = lastStop.alpha + (nextStop.alpha - lastStop.alpha) * stopFraction;
}

static int compareStops(const void* a, const void* b)
{
    float as = static_cast<const CanvasGradient::ColorStop*>(a)->stop;
    float bs = static_cast<const CanvasGradient::ColorStop*>(b)->stop;

    if (as > bs)
        return 1;
    if (as < bs)
        return -1;
    return 0;
}

int CanvasGradient::findStop(float value) const
{
    if (!m_stopsSorted) {
        bool addZeroStop = true;
        bool addOneStop = true;
        if (m_stops.size()) {
            qsort(m_stops.data(), m_stops.size(), sizeof(ColorStop), compareStops);
            addZeroStop = m_stops.first().stop != 0;
            addOneStop = m_stops.last().stop != 1;
        }
        if (addZeroStop)
            m_stops.insert(0, ColorStop(0, 0, 0, 0, 1));
        if (addOneStop)
            m_stops.append(ColorStop(1, 0, 0, 0, 1));
        m_stopsSorted = true;
    }

    ASSERT(value >= 0);
    ASSERT(value <= 1);

    int numStops = m_stops.size();
    ASSERT(numStops >= 2);
    ASSERT(m_lastStop < numStops - 1);

    int i = m_lastStop;
    if (value < m_stops[i].stop)
        i = 1;
    else
        i = m_lastStop + 1;

    for (; i < numStops - 1; ++i)
        if (value < m_stops[i].stop)
            break;

    m_lastStop = i - 1;
    return m_lastStop;
}

} //namespace
