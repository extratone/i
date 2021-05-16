//
//  WAKStringDrawing.h
//  WebKit
//
//  Copyright (C) 2005, 2006, 2007, 2008, Apple Inc.  All rights reserved.
//

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <GraphicsServices/GraphicsServices.h>

typedef enum {
    // The order of the enum items is important, and it is used for >= comparisions
    WebEllipsisStyleNone = 0, // Old style, no truncation. Doesn't respect the "width" passed to it. Left in for compatability.
    WebEllipsisStyleHead = 1,
    WebEllipsisStyleTail = 2,
    WebEllipsisStyleCenter = 3,
    WebEllipsisStyleClip = 4, // Doesn't really clip, but instad truncates at the last character.
    WebEllipsisStyleWordWrap = 5, // Truncates based on the width/height passed to it.
    WebEllipsisStyleCharacterWrap = 6, // For "drawAtPoint", it is just like WebEllipsisStyleClip, since it doesn't really clip, but truncates at the last character
} WebEllipsisStyle;

typedef enum {
    WebTextAlignmentLeft = 0,
    WebTextAlignmentCenter = 1,
    WebTextAlignmentRight = 2,
} WebTextAlignment;

@interface NSString (WebStringDrawing)

+ (void)_web_setWordRoundingEnabled:(BOOL)flag;
+ (BOOL)_web_wordRoundingEnabled;

- (CGSize)_web_drawAtPoint:(CGPoint)point withFont:(GSFontRef)font;

- (CGSize)_web_sizeWithFont:(GSFontRef)font;

// Size after applying ellipsis style and clipping to width.
- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle;
- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing;

// Draw text to fit width.  Clip or apply ellipsis according to style.
- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle;
- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing;
- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle letterSpacing:(float)letterSpacing includeEmoji:(BOOL)includeEmoji;

// Wrap and clip to rect.
- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment;
- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment lineSpacing:(int)lineSpacing;
- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment lineSpacing:(int)lineSpacing includeEmoji:(BOOL)includeEmoji;
- (CGSize)_web_sizeInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle;
- (CGSize)_web_sizeInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle lineSpacing:(int)lineSpacing;

// Determine the secured version of this string
- (NSString *)_web_securedStringIncludingLastCharacter:(BOOL)includingLastCharacter;

@end
