//
//  WAKStringDrawing.h
//  WebKit
//
//  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
//

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <GraphicsServices/GraphicsServices.h>

typedef enum {
    WebEllipsisStyleNone = 0,
    WebEllipsisStyleHead = 1,
    WebEllipsisStyleTail = 2,
    WebEllipsisStyleCenter = 3
} WebEllipsisStyle;

typedef enum {
    WebTextAlignmentLeft = 0,
    WebTextAlignmentCenter = 1
} WebTextAlignment;

@interface NSString (WebStringDrawing)

- (CGSize)_web_drawAtPoint:(CGPoint)point withFont:(GSFontRef)font;

- (CGSize)_web_sizeWithFont:(GSFontRef)font;

// Size after applying ellipsis style and clipping to width.
- (CGSize)_web_sizeWithFont:(GSFontRef)font forWidth:(float)width ellipsis:(WebEllipsisStyle)ellipsisStyle;

// Draw text to fit width.  Clip or apply ellipsis according to style.
- (CGSize)_web_drawAtPoint:(CGPoint)point forWidth:(float)width withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle;

// Wrap and clip to rect.
- (CGSize)_web_drawInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle alignment:(WebTextAlignment)alignment;
- (CGSize)_web_sizeInRect:(CGRect)rect withFont:(GSFontRef)font ellipsis:(WebEllipsisStyle)ellipsisStyle;

@end
