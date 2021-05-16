/*
 * Copyright (C) 2006, 2007, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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
#include "Gradient.h"

#include "CSSParser.h"
#include "NotImplemented.h"

#include <QGradient>

namespace WebCore {

void Gradient::platformDestroy()
{
    delete m_gradient;
    m_gradient = 0;
}

QGradient* Gradient::platformGradient()
{
    if (m_gradient)
        return m_gradient;

    if (m_radial)
        m_gradient = new QRadialGradient(m_p1.x(), m_p1.y(), m_r1, m_p0.x(), m_p0.y());
    else
        m_gradient = new QLinearGradient(m_p0.x(), m_p0.y(), m_p1.x(), m_p1.y());

    QColor stopColor;
    Vector<ColorStop>::iterator stopIterator = m_stops.begin();
    qreal lastStop(0.0);
    const qreal lastStopDiff = 0.0000001;
    while (stopIterator != m_stops.end()) {
        stopColor.setRgbF(stopIterator->red, stopIterator->green, stopIterator->blue, stopIterator->alpha);
        if (qFuzzyCompare(lastStop, qreal(stopIterator->stop)))
            lastStop = stopIterator->stop + lastStopDiff;
        else
            lastStop = stopIterator->stop;
        if (m_radial && m_r0)
            lastStop = m_r0 / m_r1 + lastStop * (1.0f - m_r0 / m_r1);
        m_gradient->setColorAt(lastStop, stopColor);
        ++stopIterator;
    }

    return m_gradient;
}

void Gradient::fill(GraphicsContext* context, const FloatRect& rect)
{
    notImplemented();
}

} //namespace
