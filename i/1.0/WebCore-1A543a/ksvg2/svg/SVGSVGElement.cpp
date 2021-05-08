/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT
#include "SVGSVGElement.h"

#include "CSSPropertyNames.h"
#include "Document.h"
#include "EventListener.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "KCanvasRenderingStyle.h"
#include "KSVGTimeScheduler.h"
#include "SVGAngle.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedRect.h"
#include "SVGDocumentExtensions.h"
#include "SVGMatrix.h"
#include "SVGNumber.h"
#include "SVGTransform.h"
#include "SVGZoomEvent.h"
#include "ksvg.h"
#include <kcanvas/RenderSVGContainer.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/device/KRenderingDevice.h>
#include "TextStream.h"

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

SVGSVGElement::SVGSVGElement(const QualifiedName& tagName, Document *doc)
    : SVGStyledLocatableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGFitToViewBox()
    , SVGZoomAndPan()
    , m_useCurrentView(false)
    , m_timeScheduler(new TimeScheduler(doc))
{
}

SVGSVGElement::~SVGSVGElement()
{
    delete m_timeScheduler;
}

SVGAnimatedLength *SVGSVGElement::x() const
{
    const SVGElement *viewport = ownerDocument()->documentElement() == this ? this : viewportElement();
    return lazy_create<SVGAnimatedLength>(m_x, (SVGStyledElement *)0, LM_WIDTH, viewport);
}

SVGAnimatedLength *SVGSVGElement::y() const
{
    const SVGElement *viewport = ownerDocument()->documentElement() == this ? this : viewportElement();
    return lazy_create<SVGAnimatedLength>(m_y, (SVGStyledElement *)0, LM_HEIGHT, viewport);
}

SVGAnimatedLength *SVGSVGElement::width() const
{
    if (!m_width) {
        String temp("100%");
        const SVGElement *viewport = ownerDocument()->documentElement() == this ? this : viewportElement();
        lazy_create<SVGAnimatedLength>(m_width, (SVGStyledElement *)ownerDocument()->documentElement() == this ? 0 : this, LM_WIDTH, viewport);
        m_width->baseVal()->setValueAsString(temp.impl());
    }

    return m_width.get();
}

SVGAnimatedLength *SVGSVGElement::height() const
{
    if (!m_height) {
        String temp("100%");
        const SVGElement *viewport = ownerDocument()->documentElement() == this ? this : viewportElement();
        lazy_create<SVGAnimatedLength>(m_height, (SVGStyledElement *)ownerDocument()->documentElement() == this ? 0 : this, LM_HEIGHT, viewport);
        m_height->baseVal()->setValueAsString(temp.impl());
    }

    return m_height.get();
}

AtomicString SVGSVGElement::contentScriptType() const
{
    return tryGetAttribute("contentScriptType", "text/ecmascript");
}

void SVGSVGElement::setContentScriptType(const AtomicString& type)
{
    setAttribute(SVGNames::contentScriptTypeAttr, type);
}

AtomicString SVGSVGElement::contentStyleType() const
{
    return tryGetAttribute("contentStyleType", "text/css");
}

void SVGSVGElement::setContentStyleType(const AtomicString& type)
{
    setAttribute(SVGNames::contentStyleTypeAttr, type);
}

FloatRect SVGSVGElement::viewport() const
{
    double _x = x()->baseVal()->value();
    double _y = y()->baseVal()->value();
    double w = width()->baseVal()->value();
    double h = height()->baseVal()->value();
    RefPtr<SVGMatrix> viewBox = viewBoxToViewTransform(w, h);
    viewBox->matrix().map(_x, _y, &_x, &_y);
    viewBox->matrix().map(w, h, &w, &h);
    return FloatRect(_x, _y, w, h);
}

float SVGSVGElement::pixelUnitToMillimeterX() const
{
#if 0
    if(ownerDocument() && ownerDocument()->paintDeviceMetrics())
    {
        Q3PaintDeviceMetrics *metrics = ownerDocument()->paintDeviceMetrics();
        return float(metrics->widthMM()) / float(metrics->width());
    }
#endif

    return .28;
}

float SVGSVGElement::pixelUnitToMillimeterY() const
{
#if 0
    if(ownerDocument() && ownerDocument()->paintDeviceMetrics())
    {
        Q3PaintDeviceMetrics *metrics = ownerDocument()->paintDeviceMetrics();
        return float(metrics->heightMM()) / float(metrics->height());
    }
#endif

    return .28;
}

