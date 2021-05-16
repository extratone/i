/*
 * Copyright 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include <objc/objc.h>


#include <GraphicsServices/GraphicsServices.h>

#ifdef __OBJC__
@class NSButtonCell;
@class NSData;
@class NSEvent;
@class NSFont;
@class NSMutableURLRequest;
@class NSURLRequest;
@class QTMovie;
@class QTMovieView;
#else
typedef struct NSArray NSArray;
typedef struct NSButtonCell NSButtonCell;
typedef struct NSData NSData;
typedef struct NSDate NSDate;
typedef struct NSEvent NSEvent;
typedef struct NSFont NSFont;
typedef struct NSImage NSImage;
typedef struct NSMenu NSMenu;
typedef struct NSMutableURLRequest NSMutableURLRequest;
typedef struct NSURLRequest NSURLRequest;
typedef struct NSString NSString;
typedef struct NSTextFieldCell NSTextFieldCell;
typedef struct NSURLConnection NSURLConnection;
typedef struct NSURLResponse NSURLResponse;
typedef struct NSView NSView;
typedef struct objc_object *id;
typedef struct QTMovie QTMovie;
typedef struct QTMovieView QTMovieView;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// In alphabetical order.

extern void (*wkAdvanceDefaultButtonPulseAnimation)(NSButtonCell *);
extern BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef);
extern CFReadStreamRef (*wkCreateCustomCFReadStream)(void *(*formCreate)(CFReadStreamRef, void *), 
    void (*formFinalize)(CFReadStreamRef, void *), 
    Boolean (*formOpen)(CFReadStreamRef, CFStreamError *, Boolean *, void *), 
    CFIndex (*formRead)(CFReadStreamRef, UInt8 *, CFIndex, CFStreamError *, Boolean *, void *), 
    Boolean (*formCanRead)(CFReadStreamRef, void *), 
    void (*formClose)(CFReadStreamRef, void *), 
    void (*formSchedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *), 
    void (*formUnschedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *),
    void *context);
extern id (*wkCreateNSURLConnectionDelegateProxy)(void);
extern BOOL (*wkGetGlyphTransformedAdvances)(GSFontRef font, CGAffineTransform *m, CGGlyph *glyph, CGSize *advance);
extern NSString* (*wkGetMIMETypeForExtension)(NSString*);
extern NSDate *(*wkGetNSURLResponseLastModifiedDate)(NSURLResponse *response);
extern void (*wkSetNSURLConnectionDefersCallbacks)(NSURLConnection *, BOOL);
extern void (*wkSetNSURLRequestShouldContentSniff)(NSMutableURLRequest *, BOOL);
extern void (*wkSetPatternBaseCTM)(CGContextRef, CGAffineTransform);
extern void (*wkSetPatternPhaseInUserSpace)(CGContextRef, CGPoint);
extern void (*wkSetUpFontCache)();
extern void (*wkSignalCFReadStreamEnd)(CFReadStreamRef stream);
extern void (*wkSignalCFReadStreamError)(CFReadStreamRef stream, CFStreamError *error);
extern void (*wkSignalCFReadStreamHasBytes)(CFReadStreamRef stream);
extern unsigned (*wkInitializeMaximumHTTPConnectionCountPerHost)(unsigned preferredConnectionCount);

#ifndef BUILDING_ON_TIGER
extern void (*wkGetGlyphsForCharacters)(CGFontRef, const UniChar[], CGGlyph[], size_t);
#else
#define GLYPH_VECTOR_SIZE (50 * 32)

extern void (*wkClearGlyphVector)(void* glyphs);
extern OSStatus (*wkConvertCharToGlyphs)(void* styleGroup, const UniChar*, unsigned numCharacters, void* glyphs);
extern CFStringRef (*wkCopyFullFontName)(CGFontRef font);
extern OSStatus (*wkGetATSStyleGroup)(ATSUStyle, void** styleGroup);
extern CGFontRef (*wkGetCGFontFromNSFont)(NSFont*);
extern void (*wkGetFontMetrics)(CGFontRef, int* ascent, int* descent, int* lineGap, unsigned* unitsPerEm);
extern void* wkGetGlyphsForCharacters;
extern int (*wkGetGlyphVectorNumGlyphs)(void* glyphVector);
extern size_t (*wkGetGlyphVectorRecordSize)(void* glyphVector);
extern OSStatus (*wkInitializeGlyphVector)(int count, void* glyphs);
extern void (*wkReleaseStyleGroup)(void* group);
extern BOOL (*wkSupportsMultipartXMixedReplace)(NSMutableURLRequest *);
#endif

extern BOOL (*wkUseSharedMediaUI)();

#ifdef __cplusplus
}
#endif

#endif
