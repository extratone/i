/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"
#include "WindowEventContext.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "EventContext.h"
#include "Node.h"

namespace WebCore {

WindowEventContext::WindowEventContext(Event* event, PassRefPtr<Node> node, const EventContext* topEventContext)
{
    // We don't dispatch load events to the window. This quirk was originally
    // added because Mozilla doesn't propagate load events to the window object.
    if (event->type() == eventNames().loadEvent)
        return;

    Node* topLevelContainer = topEventContext ? topEventContext->node() : node.get();
    if (!topLevelContainer->isDocumentNode())
        return;

    m_window = toDocument(topLevelContainer)->domWindow();
    m_target = topEventContext ? topEventContext->target() : node.get();
}

bool WindowEventContext::handleLocalEvents(Event* event)
{
    if (!m_window)
        return false;

    event->setTarget(target());
    event->setCurrentTarget(window());
    m_window->fireEventListeners(event);
    return true;
}

}
