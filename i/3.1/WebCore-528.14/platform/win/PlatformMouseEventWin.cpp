/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
 */

#include "PlatformMouseEvent.h"

#include <wtf/Assertions.h>
#include <windows.h>
#include <windowsx.h>

namespace WebCore {

#define HIGH_BIT_MASK_SHORT 0x8000

static IntPoint positionForEvent(HWND hWnd, LPARAM lParam)
{
    POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    return point;
}

static IntPoint globalPositionForEvent(HWND hWnd, LPARAM lParam)
{
    POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ClientToScreen(hWnd, &point);
    return point;
}

static MouseEventType messageToEventType(UINT message)
{
    switch (message) {
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
            //MSDN docs say double click is sent on mouse down
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            return MouseEventPressed;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            return MouseEventReleased;

        case WM_MOUSELEAVE:
        case WM_MOUSEMOVE:
            return MouseEventMoved;

        default:
            ASSERT_NOT_REACHED();
            //Move is relatively harmless
            return MouseEventMoved;
    }
}
PlatformMouseEvent::PlatformMouseEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool activatedWebView)
    : m_position(positionForEvent(hWnd, lParam))
    , m_globalPosition(globalPositionForEvent(hWnd, lParam))
    , m_clickCount(0)
    , m_shiftKey(wParam & MK_SHIFT)
    , m_ctrlKey(wParam & MK_CONTROL)
    , m_altKey(GetKeyState(VK_MENU) & HIGH_BIT_MASK_SHORT)
    , m_metaKey(m_altKey) // FIXME: We'll have to test other browsers
    , m_activatedWebView(activatedWebView)
    , m_eventType(messageToEventType(message))
    , m_modifierFlags(wParam)
{
    m_timestamp = ::GetTickCount()*0.001; // GetTickCount returns milliseconds

    switch (message) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            m_button = LeftButton;
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
            m_button = RightButton;
            break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
            m_button = MiddleButton;
            break;
        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
            if (wParam & MK_LBUTTON)
                m_button = LeftButton;
            else if (wParam & MK_MBUTTON)
                m_button = MiddleButton;
            else if (wParam & MK_RBUTTON)
                m_button = RightButton;
            else
                m_button = NoButton;
            break;
        default:
            ASSERT_NOT_REACHED();
    }
}

} // namespace WebCore
