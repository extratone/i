/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007, 2008, 2009 Rob Buis <buis@kde.org>
                  2007 Eric Seidel <eric@webkit.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGRoot.h"

#include "GraphicsContext.h"
#include "RenderPath.h"
#include "RenderSVGContainer.h"
#include "RenderView.h"
#include "SVGLength.h"
#include "SVGRenderSupport.h"
#include "SVGResourceClipper.h"
#include "SVGResourceFilter.h"
#include "SVGResourceMasker.h"
#include "SVGSVGElement.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"

using namespace std;

namespace WebCore {

RenderSVGRoot::RenderSVGRoot(SVGStyledElement* node)
    : RenderContainer(node)
{
    setReplaced(true);
}

RenderSVGRoot::~RenderSVGRoot()
{
}

int RenderSVGRoot::lineHeight(bool, bool) const
{
    return height() + marginTop() + marginBottom();
}

int RenderSVGRoot::baselinePosition(bool, bool) const
{
    return height() + marginTop() + marginBottom();
}

void RenderSVGRoot::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    int paddingAndBorders = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    int width = calcReplacedWidth(false) + paddingAndBorders;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength)
        width = min(width, style()->maxWidth().value() + (style()->boxSizing() == CONTENT_BOX ? paddingAndBorders : 0));

    if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent())) {
        m_minPrefWidth = 0;
        m_maxPrefWidth = width;
    } else
        m_minPrefWidth = m_maxPrefWidth = width;

    setPrefWidthsDirty(false);
}

void RenderSVGRoot::layout()
{
    ASSERT(needsLayout());

    calcViewport();

    // Arbitrary affine transforms are incompatible with LayoutState.
    view()->disableLayoutState();

    // FIXME: using m_absoluteBounds breaks if containerForRepaint() is not the root
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout() && selfNeedsLayout(), &m_absoluteBounds);

    calcWidth();
    calcHeight();

    m_absoluteBounds = absoluteClippedOverflowRect();
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());
    setWidth(static_cast<int>(width() * svg->currentScale()));
    setHeight(static_cast<int>(height() * svg->currentScale()));
    
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (selfNeedsLayout()) // either bounds or transform changed, force kids to relayout
            child->setNeedsLayout(true, false);
        
        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
    }

    repainter.repaintAfterLayout();

    view()->enableLayoutState();
    setNeedsLayout(false);
}

void RenderSVGRoot::applyContentTransforms(PaintInfo& paintInfo, int parentX, int parentY)
{
    // Translate from parent offsets (html renderers) to a relative transform (svg renderers)
    IntPoint origin;
    origin.move(parentX, parentY);
    origin.move(x(), y());
    origin.move(borderLeft(), borderTop());
    origin.move(paddingLeft(), paddingTop());

    if (origin.x() || origin.y()) {
        paintInfo.context->concatCTM(TransformationMatrix().translate(origin.x(), origin.y()));
        paintInfo.rect.move(-origin.x(), -origin.y());
    }

    // Respect scroll offset caused by html parents
    TransformationMatrix ctm = RenderContainer::absoluteTransform();
    paintInfo.rect.move(static_cast<int>(ctm.e()), static_cast<int>(ctm.f()));

    SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());
    paintInfo.context->concatCTM(TransformationMatrix().scale(svg->currentScale()));

    if (!viewport().isEmpty()) {
        if (style()->overflowX() != OVISIBLE)
            paintInfo.context->clip(enclosingIntRect(viewport())); // FIXME: Eventually we'll want float-precision clipping
        
        paintInfo.context->concatCTM(TransformationMatrix().translate(viewport().x(), viewport().y()));
    }

    paintInfo.context->concatCTM(TransformationMatrix().translate(svg->currentTranslate().x(), svg->currentTranslate().y()));
}

void RenderSVGRoot::paint(PaintInfo& paintInfo, int parentX, int parentY)
{
    if (paintInfo.context->paintingDisabled())
        return;

    calcViewport();

    SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());
    // A value of zero disables rendering of the element. 
    if (viewport().width() <= 0. || viewport().height() <= 0.)
        return;

    // This should only exist for <svg> renderers
    if (hasBoxDecorations() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection)) 
        paintBoxDecorations(paintInfo, x() + parentX, y() + parentY);

    if (!firstChild()) {
#if ENABLE(SVG_FILTERS)
        // Spec: groups w/o children still may render filter content.
        const SVGRenderStyle* svgStyle = style()->svgStyle();
        SVGResourceFilter* filter = getFilterById(document(), svgStyle->filter());
        if (!filter)
#endif
            return;
    }

    RenderObject::PaintInfo childPaintInfo(paintInfo);
    childPaintInfo.context->save();
 
    applyContentTransforms(childPaintInfo, parentX, parentY);

    SVGResourceFilter* filter = 0;

    FloatRect boundingBox = relativeBBox(true);
    if (childPaintInfo.phase == PaintPhaseForeground)
        prepareToRenderSVGContent(this, childPaintInfo, boundingBox, filter);        

    childPaintInfo.context->concatCTM(svg->viewBoxToViewTransform(width(), height()));
    RenderContainer::paint(childPaintInfo, 0, 0);

    if (childPaintInfo.phase == PaintPhaseForeground)
        finishRenderSVGContent(this, childPaintInfo, boundingBox, filter, paintInfo.context);

    childPaintInfo.context->restore();
    
    if ((childPaintInfo.phase == PaintPhaseOutline || childPaintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(childPaintInfo.context, m_absoluteBounds.x(), m_absoluteBounds.y(), m_absoluteBounds.width(), m_absoluteBounds.height(), style());
}

FloatRect RenderSVGRoot::viewport() const
{
    return m_viewport;
}

void RenderSVGRoot::calcViewport()
{
    SVGElement* svgelem = static_cast<SVGElement*>(element());
    if (svgelem->hasTagName(SVGNames::svgTag)) {
        SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());

        if (!selfNeedsLayout() && !svg->hasRelativeValues())
            return;

        float w, h;
        SVGLength width = svg->width();
        if (width.unitType() == LengthTypePercentage && svg->hasSetContainerSize())
            w = svg->relativeWidthValue();
        else
            w = width.value(svg);
        
        SVGLength height = svg->height();
        if (height.unitType() == LengthTypePercentage && svg->hasSetContainerSize())
            h = svg->relativeHeightValue();
        else
            h = height.value(svg);

        m_viewport = FloatRect(0, 0, w, h);
    }
}

