/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *               2006 Alexander Kellett <lypanov@kde.org>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#if SVG_SUPPORT
#include "RenderSVGText.h"

#include "GraphicsContext.h"
#include "KCanvasMatrix.h"
#include "KCanvasRenderingStyle.h"
#include "KRenderingDevice.h"
#include "SVGAnimatedLengthList.h"
#include "SVGTextElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

RenderSVGText::RenderSVGText(SVGTextElement *node) 
    : RenderBlock(node)
{
}

AffineTransform RenderSVGText::translationTopToBaseline()
{
    int offset = baselinePosition(true, true);
    return AffineTransform().translate(0, -offset);
}

AffineTransform RenderSVGText::translationForAttributes()
{
    SVGTextElement *text = static_cast<SVGTextElement *>(element());

    float xOffset = text->x()->baseVal()->getFirst() ? text->x()->baseVal()->getFirst()->value() : 0;
    float yOffset = text->y()->baseVal()->getFirst() ? text->y()->baseVal()->getFirst()->value() : 0;

    return AffineTransform().translate(xOffset, yOffset);
}

void RenderSVGText::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled())
        return;

    KRenderingDevice* device = renderingDevice();
    KRenderingDeviceContext* context = device->currentContext();
    bool shouldPopContext = false;
    if (context)
        paintInfo.p->save();
    else {
        // Need to set up KCanvas rendering if it hasn't already been done.
        context = paintInfo.p->createRenderingDeviceContext();
        device->pushContext(context);
        shouldPopContext = true;
    }
    
    context->concatCTM(localTransform());
    context->concatCTM(AffineTransform().translate(parentX, parentY));
    context->concatCTM(translationForAttributes());
    context->concatCTM(translationTopToBaseline());
    
    FloatRect boundingBox(0, 0, width(), height());
    const SVGRenderStyle *svgStyle = style()->svgStyle();
            
    if (KCanvasClipper *clipper = getClipperById(document(), svgStyle->clipPath().mid(1)))
        clipper->applyClip(boundingBox);

    if (KCanvasMasker *masker = getMaskerById(document(), svgStyle->maskElement().mid(1)))
        masker->applyMask(boundingBox);

    KCanvasFilter *filter = getFilterById(document(), svgStyle->filter().mid(1));
    if (filter)
        filter->prepareFilter(boundingBox);
        
    OwnPtr<GraphicsContext> c(device->currentContext()->createGraphicsContext());
    PaintInfo pi = paintInfo;
    pi.p = c.get();
    pi.r = (translationTopToBaseline() * translationForAttributes() * absoluteTransform()).invert().mapRect(paintInfo.r);

    float opacity = style()->opacity();
    if (opacity < 1.0f)
        c->beginTransparencyLayer(opacity);

    KRenderingPaintServer *fillPaintServer = KSVGPainterFactory::fillPaintServer(style(), this);
    if (fillPaintServer) {
        fillPaintServer->setPaintingText(true);
        // fillPaintServer->setActiveClient(this);
        if (fillPaintServer->setup(context, this, APPLY_TO_FILL)) {
            RenderBlock::paint(pi, 0, 0);
            fillPaintServer->teardown(context, this, APPLY_TO_FILL);
        }
        fillPaintServer->setPaintingText(false);
    }
    
    KRenderingPaintServer *strokePaintServer = KSVGPainterFactory::strokePaintServer(style(), this);
    if (strokePaintServer) {
        strokePaintServer->setPaintingText(true);
        // strokePaintServer->setActiveClient(this);
        if (strokePaintServer->setup(context, this, APPLY_TO_STROKE)) {
            RenderBlock::paint(pi, 0, 0);
            strokePaintServer->teardown(context, this, APPLY_TO_STROKE);
        }
        strokePaintServer->setPaintingText(false);
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

void RenderSVGText::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
    AffineTransform transform = translationTopToBaseline() * translationForAttributes() * absoluteTransform();
    r = transform.mapRect(r);
    
    RenderContainer::computeAbsoluteRepaintRect(r, f);
}

bool RenderSVGText::requiresLayer()
{
    return false;
}

void RenderSVGText::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = m_absoluteBounds;

    RenderBlock::layout();

    m_absoluteBounds = getAbsoluteRepaintRect();

    bool repainted = false;
    if (checkForRepaint)
        repainted = repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
    
    setNeedsLayout(false);
}

bool RenderSVGText::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    AffineTransform totalTransform = translationForAttributes();
    totalTransform *= translationTopToBaseline();
    totalTransform *= absoluteTransform();
    double localX, localY;
    totalTransform.invert().map(_x, _y, &localX, &localY);
    return RenderBlock::nodeAtPoint(info, (int)localX, (int)localY, _tx, _ty, hitTestAction);
}

void RenderSVGText::absoluteRects(DeprecatedValueList<IntRect>& rects, int _tx, int _ty)
{
    AffineTransform mat = translationForAttributes();
    mat *= translationTopToBaseline();
    mat *= absoluteTransform();

    rects.append(enclosingIntRect(mat.mapRect(FloatRect(0, 0, width(), height()))));
}

}

#endif // SVG_SUPPORT
