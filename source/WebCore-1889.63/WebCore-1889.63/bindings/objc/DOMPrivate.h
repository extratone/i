/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#import <WebCore/DOM.h>

#if PLATFORM(IOS)
#import <WebCore/WebAutocapitalize.h>
#import <GraphicsServices/GraphicsServices.h>
#endif

@interface DOMNode (DOMNodeExtensionsPendingPublic)
#if !PLATFORM(IOS)
- (NSImage *)renderedImage;
#endif
- (NSArray *)textRects;
@end

@interface DOMNode (WebPrivate)
+ (id)_nodeFromJSWrapper:(JSObjectRef)jsWrapper;
@end

// FIXME: this should be removed as soon as all internal Apple uses of it have been replaced with
// calls to the public method - (NSColor *)color.
@interface DOMRGBColor (WebPrivate)
#if !PLATFORM(IOS)
- (NSColor *)_color;
#endif
@end

// FIXME: this should be removed as soon as all internal Apple uses of it have been replaced with
// calls to the public method - (NSString *)text.
@interface DOMRange (WebPrivate)
- (NSString *)_text;
@end

@interface DOMRange (DOMRangeExtensions)
#if PLATFORM(IOS)
- (CGRect)boundingBox;
#else
- (NSRect)boundingBox;
#endif
#if !PLATFORM(IOS)
- (NSImage *)renderedImageForcingBlackText:(BOOL)forceBlackText;
#else
- (CGImageRef)renderedImageForcingBlackText:(BOOL)forceBlackText;
#endif
- (NSArray *)lineBoxRects; // Deprecated. Use textRects instead.
- (NSArray *)textRects;
@end

@interface DOMElement (WebPrivate)
#if !PLATFORM(IOS)
- (NSFont *)_font;
- (NSData *)_imageTIFFRepresentation;
#else
- (GSFontRef)_font;
#endif
- (NSURL *)_getURLAttribute:(NSString *)name;
- (BOOL)isFocused;
@end

@interface DOMCSSStyleDeclaration (WebPrivate)
- (NSString *)_fontSizeDelta;
- (void)_setFontSizeDelta:(NSString *)fontSizeDelta;
@end

@interface DOMHTMLDocument (WebPrivate)
- (DOMDocumentFragment *)_createDocumentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString;
- (DOMDocumentFragment *)_createDocumentFragmentWithText:(NSString *)text;
@end

@interface DOMHTMLTableCellElement (WebPrivate)
- (DOMHTMLTableCellElement *)_cellAbove;
@end

// All the methods in this category are used by Safari forms autofill and should not be used for any other purpose.
// Each one should eventually be replaced by public DOM API, and when that happens Safari will switch to implementations 
// using that public API, and these will be deleted.
@interface DOMHTMLInputElement (FormAutoFillTransition)
- (BOOL)_isAutofilled;
- (BOOL)_isTextField;
#if !PLATFORM(IOS)
- (NSRect)_rectOnScreen; // bounding box of the text field, in screen coordinates
#endif
- (void)_replaceCharactersInRange:(NSRange)targetRange withString:(NSString *)replacementString selectingFromIndex:(int)index;
- (NSRange)_selectedRange;
- (void)_setAutofilled:(BOOL)filled;
@end

// These changes are necessary to detect whether a form input was modified by a user
// or javascript
@interface DOMHTMLInputElement (FormPromptAdditions)
- (BOOL)_isEdited;
@end

@interface DOMHTMLTextAreaElement (FormPromptAdditions)
- (BOOL)_isEdited;
@end

// All the methods in this category are used by Safari forms autofill and should not be used for any other purpose.
// They are stopgap measures until we finish transitioning form controls to not use NSView. Each one should become
// replaceable by public DOM API, and when that happens Safari will switch to implementations using that public API,
// and these will be deleted.
@interface DOMHTMLSelectElement (FormAutoFillTransition)
- (void)_activateItemAtIndex:(int)index;
- (void)_activateItemAtIndex:(int)index allowMultipleSelection:(BOOL)allowMultipleSelection;
@end

#if PLATFORM(IOS)
enum { WebMediaQueryOrientationCurrent, WebMediaQueryOrientationPortrait, WebMediaQueryOrientationLandscape };
@interface DOMHTMLLinkElement (WebPrivate)
- (BOOL)_mediaQueryMatchesForOrientation:(int)orientation;
- (BOOL)_mediaQueryMatches;
@end

// These changes are useful to get the AutocapitalizeType on particular form controls.
@interface DOMHTMLInputElement (AutocapitalizeAdditions)
- (WebAutocapitalizeType)_autocapitalizeType;
@end

@interface DOMHTMLTextAreaElement (AutocapitalizeAdditions)
- (WebAutocapitalizeType)_autocapitalizeType;
@end

// These are used by Date and Time input types because the generated ObjC methods default to not dispatching events.
@interface DOMHTMLInputElement (WebInputChangeEventAdditions)
- (void)setValueWithChangeEvent:(NSString *)newValue;
- (void)setValueAsNumberWithChangeEvent:(double)newValueAsNumber;
@end
#endif
