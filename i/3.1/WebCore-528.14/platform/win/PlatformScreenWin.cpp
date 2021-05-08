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

#include "config.h"
#include "PlatformScreen.h"

#include "IntRect.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include <windows.h>

namespace WebCore {

// Returns info for the default monitor if widget is NULL
static MONITORINFOEX monitorInfoForWidget(Widget* widget)
{
    HWND window = widget ? widget->root()->hostWindow()->platformWindow() : 0;
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(monitor, &monitorInfo);
    return monitorInfo;
}

static DEVMODE deviceInfoForWidget(Widget* widget)
{
    MONITORINFOEX monitorInfo = monitorInfoForWidget(widget);

    DEVMODE deviceInfo;
    deviceInfo.dmSize = sizeof(DEVMODE);
    deviceInfo.dmDriverExtra = 0;
    EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &deviceInfo);

    return deviceInfo;
}

int screenDepth(Widget* widget)
{
    DEVMODE deviceInfo = deviceInfoForWidget(widget);
    return deviceInfo.dmBitsPerPel;
}

int screenDepthPerComponent(Widget* widget)
{
    // FIXME: Assumes RGB -- not sure if this is right.
    DEVMODE deviceInfo = deviceInfoForWidget(widget);
    return deviceInfo.dmBitsPerPel / 3;
}

bool screenIsMonochrome(Widget* widget)
{
    DEVMODE deviceInfo = deviceInfoForWidget(widget);
    return deviceInfo.dmColor == DMCOLOR_MONOCHROME;
}

FloatRect screenRect(Widget* widget)
{
    MONITORINFOEX monitorInfo = monitorInfoForWidget(widget);
    return monitorInfo.rcMonitor;
}

FloatRect screenAvailableRect(Widget* widget)
{
    MONITORINFOEX monitorInfo = monitorInfoForWidget(widget);
    return monitorInfo.rcWork;
}

} // namespace WebCore
