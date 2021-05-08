/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "WMLTimerElement.h"

#include "HTMLNames.h"
#include "WMLCardElement.h"
#include "WMLDocument.h"
#include "WMLNames.h"
#include "WMLPageState.h"
#include "WMLTemplateElement.h"
#include "WMLVariables.h"

namespace WebCore {

using namespace WMLNames;

WMLTimerElement::WMLTimerElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
    , m_timer(this, &WMLTimerElement::timerFired)
{
}

void WMLTimerElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::nameAttr)
        m_name = parseValueForbiddingVariableReferences(attr->value());
    else if (attr->name() == HTMLNames::valueAttr)
        m_value = parseValueSubstitutingVariableReferences(attr->value());
    else
        WMLElement::parseMappedAttribute(attr);
}

void WMLTimerElement::insertedIntoDocument()
{
    WMLElement::insertedIntoDocument();

    // If the value of timeout is not a positive integer, ignore it
    if (m_value.toInt() <= 0)
        return;

    Node* parent = parentNode();
    ASSERT(parent);

    if (!parent || !parent->isWMLElement())
        return;

    if (parent->hasTagName(cardTag)) {
        m_card = static_cast<WMLCardElement*>(parent);
        m_card->setIntrinsicEventTimer(this);
    }
}

void WMLTimerElement::timerFired(Timer<WMLTimerElement>*)
{
    if (!m_card)
        return;

    WMLPageState* pageState = wmlPageStateForDocument(document());
    if (!pageState)
        return;

    // When the timer expires, set the name varialbe of timer to '0'
    if (!m_name.isEmpty()) {
        m_value = "0";
        pageState->storeVariable(m_name, m_value);
    }

    WMLIntrinsicEventType eventType = WMLIntrinsicEventOnTimer;
    WMLIntrinsicEventHandler* eventHandler = m_card->eventHandler();

    bool hasIntrinsicEvent = false;
    if (eventHandler && eventHandler->hasIntrinsicEvent(eventType))
        hasIntrinsicEvent = true;
    else if (m_card->templateElement()) {
        eventHandler = m_card->templateElement()->eventHandler();
        if (eventHandler && eventHandler->hasIntrinsicEvent(eventType))
            hasIntrinsicEvent = true;
    }

    if (hasIntrinsicEvent)
        eventHandler->triggerIntrinsicEvent(eventType);
}

void WMLTimerElement::start(int interval)
{
    if (!m_card || m_timer.isActive())
        return;

    if (interval <= 0 && !m_name.isEmpty()) {
        if (WMLPageState* pageState = wmlPageStateForDocument(document()))
            interval = pageState->getVariable(m_name).toInt();
    }

    if (interval <= 0)
        interval = m_value.toInt();

    if (interval > 0)
        m_timer.startOneShot(interval / 10.0f);
}

void WMLTimerElement::stop()
{
    if (m_timer.isActive())
        m_timer.stop();
}

void WMLTimerElement::storeIntervalToPageState()
{
    if (!m_timer.isActive())
        return;

    int interval = static_cast<int>(m_timer.nextFireInterval()) * 10;

    if (WMLPageState* pageState = wmlPageStateForDocument(document()))
        pageState->storeVariable(m_name, String::number(interval));
}

}

#endif
