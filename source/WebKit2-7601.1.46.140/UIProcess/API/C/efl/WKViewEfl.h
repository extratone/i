/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WKViewEfl_h
#define WKViewEfl_h

#include <WebKit/WKBase.h>

typedef struct _Evas_Event_Mouse_Down Evas_Event_Mouse_Down;
typedef struct _Evas_Event_Mouse_Move Evas_Event_Mouse_Move;
typedef struct _Evas_Event_Mouse_Up Evas_Event_Mouse_Up;
typedef struct _cairo_surface cairo_surface_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*WKShowColorPickerCallback)(WKViewRef view, WKStringRef initialColor, WKColorPickerResultListenerRef listener, const void* clientInfo);
typedef void (*WKHideColorPickerCallback)(WKViewRef view, const void* clientInfo);

typedef struct WKColorPickerClientBase {
    int                                            version;
    const void *                                   clientInfo;
} WKColorPickerClientBase;

typedef struct WKColorPickerClientV0 {
    WKColorPickerClientBase                        base;
    WKShowColorPickerCallback                      showColorPicker;
    WKHideColorPickerCallback                      endColorPicker;
} WKColorPickerClientV0;

WK_EXPORT void WKViewSetColorPickerClient(WKViewRef page, const WKColorPickerClientBase* client);

WK_EXPORT void WKViewPaintToCairoSurface(WKViewRef, cairo_surface_t*);

WK_EXPORT WKImageRef WKViewCreateSnapshot(WKViewRef);

WK_EXPORT void WKViewSetThemePath(WKViewRef, WKStringRef);

WK_EXPORT void WKViewSendTouchEvent(WKViewRef, WKTouchEventRef);

WK_EXPORT void WKViewSendMouseDownEvent(WKViewRef, Evas_Event_Mouse_Down*);
WK_EXPORT void WKViewSendMouseUpEvent(WKViewRef, Evas_Event_Mouse_Up*);
WK_EXPORT void WKViewSendMouseMoveEvent(WKViewRef, Evas_Event_Mouse_Move*);

WK_EXPORT void WKViewSetBackgroundColor(WKViewRef, int red, int green, int blue, int alpha);
WK_EXPORT void WKViewGetBackgroundColor(WKViewRef, int* red, int* green, int* blue, int* alpha);

#ifdef __cplusplus
}
#endif

#endif /* WKViewEfl_h */
