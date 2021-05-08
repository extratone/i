/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebCoreStringTruncator.h"

#import <wtf/Assertions.h>
#import "Font.h"

#define STRING_BUFFER_SIZE 2048
#define ELLIPSIS_CHARACTER 0x2026
#define SPACE_CHARACTER 0x0020

using namespace WebCore;

static GSFontRef currentFont;

static Font* currentRenderer = 0;
static float currentEllipsisWidth;

typedef unsigned TruncationFunction(NSString *string, unsigned length, unsigned keepCount, unichar *buffer);

static unsigned centerTruncateToBuffer(NSString *string, unsigned length, unsigned keepCount, unichar *buffer)
{
    ASSERT(keepCount < length);
    ASSERT(keepCount < STRING_BUFFER_SIZE);
    
    unsigned omitStart = (keepCount + 1) / 2;
    unsigned omitEnd = NSMaxRange([string rangeOfComposedCharacterSequenceAtIndex:omitStart + (length - keepCount) - 1]);
    omitStart = [string rangeOfComposedCharacterSequenceAtIndex:omitStart].location;
    
    // Strip single character before ellipsis character, when that character is preceded by a space
    if (omitStart > 1 && [string characterAtIndex:omitStart - 1] != SPACE_CHARACTER &&
        omitStart > 2 && [string characterAtIndex:omitStart - 2] == SPACE_CHARACTER)
        omitStart--;
    // Strip whitespace before and after the ellipsis character
    while (omitStart > 1 && [string characterAtIndex:omitStart - 1] == SPACE_CHARACTER)
        omitStart--;
    // Strip single character after ellipsis character, when that character is followed by a space
    if ((length - omitEnd) > 1 && [string characterAtIndex:omitEnd] != SPACE_CHARACTER &&
        (length - omitEnd) > 2 && [string characterAtIndex:omitEnd + 1] == SPACE_CHARACTER)
        omitEnd++;
    while ((length - omitEnd) > 1 && [string characterAtIndex:omitEnd] == SPACE_CHARACTER)
        omitEnd++;
    
    NSRange beforeRange = NSMakeRange(0, omitStart);
    NSRange afterRange = NSMakeRange(omitEnd, length - omitEnd);
    
    unsigned truncatedLength = beforeRange.length + 1 + afterRange.length;
    ASSERT(truncatedLength <= length);

    [string getCharacters:buffer range:beforeRange];
    buffer[beforeRange.length] = ELLIPSIS_CHARACTER;
    [string getCharacters:&buffer[beforeRange.length + 1] range:afterRange];
    
    return truncatedLength;
}

static unsigned rightTruncateToBuffer(NSString *string, unsigned length, unsigned keepCount, unichar *buffer)
{
    ASSERT(keepCount < length);
    ASSERT(keepCount < STRING_BUFFER_SIZE);
    
    // Strip single character before ellipsis character, when that character is preceded by a space
    if (keepCount > 1 && [string characterAtIndex:keepCount - 1] != SPACE_CHARACTER &&
        keepCount > 2 && [string characterAtIndex:keepCount - 2] == SPACE_CHARACTER)
        keepCount--;
    // Strip whitespace before the ellipsis character
    while (keepCount > 1 && [string characterAtIndex:keepCount - 1] == SPACE_CHARACTER)
        keepCount--;
    
    NSRange keepRange = NSMakeRange(0, [string rangeOfComposedCharacterSequenceAtIndex:keepCount].location);
    
    [string getCharacters:buffer range:keepRange];
    buffer[keepRange.length] = ELLIPSIS_CHARACTER;
    
    return keepRange.length + 1;
}

static unsigned leftTruncateToBuffer(NSString *string, unsigned length, unsigned keepCount, unichar *buffer)
{
    ASSERT(keepCount < length);
    ASSERT(keepCount < STRING_BUFFER_SIZE);
    
    unsigned startIndex = length - keepCount;
    
    NSRange startComposedRange = [string rangeOfComposedCharacterSequenceAtIndex:startIndex];
    if (startComposedRange.location != startIndex) startIndex = NSMaxRange(startComposedRange);
    
    unsigned adjustedStartIndex = startIndex;
    
    // Strip single character after ellipsis character, when that character is preceded by a space
    if (adjustedStartIndex < length && [string characterAtIndex:adjustedStartIndex] != SPACE_CHARACTER &&
        adjustedStartIndex < length - 1 && [string characterAtIndex:adjustedStartIndex + 1] == SPACE_CHARACTER)
        adjustedStartIndex++;
    // Strip whitespace after the ellipsis character
    while (adjustedStartIndex < length && [string characterAtIndex:adjustedStartIndex] == SPACE_CHARACTER)
        adjustedStartIndex++;
    
    if (adjustedStartIndex != startIndex) {
        startComposedRange = [string rangeOfComposedCharacterSequenceAtIndex:adjustedStartIndex];
        if (startComposedRange.location != startIndex) startIndex = NSMaxRange(startComposedRange);
    }

    NSRange keepRange = NSMakeRange(startIndex, length - startIndex);
    
    [string getCharacters:&(buffer[1]) range:keepRange];
    buffer[0] = ELLIPSIS_CHARACTER;
    
    return keepRange.length + 1;
}

static float stringWidth(Font* renderer, const unichar *characters, unsigned length)
{
    TextRun run(characters, length);
    TextStyle style;
    style.disableRoundingHacks();
    return renderer->floatWidth(run, style);
}

