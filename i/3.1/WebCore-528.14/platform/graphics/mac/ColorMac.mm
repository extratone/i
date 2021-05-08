/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "Color.h"
#import "ColorMac.h"

#import <wtf/Assertions.h>
#import <wtf/StdLibExtras.h>
#import <wtf/RetainPtr.h>

#import <GraphicsServices/GSColor.h>
#import <wtf/UnusedParam.h>


namespace WebCore {


CGColorRef cgColor(const Color &color)
{
    unsigned c = color.rgb();
    switch (c) {
        case 0: {
            return GSColorForSystemColor(kGSClearColor);
        }
        case Color::black: {
            return GSColorForSystemColor(kGSBlackColor);
        }
        case Color::white: {
            return GSColorForSystemColor(kGSWhiteColor);
        }
        default: {
            const int cacheSize = 32;
            static unsigned cachedRGBAValues[cacheSize];
            static CGColorRef cachedColors[cacheSize];
            
            for (int i = 0; i != cacheSize; ++i) {
                if (cachedRGBAValues[i] == c)
                    return CGColorRetain(cachedColors[i]);
            }
            
            CGColorRef result =  GSColorCreateColorWithDeviceRGBA(color.red() / 255.f, color.green() / 255.f, color.blue() / 255.f, color.alpha() / 255.f);
            
            static int cursor;
            cachedRGBAValues[cursor] = c;
            CGColorRelease(cachedColors[cursor]);
            cachedColors[cursor] = result;
            if (++cursor == cacheSize)
                cursor = 0;
            
            return CGColorRetain(result);
        }
    }
    
    return NULL;
}

Color focusRingColor()
{
    return 0xFF9CABBD;
}

bool usesTestModeFocusRingColor()
{
    return false;
}

void setUsesTestModeFocusRingColor(bool newValue)
{
    UNUSED_PARAM(newValue);
}

}

