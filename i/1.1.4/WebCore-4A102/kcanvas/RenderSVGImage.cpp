/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the WebKit project

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

#include "config.h"
#if SVG_SUPPORT
#include "RenderSVGImage.h"

#include "Attr.h"
#include "GraphicsContext.h"
#include "KCanvasMaskerQuartz.h"
#include "KCanvasRenderingStyle.h"
#include "KCanvasResourcesQuartz.h"
#include "KRenderingDevice.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGImageElement.h"
#include "SVGImageElement.h"
#include "ksvg.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

RenderSVGImage::RenderSVGImage(SVGImageElement *impl)
: RenderImage(impl)
{
}

RenderSVGImage::~RenderSVGImage()
{
}

void RenderSVGImage::adjustRectsForAspectRatio(FloatRect& destRect, FloatRect& srcRect, SVGPreserveAspectRatio *aspectRatio)
{
    float origDestWidth = destRect.width();
    float origDestHeight = destRect.height();
    if (aspectRatio->meetOrSlice() == SVG_MEETORSLICE_MEET) {
        float widthToHeightMultiplier = srcRect.height() / srcRect.width();
        if (origDestHeight > (origDestWidth * widthToHeightMultiplier)) {
            destRect.setHeight(origDestWidth * widthToHeightMultiplier);
            switch(aspectRatio->align()) {
                case SVG_PRESERVEASPECTRATIO_XMINYMID:
                case SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVG_PRESERVEASPECTRATIO_XMAXYMID:
                    destRect.setY(origDestHeight / 2 - destRect.height() / 2);
                    break;
                case SVG_PRESERVEASPECTRATIO_XMINYMAX:
                case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    destRect.setY(origDestHeight - destRect.height());
                    break;
            }
        }
        if (origDestWidth > (origDestHeight / widthToHeightMultiplier)) {
            destRect.setWidth(origDestHeight / widthToHeightMultiplier);
            switch(aspectRatio->align()) {
                case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
                case SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                    destRect.setX(origDestWidth / 2 - destRect.width() / 2);
                    break;
                case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
                case SVG_PRESERVEASPECTRATIO_XMAXYMID:
                case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    destRect.setX(origDestWidth - destRect.width());
                    break;
            }
        }
    } else if (aspectRatio->meetOrSlice() == SVG_MEETORSLICE_SLICE) {
        float widthToHeightMultiplier = srcRect.height() / srcRect.width();
        // if the destination height is less than the height of the image we'll be drawing
        if (origDestHeight < (origDestWidth * widthToHeightMultiplier)) {
            float destToSrcMultiplier = srcRect.width() / destRect.width();
            srcRect.setHeight(destRect.height() * destToSrcMultiplier);
            switch(aspectRatio->align()) {
                case SVG_PRESERVEASPECTRATIO_XMINYMID:
                case SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVG_PRESERVEASPECTRATIO_XMAXYMID:
                    srcRect.setY(image()->height() / 2 - srcRect.height() / 2);
                    break;
                case SVG_PRESERVEASPECTRATIO_XMINYMAX:
                case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    srcRect.setY(image()->height() - srcRect.height());
                    break;
            }
        }
        // if the destination width is less than the width of the image we'll be drawing
        if (origDestWidth < (origDestHeight / widthToHeightMultiplier)) {
            float destToSrcMultiplier = srcRect.height() / destRect.height();
            srcRect.setWidth(destRect.width() * destToSrcMultiplier);
            switch(aspectRatio->align()) {
                case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
                case SVG_PRESERVEASPECTRATIO_XMIDYMID:
                case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                    srcRect.setX(image()->width() / 2 - srcRect.width() / 2);
                    break;
                case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
                case SVG_PRESERVEASPECTRATIO_XMAXYMID:
                case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                    srcRect.setX(image()->width() - srcRect.width());
                    break;
            }
        }
    }
}

