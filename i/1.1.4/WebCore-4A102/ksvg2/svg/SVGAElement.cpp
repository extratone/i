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
#include "SVGAElement.h"

#include "Attr.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "MouseEvent.h"
#include "MouseEvent.h"
#include "SVGAnimatedString.h"
#include "SVGHelper.h"
#include "SVGNames.h"
#include "csshelper.h"
#include <kcanvas/RenderSVGContainer.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/device/KRenderingDevice.h>

namespace WebCore {

SVGAElement::SVGAElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGURIReference(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired()
{
}

SVGAElement::~SVGAElement()
{
}

SVGAnimatedString *SVGAElement::target() const
{
    return lazy_create<SVGAnimatedString>(m_target, this);
}

void SVGAElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value(attr->value());
    if (attr->name() == SVGNames::targetAttr) {
        target()->setBaseVal(value.impl());
    } else {
        if(SVGURIReference::parseMappedAttribute(attr))
        {
            m_isLink = attr->value() != 0;
            return;
        }
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;        
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

RenderObject* SVGAElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderSVGContainer(this);
}

void SVGAElement::defaultEventHandler(Event *evt)
{
    // TODO : should use CLICK instead
    if((evt->type() == EventNames::mouseupEvent && m_isLink))
    {
        MouseEvent *e = static_cast<MouseEvent*>(evt);

        DeprecatedString url;
        DeprecatedString utarget;
        if(e && e->button() == 2)
        {
            SVGStyledTransformableElement::defaultEventHandler(evt);
            return;
        }
        url = parseURL(href()->baseVal()).deprecatedString();
        utarget = getAttribute(SVGNames::targetAttr).deprecatedString();

        if(e && e->button() == 1)
            utarget = "_blank";

        if (!evt->defaultPrevented()) {
            if(ownerDocument() && ownerDocument()->view() && ownerDocument()->frame())
            {
                //document()->view()->resetCursor();
                document()->frame()->urlSelected(url, utarget);
            }
        }

        evt->setDefaultHandled();
    }

    SVGStyledTransformableElement::defaultEventHandler(evt);
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT
