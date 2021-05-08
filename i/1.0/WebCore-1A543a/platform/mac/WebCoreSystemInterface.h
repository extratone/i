/*
 * Copyright 2006 Apple Computer, Inc. All rights reserved.
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

#ifndef WebCoreSystemInterface_h
#define WebCoreSystemInterface_h

typedef signed char BOOL;

#ifndef CGGEOMETRY_H_
typedef struct CGRect CGRect;
#endif
#include <GraphicsServices/GraphicsServices.h>

#ifndef __OBJC__
class NSImage;
class NSMenu;
class NSString;
class NSView;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GLYPH_VECTOR_SIZE (50 * 32)

// In alphabetical order.

extern BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef);
extern void (*wkClearGlyphVector)(void* glyphs);
extern OSStatus (*wkConvertCharToGlyphs)(void* styleGroup, const UniChar*, unsigned numCharacters, void* glyphs);
extern BOOL (*wkGetGlyphTransformedAdvances)(GSFontRef font, CGAffineTransform *m, CGGlyph *glyph, CGSize *advance);
extern int (*wkGetGlyphVectorNumGlyphs)(void* glyphVector);
extern size_t (*wkGetGlyphVectorRecordSize)(void* glyphVector);
extern void (*wkSetPatternPhaseInUserSpace)(CGContextRef, CGPoint point);
extern void (*wkSetUpFontCache)(size_t);

extern void (*wkAssistControl)(id anAssistedControl);

#ifdef __cplusplus
}
#endif

#endif