IntRect RenderSVGRoot::clippedOverflowRectForRepaint(RenderBox* repaintContainer)
{
    IntRect repaintRect;

    for (RenderObject* current = firstChild(); current != 0; current = current->nextSibling())
        repaintRect.unite(current->clippedOverflowRectForRepaint(repaintContainer));

#if ENABLE(SVG_FILTERS)
    // Filters can expand the bounding box
    SVGResourceFilter* filter = getFilterById(document(), style()->svgStyle()->filter());
    if (filter)
        repaintRect.unite(enclosingIntRect(filter->filterBBoxForItemBBox(repaintRect)));
#endif

    return repaintRect;
}

void RenderSVGRoot::addFocusRingRects(GraphicsContext* graphicsContext, int, int)
{
    graphicsContext->addFocusRingRect(m_absoluteBounds);
}

void RenderSVGRoot::absoluteRects(Vector<IntRect>& rects, int, int)
{
    for (RenderObject* current = firstChild(); current != 0; current = current->nextSibling())
        current->absoluteRects(rects, 0, 0);
}

void RenderSVGRoot::absoluteQuads(Vector<FloatQuad>& quads, bool)
{
    for (RenderObject* current = firstChild(); current != 0; current = current->nextSibling())
        current->absoluteQuads(quads);
}

TransformationMatrix RenderSVGRoot::absoluteTransform() const
{
    TransformationMatrix ctm = RenderContainer::absoluteTransform();
    ctm.translate(x(), y());
    SVGSVGElement* svg = static_cast<SVGSVGElement*>(element());
    ctm.scale(svg->currentScale());
    ctm.translate(svg->currentTranslate().x(), svg->currentTranslate().y());
    ctm.translate(viewport().x(), viewport().y());
    return svg->viewBoxToViewTransform(width(), height()) * ctm;
}

FloatRect RenderSVGRoot::relativeBBox(bool includeStroke) const
{
    FloatRect rect;
    
    RenderObject* current = firstChild();
    for (; current != 0; current = current->nextSibling()) {
        FloatRect childBBox = current->relativeBBox(includeStroke);
        FloatRect mappedBBox = current->localTransform().mapRect(childBBox);
        // <svg> can have a viewBox contributing to the bbox
        if (current->isSVGContainer())
            mappedBBox = static_cast<RenderSVGContainer*>(current)->viewportTransform().mapRect(mappedBBox);
        rect.unite(mappedBBox);
    }

    return rect;
}

TransformationMatrix RenderSVGRoot::localTransform() const
{
    return TransformationMatrix();
}

bool RenderSVGRoot::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    TransformationMatrix ctm = RenderContainer::absoluteTransform();

    int sx = (_tx - static_cast<int>(ctm.e())); // scroll offset
    int sy = (_ty - static_cast<int>(ctm.f())); // scroll offset
 
    if (!viewport().isEmpty()
        && style()->overflowX() == OHIDDEN
        && style()->overflowY() == OHIDDEN) {
        int tx = x() - _tx + sx;
        int ty = y() - _ty + sy;

        // Check if we need to do anything at all.
        IntRect overflowBox = overflowRect(false);
        overflowBox.move(tx, ty);
        ctm.translate(viewport().x(), viewport().y());
        double localX, localY;
        ctm.inverse().map(_x - _tx, _y - _ty, localX, localY);
        if (!overflowBox.contains((int)localX, (int)localY))
            return false;
    }

    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (child->nodeAtPoint(request, result, _x - sx, _y - sy, 0, 0, hitTestAction)) {
            updateHitTestResult(result, IntPoint(_x - _tx, _y - _ty));
            return true;
        }
    }
    
    // Spec: Only graphical elements can be targeted by the mouse, period.
    // 16.4: "If there are no graphics elements whose relevant graphics content is under the pointer (i.e., there is no target element), the event is not dispatched."
    return false;
}

void RenderSVGRoot::position(InlineBox* box)
{
    RenderContainer::position(box);
    if (m_absoluteBounds.isEmpty())
        setNeedsLayout(true, false);
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