float SVGSVGElement::screenPixelToMillimeterX() const
{
    return pixelUnitToMillimeterX();
}

float SVGSVGElement::screenPixelToMillimeterY() const
{
    return pixelUnitToMillimeterY();
}

bool SVGSVGElement::useCurrentView() const
{
    return m_useCurrentView;
}

void SVGSVGElement::setUseCurrentView(bool currentView)
{
    m_useCurrentView = currentView;
}

float SVGSVGElement::currentScale() const
{
    //if(!canvasView())
        return 1.;

    //return canvasView()->zoom();
}

void SVGSVGElement::setCurrentScale(float scale)
{
//    if(canvasView())
//    {
//        float oldzoom = canvasView()->zoom();
//        canvasView()->setZoom(scale);
//        document()->dispatchZoomEvent(oldzoom, scale);
//    }
}

FloatPoint SVGSVGElement::currentTranslate() const
{
    //if(!view())
        return FloatPoint();

    //return createSVGPoint(canvasView()->pan());
}

void SVGSVGElement::addSVGWindowEventListner(const AtomicString& eventType, const Attribute* attr)
{
    // FIXME: None of these should be window events long term.
    // Once we propertly support SVGLoad, etc.
    RefPtr<EventListener> listener = document()->accessSVGExtensions()->
        createSVGEventListener(attr->localName().domString(), attr->value(), this);
    document()->setHTMLWindowEventListener(eventType, listener.release());
}

void SVGSVGElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (!nearestViewportElement()) {
        // Only handle events if we're the outermost <svg> element
        if (attr->name() == onloadAttr)
            addSVGWindowEventListner(loadEvent, attr);
        else if (attr->name() == onunloadAttr)
            addSVGWindowEventListner(unloadEvent, attr);
        else if (attr->name() == onabortAttr)
            addSVGWindowEventListner(abortEvent, attr);
        else if (attr->name() == onerrorAttr)
            addSVGWindowEventListner(errorEvent, attr);
        else if (attr->name() == onresizeAttr)
            addSVGWindowEventListner(resizeEvent, attr);
        else if (attr->name() == onscrollAttr)
            addSVGWindowEventListner(scrollEvent, attr);
        else if (attr->name() == SVGNames::onzoomAttr)
            addSVGWindowEventListner(zoomEvent, attr);
    }
    if (attr->name() == SVGNames::xAttr) {
        x()->baseVal()->setValueAsString(value.impl());
    } else if (attr->name() == SVGNames::yAttr) {
        y()->baseVal()->setValueAsString(value.impl());
    } else if (attr->name() == SVGNames::widthAttr) {
        width()->baseVal()->setValueAsString(value.impl());
        addCSSProperty(attr, CSS_PROP_WIDTH, value);
    } else if (attr->name() == SVGNames::heightAttr) {
        height()->baseVal()->setValueAsString(value.impl());
        addCSSProperty(attr, CSS_PROP_HEIGHT, value);
    } else
    {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        if (SVGFitToViewBox::parseMappedAttribute(attr)) {
            if (renderer())
                static_cast<RenderSVGContainer*>(renderer())->setViewBox(FloatRect(viewBox()->baseVal()->x(), viewBox()->baseVal()->y(), viewBox()->baseVal()->width(), viewBox()->baseVal()->height()));
        }
        if(SVGZoomAndPan::parseMappedAttribute(attr)) return;

        SVGStyledLocatableElement::parseMappedAttribute(attr);
    }
}

unsigned long SVGSVGElement::suspendRedraw(unsigned long /* max_wait_milliseconds */)
{
    // TODO
    return 0;
}

void SVGSVGElement::unsuspendRedraw(unsigned long /* suspend_handle_id */, ExceptionCode& ec)
{
    // if suspend_handle_id is not found, throw exception
    // TODO
}

void SVGSVGElement::unsuspendRedrawAll()
{
    // TODO
}

void SVGSVGElement::forceRedraw()
{
    // TODO
}

NodeList *SVGSVGElement::getIntersectionList(const FloatRect& rect, SVGElement*)
{
    //NodeList *list;
    // TODO
    return 0;
}

NodeList *SVGSVGElement::getEnclosureList(const FloatRect& rect, SVGElement*)
{
    // TODO
    return 0;
}