static NSString *truncateString(NSString *string, float maxWidth, GSFontRef font, TruncationFunction truncateToBuffer, float *resultWidth)
{
    unsigned length = [string length];
    if (length == 0) {
        return string;
    }

    if (resultWidth)
        *resultWidth = 0;
    
    unichar stringBuffer[STRING_BUFFER_SIZE];
    unsigned keepCount;
    unsigned truncatedLength;
    float width;
    unichar ellipsis;
    unsigned keepCountForLargestKnownToFit, keepCountForSmallestKnownToNotFit;
    float widthForLargestKnownToFit, widthForSmallestKnownToNotFit;
    float ratio;
    
    ASSERT_ARG(font, font);
    ASSERT_ARG(maxWidth, maxWidth >= 0);
    
    if (currentFont != font) {
        if (currentFont)
            CFRelease (currentFont);
        currentFont = (GSFontRef)CFRetain (font);
        FontPlatformData f(font);
        delete currentRenderer;
        currentRenderer = new Font(f);
        ellipsis = ELLIPSIS_CHARACTER;
        currentEllipsisWidth = stringWidth(currentRenderer, &ellipsis, 1);
    }
    
    ASSERT(currentRenderer);

    if (length > STRING_BUFFER_SIZE) {
        keepCount = STRING_BUFFER_SIZE - 1; // need 1 character for the ellipsis
        truncatedLength = centerTruncateToBuffer(string, length, keepCount, stringBuffer);
    } else {
        keepCount = length;
        [string getCharacters:stringBuffer];
        truncatedLength = length;
    }

    width = stringWidth(currentRenderer, stringBuffer, truncatedLength);
    if (width <= maxWidth) {
        if (resultWidth)
            *resultWidth = width;
        return string;
    }

    keepCountForLargestKnownToFit = 0;
    widthForLargestKnownToFit = currentEllipsisWidth;
    
    keepCountForSmallestKnownToNotFit = keepCount;
    widthForSmallestKnownToNotFit = width;
    
    if (currentEllipsisWidth >= maxWidth) {
        keepCountForLargestKnownToFit = 1;
        keepCountForSmallestKnownToNotFit = 2;
    }
    
    while (keepCountForLargestKnownToFit + 1 < keepCountForSmallestKnownToNotFit) {
        ASSERT(widthForLargestKnownToFit <= maxWidth);
        ASSERT(widthForSmallestKnownToNotFit > maxWidth);

        ratio = (keepCountForSmallestKnownToNotFit - keepCountForLargestKnownToFit)
            / (widthForSmallestKnownToNotFit - widthForLargestKnownToFit);
        keepCount = static_cast<unsigned>(maxWidth * ratio);
        
        if (keepCount <= keepCountForLargestKnownToFit) {
            keepCount = keepCountForLargestKnownToFit + 1;
        } else if (keepCount >= keepCountForSmallestKnownToNotFit) {
            keepCount = keepCountForSmallestKnownToNotFit - 1;
        }
        
        ASSERT(keepCount < length);
        ASSERT(keepCount > 0);
        ASSERT(keepCount < keepCountForSmallestKnownToNotFit);
        ASSERT(keepCount > keepCountForLargestKnownToFit);
        
        truncatedLength = truncateToBuffer(string, length, keepCount, stringBuffer);

        width = stringWidth(currentRenderer, stringBuffer, truncatedLength);
        if (width <= maxWidth) {
            keepCountForLargestKnownToFit = keepCount;
            widthForLargestKnownToFit = width;
            if (resultWidth)
                *resultWidth = width;
        } else {
            keepCountForSmallestKnownToNotFit = keepCount;
            widthForSmallestKnownToNotFit = width;
        }
    }
    
    if (keepCountForLargestKnownToFit == 0) {
        keepCountForLargestKnownToFit = 1;
    }
    
    if (keepCount != keepCountForLargestKnownToFit) {
        keepCount = keepCountForLargestKnownToFit;
        truncatedLength = truncateToBuffer(string, length, keepCount, stringBuffer);
    }

    return [NSString stringWithCharacters:stringBuffer length:truncatedLength];
}

@implementation WebCoreStringTruncator


+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(GSFontRef)font
{
    return truncateString(string, maxWidth, font, centerTruncateToBuffer, 0);
}

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(GSFontRef)font resultWidth:(float *)resultWidth
{
    return truncateString(string, maxWidth, font, centerTruncateToBuffer, resultWidth);
}

+ (NSString *)rightTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(GSFontRef)font
{   
    return truncateString(string, maxWidth, font, rightTruncateToBuffer, 0);
}

+ (NSString *)rightTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(GSFontRef)font resultWidth:(float *)resultWidth
{   
    return truncateString(string, maxWidth, font, rightTruncateToBuffer, resultWidth);
}

+ (NSString *)leftTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(GSFontRef)font resultWidth:(float *)resultWidth
{   
    return truncateString(string, maxWidth, font, leftTruncateToBuffer, resultWidth);
}


+ (float)widthOfString:(NSString *)string font:(GSFontRef)font
{
    unsigned length = [string length];
    unichar *s = static_cast<unichar*>(malloc(sizeof(unichar) * length));
    [string getCharacters:s];
    FontPlatformData f(font);
    Font fontRenderer(f);
    float width = stringWidth(&fontRenderer, s, length);
    free(s);
    return width;
}

+ (void)clear
{
    delete currentRenderer;
    currentRenderer = 0;
	CFRelease (currentFont);
    currentFont = nil;
}

@end
