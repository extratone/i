/*
 * This file is part of the render object implementation for KHTML.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef RenderInline_h
#define RenderInline_h

#include "RenderFlow.h"

namespace WebCore {

class Position;

class RenderInline : public RenderFlow {
public:
    RenderInline(Node*);
    virtual ~RenderInline();

    virtual const char* renderName() const;

    virtual bool isRenderInline() const { return true; }
    virtual bool childrenInline() const { return true; }

    virtual bool isInlineContinuation() const;

    virtual void addChildToFlow(RenderObject* newChild, RenderObject* beforeChild);
    void splitInlines(RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock,
                      RenderObject* beforeChild, RenderFlow* oldCont);
    void splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                   RenderObject* newChild, RenderFlow* oldCont);

    virtual void layout() { } // Do nothing for layout()

    virtual void paint(PaintInfo&, int tx, int ty);

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    virtual bool requiresLayer() const { return isRelPositioned() || isTransparent() || hasMask(); }

    virtual int offsetLeft() const;
    virtual int offsetTop() const;
    virtual int offsetWidth() const { return linesBoundingBox().width(); }
    virtual int offsetHeight() const { return linesBoundingBox().height(); }

    void absoluteRects(Vector<IntRect>&, int tx, int ty, bool topLevel = true);
    virtual void absoluteQuads(Vector<FloatQuad>&, bool topLevel = true);

    virtual IntRect clippedOverflowRectForRepaint(RenderBox* repaintContainer);

    virtual VisiblePosition positionForCoordinates(int x, int y);

    IntRect linesBoundingBox() const;
    
    virtual IntRect borderBoundingBox() const
    {
        IntRect boundingBox = linesBoundingBox();
        return IntRect(0, 0, boundingBox.width(), boundingBox.height());
    }

protected:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    static RenderInline* cloneInline(RenderFlow* src);

};

} // namespace WebCore

#endif // RenderInline_h
