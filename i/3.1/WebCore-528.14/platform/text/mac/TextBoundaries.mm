/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "TextBoundaries.h"

#import <CoreFoundation/CFStringTokenizer.h>
#import <Foundation/Foundation.h>
#import <unicode/ubrk.h>
#import <unicode/uchar.h>
#import <unicode/ustring.h>
#import <unicode/utypes.h>

using namespace WTF::Unicode;

namespace WebCore {


static bool isSkipCharacter(UChar c)
{
    return c == 0xA0 || 
        c == '\n' || 
        c == '.' || 
        c == ',' || 
        c == '!'  || 
        c == '?' || 
        c == ';' || 
        c == ':' || 
        u_isspace(c);
}

static bool isWhitespaceCharacter(UChar c)
{
    return c == 0xA0 || 
        c == '\n' || 
        u_isspace(c);
}

static bool isWordDelimitingCharacter(UChar c)
{
    CFCharacterSetRef set = CFCharacterSetGetPredefined(kCFCharacterSetAlphaNumeric);
    return !CFCharacterSetIsCharacterMember(set, c) && c != '\'' && c != '&';
}

static CFStringTokenizerRef tokenizerForString(CFStringRef str)
{
    static CFStringTokenizerRef tokenizer = NULL;
    static CFLocaleRef          locale = NULL;
    
    if (locale == NULL) {
        const char *currentLocaleID = currentTextBreakLocaleID();
        CFStringRef lang = CFStringCreateWithBytesNoCopy(NULL, reinterpret_cast<const UInt8 *>(currentLocaleID), strlen(currentLocaleID),  kCFStringEncodingASCII, false, kCFAllocatorNull);
        locale = CFLocaleCreate(NULL, lang);
        if (!locale)
            return NULL;
        CFRelease(lang);
    }

    CFRange entireRange = CFRangeMake(0, CFStringGetLength(str));    

    if (!tokenizer) {
        tokenizer = CFStringTokenizerCreate(NULL, str, entireRange, kCFStringTokenizerUnitWordBoundary, locale);
    } else {
        CFStringTokenizerSetString(tokenizer, str, entireRange);
    }
    return tokenizer;
}


// Simple case: a word is a stream of characters
// delimited by a special set of word-delimiting characters.
static void findSimpleWordBoundary(const UChar* chars, int len, int position, int* start, int* end)
{
    int startPos = position;
    while (startPos > 0 && !isWordDelimitingCharacter(chars[startPos-1]))
        startPos--;
    
    int endPos = position;
    while (endPos < len && !isWordDelimitingCharacter(chars[endPos]))
        endPos++;
    
    *start = startPos;
    *end = endPos;
}

// Complex case: use CFStringTokenizer to find word boundary.
static void findComplexWordBoundary(const UChar* chars, int len, int position, int* start, int* end)
{
    CFStringRef charString = CFStringCreateWithCharactersNoCopy(NULL, chars, len, kCFAllocatorNull);
    ASSERT(charString);
    
    CFStringTokenizerRef tokenizer = tokenizerForString(charString);
    if (tokenizer) {        
        CFStringTokenizerTokenType  token = CFStringTokenizerGoToTokenAtIndex(tokenizer, position);
        CFRange result;
        if (token != kCFStringTokenizerTokenNone) {
            result = CFStringTokenizerGetCurrentTokenRange(tokenizer);
        } else {
            // if no token found: select entire block
            // NB: I never hit this section in all my testing...
            result.location = 0;
            result.length = len;
        }
        
        *start = result.location;
        *end = result.location + result.length;
    } else {    // error creating tokenizer
        findSimpleWordBoundary(chars, len, position, start, end);
    }

    CFRelease(charString);
}

void findWordBoundary(const UChar* chars, int len, int position, int* start, int* end)
{
    int pos = position;
    if ( position == len && position != 0)
        pos--;
    
    // For complex text (Thai, Japanese, Chinese), visible_units will pass the text in as a 
    // single contiguous run of characters, providing as much context as is possible.
    // We only need one character to determine if the text is complex.
    UChar32 ch;
    int i = 0;
    U16_NEXT(chars,i,len,ch);
    bool isComplex = requiresContextForWordBoundary(ch);
    if (isComplex) {
        findComplexWordBoundary(chars,len, position, start, end);
    } else {
        findSimpleWordBoundary(chars,len, position, start, end);
    }

#define LOG_WORD_BREAK  0
#if LOG_WORD_BREAK
    CFStringRef uniString = CFStringCreateWithCharacters(NULL, chars, len);
    CFStringRef foundWord = CFStringCreateWithCharacters(NULL, (const UniChar *) chars + *start, *end - *start);        
    NSLog(@"%s_BREAK '%@' (%d,%d) in '%@' (#%xd) at %d, length=%d", isComplex ? "COMPLEX" : "SIMPLE", foundWord, *start, *end, uniString, CFHashCode(uniString), position, len);
    CFRelease(foundWord);
    CFRelease(uniString);
#endif
    
}

int findNextWordFromIndex(const UChar* chars, int len, int position, bool forward)
{   
    // This very likely won't behave exactly like the non-iPhone version, but it works
    // for the contexts in which it is used on iPhone, and in the future will be
    // tuned to improve the iPhone-specific behavior for the keyboard and text editing.
    int pos = position;
    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator *boundary = ubrk_open(UBRK_WORD, currentTextBreakLocaleID(), const_cast<unichar *>(reinterpret_cast<const unichar *>(chars)), len, &status);

    if (boundary && U_SUCCESS(status)) {
        if (forward) {
            do {
                pos = ubrk_following(boundary, pos);    
                if (pos == UBRK_DONE) {
                    pos = len;
                }
            } while (pos <= len && (pos == 0 || !isSkipCharacter(chars[pos-1])) && isSkipCharacter(chars[pos]));
        }
        else {
            do {
                pos = ubrk_preceding(boundary, pos);
                if (pos == UBRK_DONE) {
                    pos = 0;
                }
            } while (pos > 0 && isSkipCharacter(chars[pos]) && !isWhitespaceCharacter(chars[pos-1]));
        }
        ubrk_close(boundary);
    }
    return pos;
}

}
