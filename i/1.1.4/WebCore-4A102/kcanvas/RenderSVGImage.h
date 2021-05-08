/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the WebKit project.

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

#ifndef KCanvas_RenderSVGImage_H
#define KCanvas_RenderSVGImage_H
#if SVG_SUPPORT

#include "RenderImage.h"
#include "AffineTransform.h"

namespace WebCore
{
    class SVGImageElement;
    class SVGPreserveAspectRatio;
    class RenderSVGImage : public RenderImage {
    public:
        RenderSVGImage(SVGImageElement *impl);
        virtual ~RenderSVGImage();
        
        virtual AffineTransform localTransform() const { return m_transform; }
        virtual void setLocalTransform(const AffineTransform& transform) { m_transform = transform; }
        
        virtual FloatRect relativeBBox(bool includeStroke = true) const;
        virtual IntRect getAbsoluteRepaintRect();
        
        virtual void absoluteRects(DeprecatedValueList<IntRect>& rects, int tx, int ty);

        virtual void imageChanged(CachedImage*);
        void adjustRectsForAspectRatio(FloatRect& destRect, FloatRect& srcRect, SVGPreserveAspectRatio *aspectRatio);
        virtual void paint(PaintInfo& paintInfo, int parentX, int parentY);
        virtual void layout();

        bool requiresLayer();

        virtual void computeAbsoluteRepaintRect(IntRect& r, bool f);

        virtual bool RenderSVGImage::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction);

    private:
        void translateForAttributes();
        AffineTransform translationForAttributes();
        AffineTransform m_transform;
        IntRect m_absoluteBounds;
    };
}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