bool SVGSVGElement::checkIntersection(SVGElement* element, const FloatRect& rect)
{
    // TODO : take into account pointer-events?
    // FIXME: Why is element ignored??
    return rect.intersects(getBBox());
}

bool SVGSVGElement::checkEnclosure(SVGElement* element, const FloatRect& rect)
{
    // TODO : take into account pointer-events?
    // FIXME: Why is element ignored??
    return rect.contains(getBBox());
}

void SVGSVGElement::deselectAll()
{
    // TODO
}

float SVGSVGElement::createSVGNumber()
{
    return 0;
}

SVGLength *SVGSVGElement::createSVGLength()
{
    return new SVGLength(0);
}

SVGAngle *SVGSVGElement::createSVGAngle()
{
    return new SVGAngle(0);
}

FloatPoint SVGSVGElement::createSVGPoint(const IntPoint &p)
{
    return FloatPoint(p);
}

SVGMatrix *SVGSVGElement::createSVGMatrix()
{
    return new SVGMatrix();
}

FloatRect SVGSVGElement::createSVGRect()
{
    return FloatRect();
}

SVGTransform *SVGSVGElement::createSVGTransform()
{
    return new SVGTransform();
}

SVGTransform *SVGSVGElement::createSVGTransformFromMatrix(SVGMatrix *matrix)
{    
    SVGTransform *obj = SVGSVGElement::createSVGTransform();
    obj->setMatrix(matrix);
    return obj;
}

SVGMatrix *SVGSVGElement::getCTM() const
{
    SVGMatrix *mat = createSVGMatrix();
    if(mat)
    {
        mat->translate(x()->baseVal()->value(), y()->baseVal()->value());

        if(attributes()->getNamedItem(SVGNames::viewBoxAttr))
        {
            RefPtr<SVGMatrix> viewBox = viewBoxToViewTransform(width()->baseVal()->value(), height()->baseVal()->value());
            mat->multiply(viewBox.get());
        }
    }

    return mat;
}

SVGMatrix *SVGSVGElement::getScreenCTM() const
{
    SVGMatrix *mat = SVGStyledLocatableElement::getScreenCTM();
    if(mat)
    {
        mat->translate(x()->baseVal()->value(), y()->baseVal()->value());

        if(attributes()->getNamedItem(SVGNames::viewBoxAttr))
        {
            RefPtr<SVGMatrix> viewBox = viewBoxToViewTransform(width()->baseVal()->value(), height()->baseVal()->value());
            mat->multiply(viewBox.get());
        }
    }

    return mat;
}

RenderObject* SVGSVGElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    RenderSVGContainer *rootContainer = new (arena) RenderSVGContainer(this);

    // FIXME: all this setup should be done after attributesChanged, not here.
    float _x = x()->baseVal()->value();
    float _y = y()->baseVal()->value();
    float _width = width()->baseVal()->value();
    float _height = height()->baseVal()->value();

    rootContainer->setViewport(FloatRect(_x, _y, _width, _height));
    rootContainer->setViewBox(FloatRect(viewBox()->baseVal()->x(), viewBox()->baseVal()->y(), viewBox()->baseVal()->width(), viewBox()->baseVal()->height()));
    rootContainer->setAlign(KCAlign(preserveAspectRatio()->baseVal()->align() - 1));
    rootContainer->setSlice(preserveAspectRatio()->baseVal()->meetOrSlice() == SVG_MEETORSLICE_SLICE);
    
    return rootContainer;
}

void SVGSVGElement::setZoomAndPan(unsigned short zoomAndPan)
{
    SVGZoomAndPan::setZoomAndPan(zoomAndPan);
    //canvasView()->enableZoomAndPan(zoomAndPan == SVG_ZOOMANDPAN_MAGNIFY);
}

void SVGSVGElement::pauseAnimations()
{
    if (!m_timeScheduler->animationsPaused())
        m_timeScheduler->toggleAnimations();
}

void SVGSVGElement::unpauseAnimations()
{
    if (m_timeScheduler->animationsPaused())
        m_timeScheduler->toggleAnimations();
}

bool SVGSVGElement::animationsPaused() const
{
    return m_timeScheduler->animationsPaused();
}

float SVGSVGElement::getCurrentTime() const
{
    return m_timeScheduler->elapsed();
}

void SVGSVGElement::setCurrentTime(float /* seconds */)
{
    // TODO
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

