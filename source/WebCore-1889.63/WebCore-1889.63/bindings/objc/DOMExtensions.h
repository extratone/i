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

#import <WebCore/DOMAttr.h>
#import <WebCore/DOMCSS.h>
#import <WebCore/DOMCSSStyleDeclaration.h>
#import <WebCore/DOMDocument.h>
#import <WebCore/DOMElement.h>
#import <WebCore/DOMHTML.h>
#import <WebCore/DOMHTMLAnchorElement.h>
#import <WebCore/DOMHTMLAreaElement.h>
#import <WebCore/DOMHTMLDocument.h>
#import <WebCore/DOMHTMLElement.h>
#import <WebCore/DOMHTMLEmbedElement.h>
#import <WebCore/DOMHTMLImageElement.h>
#import <WebCore/DOMHTMLInputElement.h>
#import <WebCore/DOMHTMLLinkElement.h>
#import <WebCore/DOMHTMLObjectElement.h>
#import <WebCore/DOMNode.h>
#import <WebCore/DOMRGBColor.h>
#import <WebCore/DOMRange.h>

#if PLATFORM(IOS)
#import <CoreGraphics/CoreGraphics.h>
#endif

@class NSArray;
@class NSImage;
@class NSURL;


#if PLATFORM(IOS)
@interface DOMHTMLElement (DOMHTMLElementExtensions)
- (int)scrollXOffset;
- (int)scrollYOffset;
- (void)setScrollXOffset:(int)x scrollYOffset:(int)y;
- (void)setScrollXOffset:(int)x scrollYOffset:(int)y adjustForPurpleCaret:(BOOL)adjustForPurpleCaret;
- (void)absolutePosition:(int *)x :(int *)y :(int *)w :(int *)h;
@end
#endif

#if PLATFORM(IOS)
typedef struct _WKQuad {
    CGPoint p1;
    CGPoint p2;
    CGPoint p3;
    CGPoint p4;
} WKQuad;

@interface WKQuadObject : NSObject
{
    WKQuad  _quad;
}

- (id)initWithQuad:(WKQuad)quad;
- (WKQuad)quad;
- (CGRect)boundingBox;
@end
#endif

@interface DOMNode (DOMNodeExtensions)
#if PLATFORM(IOS)
- (CGRect)boundingBox;
#else
- (NSRect)boundingBox WEBKIT_OBJC_METHOD_ANNOTATION(AVAILABLE_WEBKIT_VERSION_3_0_AND_LATER);
#endif
- (NSArray *)lineBoxRects WEBKIT_OBJC_METHOD_ANNOTATION(AVAILABLE_WEBKIT_VERSION_3_0_AND_LATER);

#if PLATFORM(IOS)
- (CGRect)boundingBoxUsingTransforms; // takes transforms into account

- (WKQuad)absoluteQuad;
- (WKQuad)absoluteQuadAndInsideFixedPosition:(BOOL *)insideFixed;
- (NSArray *)lineBoxQuads;      // returns array of WKQuadObject

- (NSURL *)hrefURL;
- (CGRect)hrefFrame;
- (NSString *)hrefTarget;
- (NSString *)hrefLabel;
- (NSString *)hrefTitle;
- (CGRect)boundingFrame;
- (WKQuad)innerFrameQuad;       // takes transforms into account
- (float)computedFontSize;
- (DOMNode *)nextFocusNode;
- (DOMNode *)previousFocusNode;
#endif
@end

@interface DOMElement (DOMElementAppKitExtensions)
#if !PLATFORM(IOS)
- (NSImage *)image WEBKIT_OBJC_METHOD_ANNOTATION(AVAILABLE_WEBKIT_VERSION_3_0_AND_LATER);
#endif
@end

@interface DOMHTMLDocument (DOMHTMLDocumentExtensions)
- (DOMDocumentFragment *)createDocumentFragmentWithMarkupString:(NSString *)markupString baseURL:(NSURL *)baseURL WEBKIT_OBJC_METHOD_ANNOTATION(AVAILABLE_WEBKIT_VERSION_3_0_AND_LATER);
- (DOMDocumentFragment *)createDocumentFragmentWithText:(NSString *)text WEBKIT_OBJC_METHOD_ANNOTATION(AVAILABLE_WEBKIT_VERSION_3_0_AND_LATER);
@end

#if PLATFORM(IOS)
@interface DOMHTMLAreaElement (DOMHTMLAreaElementExtensions)
- (CGRect)boundingFrameForOwner:(DOMNode *)anOwner;
@end

@interface DOMHTMLSelectElement (DOMHTMLSelectElementExtensions)
- (DOMNode *)listItemAtIndex:(int)anIndex;
@end
#endif
