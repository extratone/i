/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
 */

#include "config.h"
#include "HTMLFrameElement.h"

#include "Frame.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
#include "RenderFrame.h"

namespace WebCore {

using namespace HTMLNames;

HTMLFrameElement::HTMLFrameElement(const QualifiedName& tagName, Document* doc, bool createdByParser)
    : HTMLFrameElementBase(tagName, doc, createdByParser)
    , m_frameBorder(true)
    , m_frameBorderSet(false)
{
    ASSERT(hasTagName(frameTag));
}

bool HTMLFrameElement::rendererIsNeeded(RenderStyle*)
{
    // For compatibility, frames render even when display: none is set.
    return isURLAllowed(m_URL);
}

RenderObject* HTMLFrameElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderFrame(this);
}

static inline HTMLFrameSetElement* containingFrameSetElement(Node* node)
{
    while ((node = node->parentNode()))
        if (node->hasTagName(framesetTag))
            return static_cast<HTMLFrameSetElement*>(node);
    return 0;
}

void HTMLFrameElement::attach()
{
    HTMLFrameElementBase::attach();
    
    if (HTMLFrameSetElement* frameSetElement = containingFrameSetElement(this)) {
        if (!m_frameBorderSet)
            m_frameBorder = frameSetElement->hasFrameBorder();
        if (!m_noResize)
            m_noResize = frameSetElement->noResize();
    }
}

void HTMLFrameElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == frameborderAttr) {
        m_frameBorder = attr->value().toInt();
        m_frameBorderSet = !attr->isNull();
        // FIXME: If we are already attached, this has no effect.
    } else
        HTMLFrameElementBase::parseMappedAttribute(attr);
}

} // namespace WebCore
