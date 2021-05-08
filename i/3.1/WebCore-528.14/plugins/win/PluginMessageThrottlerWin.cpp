/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PluginMessageThrottlerWin.h"

#include "PluginView.h"
#include <wtf/ASCIICType.h>

using namespace WTF;

namespace WebCore {

static const double MessageThrottleTimeInterval = 0.001;

PluginMessageThrottlerWin::PluginMessageThrottlerWin(PluginView* pluginView)
    : m_back(0), m_front(0)
    , m_pluginView(pluginView)
    , m_messageThrottleTimer(this, &PluginMessageThrottlerWin::messageThrottleTimerFired)
{
    // Initialize the free list with our inline messages
    for (unsigned i = 0; i < NumInlineMessages - 1; i++)
        m_inlineMessages[i].next = &m_inlineMessages[i + 1];
    m_inlineMessages[NumInlineMessages - 1].next = 0;
    m_freeInlineMessages = &m_inlineMessages[0];
}

PluginMessageThrottlerWin::~PluginMessageThrottlerWin()
{
    PluginMessage* next;

    for (PluginMessage* message = m_front; message; message = next) {
        next = message->next;
        freeMessage(message);
    }
}

void PluginMessageThrottlerWin::appendMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PluginMessage* message = allocateMessage();

    message->hWnd = hWnd;
    message->msg = msg;
    message->wParam = wParam;
    message->lParam = lParam;
    message->next = 0;

    if (m_back)
        m_back->next = message;
    m_back = message;
    if (!m_front)
        m_front = message;

    if (!m_messageThrottleTimer.isActive())
        m_messageThrottleTimer.startOneShot(MessageThrottleTimeInterval);
}

void PluginMessageThrottlerWin::messageThrottleTimerFired(Timer<PluginMessageThrottlerWin>*)
{
    PluginMessage* message = m_front;
    m_front = m_front->next;
    if (message == m_back)
        m_back = 0;

    ::CallWindowProc(m_pluginView->pluginWndProc(), message->hWnd, message->msg, message->wParam, message->lParam);

    freeMessage(message);

    if (m_front)
        m_messageThrottleTimer.startOneShot(MessageThrottleTimeInterval);
}

PluginMessage* PluginMessageThrottlerWin::allocateMessage()
{
    PluginMessage *message;

    if (m_freeInlineMessages) {
        message = m_freeInlineMessages;
        m_freeInlineMessages = message->next;
    } else
        message = new PluginMessage;

    return message;
}

bool PluginMessageThrottlerWin::isInlineMessage(PluginMessage* message)
{
    return message >= &m_inlineMessages[0] && message <= &m_inlineMessages[NumInlineMessages - 1];
}

void PluginMessageThrottlerWin::freeMessage(PluginMessage* message)
{
    if (isInlineMessage(message)) {
        message->next = m_freeInlineMessages;
        m_freeInlineMessages = message;
    } else
        delete message;
}

} // namespace WebCore
