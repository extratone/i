/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#include "config.h"

#if ENABLE(WML)
#include "WMLAElement.h"

#include "DNS.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "RenderFlow.h"
#include "WMLNames.h"

namespace WebCore {

using namespace WMLNames;

WMLAElement::WMLAElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

void WMLAElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::hrefAttr) {
        bool wasLink = isLink();
        setIsLink(!attr->isNull());
        if (wasLink != isLink())
            setChanged();
        if (isLink() && document()->isDNSPrefetchEnabled()) {
            String value = attr->value();
            if (protocolIs(value, "http") || protocolIs(value, "https") || value.startsWith("//"))
                prefetchDNS(document()->completeURL(value).host());
        }
    } else if (attr->name() == HTMLNames::nameAttr
               || attr->name() == HTMLNames::titleAttr
               || attr->name() == HTMLNames::relAttr) {
        // Do nothing.
    } else
        WMLElement::parseMappedAttribute(attr);
}

bool WMLAElement::supportsFocus() const
{
    return isFocusable() || (isLink() && document() && !document()->haveStylesheetsLoaded());
}

bool WMLAElement::isFocusable() const
{
    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Dave wants to fix that some day with a "has visible content" flag or the like.
    if (!(isLink() && renderer() && renderer()->style()->visibility() == VISIBLE))
        return false;

    return true;
}

bool WMLAElement::isMouseFocusable() const
{
    return false;
}

bool WMLAElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (!isFocusable())
        return false;
    
    if (!document()->frame())
        return false;

    if (!document()->frame()->eventHandler()->tabsToLinks(event))
        return false;

    // Before calling absoluteRects, check for the common case where the renderer
    // or one of the continuations is non-empty, since this is a faster check and
    // almost always returns true.
    RenderBox* box = toRenderBox(renderer());
    if (!box->borderBoundingBox().isEmpty())
        return true;
    for (RenderFlow* r = box->virtualContinuation(); r; r = r->continuation())
        if (!r->borderBoundingBox().isEmpty())
            return true;

    Vector<IntRect> rects;
    FloatPoint absPos = renderer()->localToAbsolute();
    renderer()->absoluteRects(rects, absPos.x(), absPos.y());
    size_t n = rects.size();
    for (size_t i = 0; i < n; ++i)
        if (!rects[i].isEmpty())
            return true;

    return false;
}

void WMLAElement::defaultEventHandler(Event* event)
{
    if (isLink() && (event->type() == eventNames().clickEvent || (event->type() == eventNames().keydownEvent && focused()))) {
        MouseEvent* e = 0;
        if (event->type() == eventNames().clickEvent && event->isMouseEvent())
            e = static_cast<MouseEvent*>(event);

        KeyboardEvent* k = 0;
        if (event->type() == eventNames().keydownEvent && event->isKeyboardEvent())
            k = static_cast<KeyboardEvent*>(event);

        if (e && e->button() == RightButton) {
            WMLElement::defaultEventHandler(event);
            return;
        }

        if (k) {
            if (k->keyIdentifier() != "Enter") {
                WMLElement::defaultEventHandler(event);
                return;
            }

            event->setDefaultHandled();
            dispatchSimulatedClick(event);
            return;
        }
 
        if (!event->defaultPrevented() && document()->frame()) {
            KURL url = document()->completeURL(parseURL(getAttribute(HTMLNames::hrefAttr)));
            document()->frame()->loader()->urlSelected(url, target(), event, false, true);
        }

        event->setDefaultHandled();
    }

    WMLElement::defaultEventHandler(event);
}

void WMLAElement::accessKeyAction(bool sendToAnyElement)
{
    // send the mouse button events if the caller specified sendToAnyElement
    dispatchSimulatedClick(0, sendToAnyElement);
}

bool WMLAElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == HTMLNames::hrefAttr;
}

String WMLAElement::target() const
{
    return getAttribute(HTMLNames::targetAttr);
}

}

#endif
