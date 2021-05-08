/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "IntRect.h"

#include "FloatRect.h"
#include <algorithm>

using std::max;
using std::min;

namespace WebCore {

IntRect::IntRect(const FloatRect& r)
    : m_location(IntPoint(static_cast<int>(r.x()), static_cast<int>(r.y())))
    , m_size(IntSize(static_cast<int>(r.width()), static_cast<int>(r.height())))
{
}

bool IntRect::intersects(const IntRect& other) const
{
    // Checking emptiness handles negative widths as well as zero.
    return !isEmpty() && !other.isEmpty()
        && x() < other.right() && other.x() < right()
        && y() < other.bottom() && other.y() < bottom();
}

bool IntRect::contains(const IntRect& other) const
{
    return x() <= other.x() && right() >= other.right()
        && y() <= other.y() && bottom() >= other.bottom();
}

void IntRect::intersect(const IntRect& other)
{
    int l = max(x(), other.x());
    int t = max(y(), other.y());
    int r = min(right(), other.right());
    int b = min(bottom(), other.bottom());

    // Return a clean empty rectangle for non-intersecting cases.
    if (l >= r || t >= b) {
        l = 0;
        t = 0;
        r = 0;
        b = 0;
    }

    m_location.setX(l);
    m_location.setY(t);
    m_size.setWidth(r - l);
    m_size.setHeight(b - t);
}

void IntRect::unite(const IntRect& other)
{
    // Handle empty special cases first.
    if (other.isEmpty())
        return;
    if (isEmpty()) {
        *this = other;
        return;
    }

    int l = min(x(), other.x());
    int t = min(y(), other.y());
    int r = max(right(), other.right());
    int b = max(bottom(), other.bottom());

    m_location.setX(l);
    m_location.setY(t);
    m_size.setWidth(r - l);
    m_size.setHeight(b - t);
}

void IntRect::scale(float s)
{
    m_location.setX((int)(x() * s));
    m_location.setY((int)(y() * s));
    m_size.setWidth((int)(width() * s));
    m_size.setHeight((int)(height() * s));
}

}
