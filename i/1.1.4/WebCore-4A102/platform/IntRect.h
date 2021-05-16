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

#ifndef INTRECT_H_
#define INTRECT_H_

#include "IntPoint.h"

#import <CoreGraphics/CGGeometry.h>

#if __APPLE__

typedef struct CGRect CGRect;


#ifndef NSRect
#define NSRect CGRect
#endif

#endif

#if WIN32
typedef struct tagRECT RECT;
#endif

namespace WebCore {

class IntRect {
public:
    IntRect() { }
    IntRect(const IntPoint& location, const IntSize& size)
        : m_location(location), m_size(size) { }
    IntRect(int x, int y, int width, int height)
        : m_location(IntPoint(x, y)), m_size(IntSize(width, height)) { }

    IntPoint location() const { return m_location; }
    IntSize size() const { return m_size; }

    void setLocation(const IntPoint& location) { m_location = location; }
    void setSize(const IntSize& size) { m_size = size; }

    int x() const { return m_location.x(); }
    int y() const { return m_location.y(); }
    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }

    void setX(int x) { m_location.setX(x); }
    void setY(int y) { m_location.setY(y); }
    void setWidth(int width) { m_size.setWidth(width); }
    void setHeight(int height) { m_size.setHeight(height); }

    bool isEmpty() const { return m_size.isEmpty(); }

    int right() const { return x() + width(); }
    int bottom() const { return y() + height(); }

    void move(const IntSize& s) { m_location += s; } 
    void move(int dx, int dy) { m_location.move(dx, dy); } 

    bool intersects(const IntRect&) const;
    bool contains(const IntRect&) const;

    // This checks to see if the rect contains x,y in the traditional sense.
    // Equivalent to checking if the rect contains a 1x1 rect below and to the right of (px,py).
    bool contains(int px, int py) const
        { return px >= x() && px < right() && py >= y() && py < bottom(); }
    bool contains(const IntPoint& point) const { return contains(point.x(), point.y()); }

    void intersect(const IntRect&);
    void unite(const IntRect&);

    void inflateX(int dx) {
        m_location.setX(m_location.x() - dx);
        m_size.setWidth(m_size.width() + dx + dx);
    }
    void inflateY(int dy) {
        m_location.setY(m_location.y() - dy);
        m_size.setHeight(m_size.height() + dy + dy);
    }
    void inflate(int d) { inflateX(d); inflateY(d); }
    void scale(float s);

#if WIN32
    IntRect(const RECT&);
    operator RECT() const;
#endif

#if __APPLE__

    operator CGRect() const;

#endif

private:
    IntPoint m_location;
    IntSize m_size;
};

inline IntRect intersection(const IntRect& a, const IntRect& b)
{
    IntRect c = a;
    c.intersect(b);
    return c;
}

inline IntRect unionRect(const IntRect& a, const IntRect& b)
{
    IntRect c = a;
    c.unite(b);
    return c;
}

inline bool operator==(const IntRect& a, const IntRect& b)
{
    return a.location() == b.location() && a.size() == b.size();
}

inline bool operator!=(const IntRect& a, const IntRect& b)
{
    return a.location() != b.location() || a.size() != b.size();
}

#if __APPLE__

IntRect enclosingIntRect(const CGRect&);

#endif

}

#endif