void RenderSVGImage::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled() || (paintInfo.phase != PaintPhaseForeground) || style()->visibility() == HIDDEN)
        return;
    
    KRenderingDevice* device = renderingDevice();
    KRenderingDeviceContext* context = device->currentContext();
    bool shouldPopContext = false;
    if (context)
        paintInfo.p->save();
    else {
        // Need to push a device context on the stack if empty.
        context = paintInfo.p->createRenderingDeviceContext();
        device->pushContext(context);
        shouldPopContext = true;
    }

    context->concatCTM(AffineTransform().translate(parentX, parentY));
    context->concatCTM(localTransform());
    translateForAttributes();
    
    FloatRect boundingBox = relativeBBox(true);
    const SVGRenderStyle *svgStyle = style()->svgStyle();
            
    if (KCanvasClipper *clipper = getClipperById(document(), svgStyle->clipPath().mid(1)))
        clipper->applyClip(boundingBox);

    if (KCanvasMasker *masker = getMaskerById(document(), svgStyle->maskElement().mid(1)))
        masker->applyMask(boundingBox);

    KCanvasFilter *filter = getFilterById(document(), svgStyle->filter().mid(1));
    if (filter)
        filter->prepareFilter(boundingBox);
    
    OwnPtr<GraphicsContext> c(device->currentContext()->createGraphicsContext());

    float opacity = style()->opacity();
    if (opacity < 1.0f)
        c->beginTransparencyLayer(opacity);

    PaintInfo pi(paintInfo);
    pi.p = c.get();
    pi.r = absoluteTransform().invert().mapRect(paintInfo.r);

    int x = 0, y = 0;
    if (!shouldPaint(pi, x, y))
        return;
        
    SVGImageElement *imageElt = static_cast<SVGImageElement *>(node());
        
    if (imageElt->preserveAspectRatio()->baseVal()->align() == SVG_PRESERVEASPECTRATIO_NONE)
        RenderImage::paint(pi, 0, 0);
    else {
        FloatRect destRect(m_x, m_y, contentWidth(), contentHeight());
        FloatRect srcRect(0, 0, image()->width(), image()->height());
        adjustRectsForAspectRatio(destRect, srcRect, imageElt->preserveAspectRatio()->baseVal());
        c->drawImage(image(), destRect, srcRect);
    }

    if (filter)
        filter->applyFilter(boundingBox);
    
    if (opacity < 1.0f)
        c->endTransparencyLayer();

    // restore drawing state
    if (!shouldPopContext)
        paintInfo.p->restore();
    else {
        device->popContext();
        delete context;
    }
}

void RenderSVGImage::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
    AffineTransform transform = translationForAttributes() * localTransform();
    r = transform.mapRect(r);
    
    RenderImage::computeAbsoluteRepaintRect(r, f);
}

bool RenderSVGImage::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    AffineTransform totalTransform = absoluteTransform();
    totalTransform *= translationForAttributes();
    double localX, localY;
    totalTransform.invert().map(_x + _tx, _y + _ty, &localX, &localY);
    return RenderImage::nodeAtPoint(info, (int)localX, (int)localY, 0, 0, hitTestAction);
}

bool RenderSVGImage::requiresLayer()
{
    return false;
}

void RenderSVGImage::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = m_absoluteBounds;

    // minimum height
    m_height = cachedImage() && cachedImage() ? intrinsicHeight() : 0;

    calcWidth();
    calcHeight();

    m_absoluteBounds = getAbsoluteRepaintRect();

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
    
    setNeedsLayout(false);
}

FloatRect RenderSVGImage::relativeBBox(bool includeStroke) const
{
    SVGImageElement *image = static_cast<SVGImageElement *>(node());
    float xOffset = image->x()->baseVal() ? image->x()->baseVal()->value() : 0;
    float yOffset = image->y()->baseVal() ? image->y()->baseVal()->value() : 0;
    return FloatRect(xOffset, yOffset, width(), height());
}

void RenderSVGImage::imageChanged(CachedImage* image)
{
    RenderImage::imageChanged(image);
    // We override to invalidate a larger rect, since SVG images can draw outside their "bounds"
    repaintRectangle(getAbsoluteRepaintRect());
}

IntRect RenderSVGImage::getAbsoluteRepaintRect()
{
    SVGImageElement *image = static_cast<SVGImageElement *>(node());
    float xOffset = image->x()->baseVal() ? image->x()->baseVal()->value() : 0;
    float yOffset = image->y()->baseVal() ? image->y()->baseVal()->value() : 0;
    FloatRect repaintRect = absoluteTransform().mapRect(FloatRect(xOffset, yOffset, width(), height()));

    // Filters can expand the bounding box
    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (filter)
        repaintRect.unite(filter->filterBBoxForItemBBox(repaintRect));

    return enclosingIntRect(repaintRect);
}

void RenderSVGImage::absoluteRects(DeprecatedValueList<IntRect>& rects, int _tx, int _ty)
{
    rects.append(getAbsoluteRepaintRect());
}


AffineTransform RenderSVGImage::translationForAttributes()
{
    SVGImageElement *image = static_cast<SVGImageElement *>(node());
    float xOffset = image->x()->baseVal() ? image->x()->baseVal()->value() : 0;
    float yOffset = image->y()->baseVal() ? image->y()->baseVal()->value() : 0;
    return AffineTransform().translate(xOffset, yOffset);
}

void RenderSVGImage::translateForAttributes()
{
    KRenderingDeviceContext *context = renderingDevice()->currentContext();
    context->concatCTM(translationForAttributes());
}

}

#endif // SVG_SUPPORT
