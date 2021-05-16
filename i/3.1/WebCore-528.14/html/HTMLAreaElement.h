/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLAreaElement_h
#define HTMLAreaElement_h

#include "HTMLAnchorElement.h"
#include "IntSize.h"
#include "Path.h"

namespace WebCore {

class HitTestResult;

class HTMLAreaElement : public HTMLAnchorElement {
public:
    HTMLAreaElement(const QualifiedName&, Document*);
    ~HTMLAreaElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual void parseMappedAttribute(MappedAttribute*);

    bool isDefault() const { return m_shape == Default; }

    bool mapMouseEvent(int x, int y, const IntSize&, HitTestResult&);

    virtual IntRect getRect(RenderObject*) const;

    String accessKey() const;
    void setAccessKey(const String&);

    String alt() const;
    void setAlt(const String&);

    String coords() const;
    void setCoords(const String&);

    KURL href() const;
    void setHref(const String&);

    bool noHref() const;
    void setNoHref(bool);

    String shape() const;
    void setShape(const String&);

    virtual bool isFocusable() const;

    virtual String target() const;
    void setTarget(const String&);

private:
    enum Shape { Default, Poly, Rect, Circle, Unknown };
    Path getRegion(const IntSize&) const;
    Path region;
    Length* m_coords;
    int m_coordsLen;
    IntSize m_lastSize;
    Shape m_shape;
};

} //namespace

#endif
