/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef RenderSVGTextPath_h
#define RenderSVGTextPath_h

#if ENABLE(SVG)
#include "RenderSVGInline.h"

namespace WebCore {

    class RenderSVGTextPath : public RenderSVGInline {
    public:
        RenderSVGTextPath(Node*);

        Path layoutPath() const;
        float startOffset() const;
        bool exactAlignment() const;
        bool stretchMethod() const;

        virtual const char* renderName() const { return "RenderSVGTextPath"; }
        virtual void absoluteRects(Vector<IntRect>& rects, int tx, int ty);
        virtual void absoluteQuads(Vector<FloatQuad>&, bool topLevel = true);

    private:
        float m_startOffset;

        bool m_exactAlignment : 1;
        bool m_stretchMethod : 1;

        Path m_layoutPath;
    };
}

#endif // ENABLE(SVG)
#endif // RenderSVGTextPath_h
