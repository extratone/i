/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "PlatformScreen.h"

#import "FloatRect.h"
#import "FrameView.h"
#import "IntRect.h"
#import "WebCoreSystemInterface.h"
#import "Widget.h"
#import "NotImplemented.h"

#import "WAKWindow.h"

namespace WebCore {

int screenHorizontalDPI(Widget*)
{
    notImplemented();
    return 0;
}

int screenVerticalDPI(Widget*)
{
    notImplemented();
    return 0;
}

int screenDepth(Widget*)
{
    // Assume 32 bits per pixel.  See <rdar://problem/9378829>.
    return 32;
}

int screenDepthPerComponent(Widget*)
{
    // Assume the screen depth is evenly divided into four color components.  See <rdar://problem/9378829>.
    return screenDepth(0) / 4;
}

bool screenIsMonochrome(Widget*)
{
    return NO;
}

// These functions scale between screen and page coordinates because JavaScript/DOM operations 
// assume that the screen and the page share the same coordinate system.
FloatRect screenRect(Widget* widget)
{
    if (!widget)
        return CGRectZero;
    WAKWindow *window = [widget->platformWidget() window];
    if (!window)
        return [widget->platformWidget() frame];
    CGRect screenRect = { CGPointZero, [window screenSize] };
    return enclosingIntRect(screenRect);
}

FloatRect screenAvailableRect(Widget* widget)
{
    if (!widget)
        return CGRectZero;
    WAKWindow *window = [widget->platformWidget() window];
    if (!window)
        return CGRectZero;
    CGRect screenRect = { CGPointZero, [window availableScreenSize] };
    return enclosingIntRect(screenRect);
}

} // namespace WebCore
