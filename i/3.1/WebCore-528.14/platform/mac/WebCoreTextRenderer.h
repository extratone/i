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
#ifdef __cplusplus
extern "C" {
#endif


#import <GraphicsServices/GSFont.h>

extern void WebCoreDrawTextAtPoint(const UniChar*, unsigned length, CGPoint, GSFontRef, CGColorRef);
extern float WebCoreTextFloatWidth(const UniChar*, unsigned length, GSFontRef);
extern void WebCoreSetShouldUseFontSmoothing(bool);
extern bool WebCoreShouldUseFontSmoothing();
extern void WebCoreSetAlwaysUsesComplexTextCodePath(bool);
extern bool WebCoreAlwaysUsesComplexTextCodePath();
extern GSFontRef WebCoreFindFont(NSString* familyName, GSFontTraitMask, int weight, int size);

extern void WebCoreSetFontSmoothingStyle(CGFontSmoothingStyle newStyle);
extern CGFontSmoothingStyle WebCoreFontSmoothingStyle();
extern void WebCoreSetFontAntialiasingStyle(CGFontAntialiasingStyle newStyle);
extern CGFontAntialiasingStyle WebCoreFontAntialiasingStyle();


#ifdef __cplusplus
}
#endif
