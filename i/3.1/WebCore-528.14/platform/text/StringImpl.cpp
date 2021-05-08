/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "StringImpl.h"

#include "AtomicString.h"
#include "CString.h"
#include "CharacterNames.h"
#include "FloatConversion.h"
#include "StringBuffer.h"
#include "StringHash.h"
#include "TextBreakIterator.h"
#include "TextEncoding.h"
#include "ThreadGlobalData.h"
#include <wtf/dtoa.h>
#include <wtf/Assertions.h>
#include <wtf/Threading.h>
#include <wtf/unicode/Unicode.h>

using namespace WTF;
using namespace Unicode;

namespace WebCore {

static inline UChar* newUCharVector(unsigned n)
{
    return static_cast<UChar*>(fastMalloc(sizeof(UChar) * n));
}

static inline void deleteUCharVector(const UChar* p)
{
    fastFree(const_cast<UChar*>(p));
}

// This constructor is used only to create the empty string.
StringImpl::StringImpl()
    : m_length(0)
    , m_data(0)
    , m_hash(0)
    , m_inTable(false)
    , m_hasTerminatingNullCharacter(false)
{
    // Ensure that the hash is computed so that AtomicStringHash can call existingHash()
    // with impunity. The empty string is special because it is never entered into
    // AtomicString's HashKey, but still needs to compare correctly.
    hash();
}

// This is one of the most common constructors, but it's also used for the copy()
// operation. Because of that, it's the one constructor that doesn't assert the
// length is non-zero, since we support copying the empty string.
inline StringImpl::StringImpl(const UChar* characters, unsigned length)
    : m_length(length)
    , m_hash(0)
    , m_inTable(false)
    , m_hasTerminatingNullCharacter(false)
{
    UChar* data = newUCharVector(length);
    memcpy(data, characters, length * sizeof(UChar));
    m_data = data;
}

inline StringImpl::StringImpl(const StringImpl& str, WithTerminatingNullCharacter)
    : m_length(str.m_length)
    , m_hash(str.m_hash)
    , m_inTable(false)
    , m_hasTerminatingNullCharacter(true)
{
    UChar* data = newUCharVector(str.m_length + 1);
    memcpy(data, str.m_data, str.m_length * sizeof(UChar));
    data[str.m_length] = 0;
    m_data = data;
}

inline StringImpl::StringImpl(const char* characters, unsigned length)
    : m_length(length)
    , m_hash(0)
    , m_inTable(false)
    , m_hasTerminatingNullCharacter(false)
{
    ASSERT(characters);
    ASSERT(length);

    UChar* data = newUCharVector(length);
    for (unsigned i = 0; i != length; ++i) {
        unsigned char c = characters[i];
        data[i] = c;
    }
    m_data = data;
}

inline StringImpl::StringImpl(UChar* characters, unsigned length, AdoptBuffer)
    : m_length(length)
    , m_data(characters)
    , m_hash(0)
    , m_inTable(false)
    , m_hasTerminatingNullCharacter(false)
{
    ASSERT(characters);
    ASSERT(length);
}

// This constructor is only for use by AtomicString.
StringImpl::StringImpl(const UChar* characters, unsigned length, unsigned hash)
    : m_length(length)
    , m_hash(hash)
    , m_inTable(true)
    , m_hasTerminatingNullCharacter(false)
{
    ASSERT(hash);
    ASSERT(characters);
    ASSERT(length);

    UChar* data = newUCharVector(length);
    memcpy(data, characters, length * sizeof(UChar));
    m_data = data;
}

// This constructor is only for use by AtomicString.
StringImpl::StringImpl(const char* characters, unsigned length, unsigned hash)
    : m_length(length)
    , m_hash(hash)
    , m_inTable(true)
    , m_hasTerminatingNullCharacter(false)
{
    ASSERT(hash);
    ASSERT(characters);
    ASSERT(length);

    UChar* data = newUCharVector(length);
    for (unsigned i = 0; i != length; ++i) {
        unsigned char c = characters[i];
        data[i] = c;
    }
    m_data = data;
}

StringImpl::~StringImpl()
{
    if (m_inTable)
        AtomicString::remove(this);
    deleteUCharVector(m_data);
}

StringImpl* StringImpl::empty()
{
    return threadGlobalData().emptyString();
}

bool StringImpl::containsOnlyWhitespace()
{
    // FIXME: The definition of whitespace here includes a number of characters
    // that are not whitespace from the point of view of RenderText; I wonder if
    // that's a problem in practice.
    for (unsigned i = 0; i < m_length; i++)
        if (!isASCIISpace(m_data[i]))
            return false;
    return true;
}

PassRefPtr<StringImpl> StringImpl::substring(unsigned pos, unsigned len)
{
    if (pos >= m_length)
        return empty();
    if (len > m_length - pos)
        len = m_length - pos;
    return create(m_data + pos, len);
}

PassRefPtr<StringImpl> StringImpl::substringCopy(unsigned pos, unsigned len)
{
    if (pos >= m_length)
        pos = m_length;
    if (len > m_length - pos)
        len = m_length - pos;
    if (!len)
        return adoptRef(new StringImpl);
    return substring(pos, len);
}

UChar32 StringImpl::characterStartingAt(unsigned i)
{
    if (U16_IS_SINGLE(m_data[i]))
        return m_data[i];
    if (i + 1 < m_length && U16_IS_LEAD(m_data[i]) && U16_IS_TRAIL(m_data[i + 1]))
        return U16_GET_SUPPLEMENTARY(m_data[i], m_data[i + 1]);
    return 0;
}

bool StringImpl::isLower()
{
    // Do a faster loop for the case where all the characters are ASCII.
    bool allLower = true;
    UChar ored = 0;
    for (unsigned i = 0; i < m_length; i++) {
        UChar c = m_data[i];
        allLower = allLower && isASCIILower(c);
        ored |= c;
    }
    if (!(ored & ~0x7F))
        return allLower;

    // Do a slower check for cases that include non-ASCII characters.
    allLower = true;
    unsigned i = 0;
    while (i < m_length) {
        UChar32 character;
        U16_NEXT(m_data, i, m_length, character)
        allLower = allLower && Unicode::isLower(character);
    }
    return allLower;
}

PassRefPtr<StringImpl> StringImpl::lower()
{
    StringBuffer data(m_length);
    int32_t length = m_length;

    // Do a faster loop for the case where all the characters are ASCII.
    UChar ored = 0;
    for (int i = 0; i < length; i++) {
        UChar c = m_data[i];
        ored |= c;
        data[i] = toASCIILower(c);
    }
    if (!(ored & ~0x7F))
        return adopt(data);

    // Do a slower implementation for cases that include non-ASCII characters.
    bool error;
    int32_t realLength = Unicode::toLower(data.characters(), length, m_data, m_length, &error);
    if (!error && realLength == length)
        return adopt(data);
    data.resize(realLength);
    Unicode::toLower(data.characters(), realLength, m_data, m_length, &error);
    if (error)
        return this;
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::upper()
{
    StringBuffer data(m_length);
    int32_t length = m_length;

    // Do a faster loop for the case where all the characters are ASCII.
    UChar ored = 0;
    for (int i = 0; i < length; i++) {
        UChar c = m_data[i];
        ored |= c;
        data[i] = toASCIIUpper(c);
    }
    if (!(ored & ~0x7F))
        return adopt(data);

    // Do a slower implementation for cases that include non-ASCII characters.
    bool error;
    int32_t realLength = Unicode::toUpper(data.characters(), length, m_data, m_length, &error);
    if (!error && realLength == length)
        return adopt(data);
    data.resize(realLength);
    Unicode::toUpper(data.characters(), realLength, m_data, m_length, &error);
    if (error)
        return this;
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::secure(UChar aChar, bool last)
{
    int length = m_length;
    Vector<UChar> data(length);
    if (length > 0) {
        for (int i = 0; i <  length - 1; ++i)
            data[i] = aChar;
        data[length - 1] = (last ? aChar : m_data[length - 1]);
    }
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::foldCase()
{
    StringBuffer data(m_length);
    int32_t length = m_length;

    // Do a faster loop for the case where all the characters are ASCII.
    UChar ored = 0;
    for (int i = 0; i < length; i++) {
        UChar c = m_data[i];
        ored |= c;
        data[i] = toASCIILower(c);
    }
    if (!(ored & ~0x7F))
        return adopt(data);

    // Do a slower implementation for cases that include non-ASCII characters.
    bool error;
    int32_t realLength = Unicode::foldCase(data.characters(), length, m_data, m_length, &error);
    if (!error && realLength == length)
        return adopt(data);
    data.resize(realLength);
    Unicode::foldCase(data.characters(), realLength, m_data, m_length, &error);
    if (error)
        return this;
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::stripWhiteSpace()
{
    if (!m_length)
        return empty();

    unsigned start = 0;
    unsigned end = m_length - 1;
    
    // skip white space from start
    while (start <= end && isSpaceOrNewline(m_data[start]))
        start++;
    
    // only white space
    if (start > end) 
        return empty();

    // skip white space from end
    while (end && isSpaceOrNewline(m_data[end]))
        end--;

    return create(m_data + start, end + 1 - start);
}

PassRefPtr<StringImpl> StringImpl::removeCharacters(CharacterMatchFunctionPtr findMatch)
{
    const UChar* from = m_data;
    const UChar* fromend = from + m_length;

    // Assume the common case will not remove any characters
    while (from != fromend && !findMatch(*from))
        from++;
    if (from == fromend)
        return this;

    StringBuffer data(m_length);
    UChar* to = data.characters();
    unsigned outc = from - m_data;

    if (outc)
        memcpy(to, m_data, outc * sizeof(UChar));

    while (true) {
        while (from != fromend && findMatch(*from))
            from++;
        while (from != fromend && !findMatch(*from))
            to[outc++] = *from++;
        if (from == fromend)
            break;
    }

    data.shrink(outc);

    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::simplifyWhiteSpace()
{
    StringBuffer data(m_length);

    const UChar* from = m_data;
    const UChar* fromend = from + m_length;
    int outc = 0;
    
    UChar* to = data.characters();
    
    while (true) {
        while (from != fromend && isSpaceOrNewline(*from))
            from++;
        while (from != fromend && !isSpaceOrNewline(*from))
            to[outc++] = *from++;
        if (from != fromend)
            to[outc++] = ' ';
        else
            break;
    }
    
    if (outc > 0 && to[outc - 1] == ' ')
        outc--;
    
    data.shrink(outc);
    
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::capitalize(UChar previous)
{
    StringBuffer stringWithPrevious(m_length + 1);
    stringWithPrevious[0] = previous == noBreakSpace ? ' ' : previous;
    for (unsigned i = 1; i < m_length + 1; i++) {
        // Replace &nbsp with a real space since ICU no longer treats &nbsp as a word separator.
        if (m_data[i - 1] == noBreakSpace)
            stringWithPrevious[i] = ' ';
        else
            stringWithPrevious[i] = m_data[i - 1];
    }

    TextBreakIterator* boundary = wordBreakIterator(stringWithPrevious.characters(), m_length + 1);
    if (!boundary)
        return this;

    StringBuffer data(m_length);

    int32_t endOfWord;
    int32_t startOfWord = textBreakFirst(boundary);
    for (endOfWord = textBreakNext(boundary); endOfWord != TextBreakDone; startOfWord = endOfWord, endOfWord = textBreakNext(boundary)) {
        if (startOfWord != 0) // Ignore first char of previous string
            data[startOfWord - 1] = m_data[startOfWord - 1] == noBreakSpace ? noBreakSpace : toTitleCase(stringWithPrevious[startOfWord]);
        for (int i = startOfWord + 1; i < endOfWord; i++)
            data[i - 1] = m_data[i - 1];
    }

    return adopt(data);
}

int StringImpl::toIntStrict(bool* ok, int base)
{
    return charactersToIntStrict(m_data, m_length, ok, base);
}

unsigned StringImpl::toUIntStrict(bool* ok, int base)
{
    return charactersToUIntStrict(m_data, m_length, ok, base);
}

int64_t StringImpl::toInt64Strict(bool* ok, int base)
{
    return charactersToInt64Strict(m_data, m_length, ok, base);
}

uint64_t StringImpl::toUInt64Strict(bool* ok, int base)
{
    return charactersToUInt64Strict(m_data, m_length, ok, base);
}

int StringImpl::toInt(bool* ok)
{
    return charactersToInt(m_data, m_length, ok);
}

unsigned StringImpl::toUInt(bool* ok)
{
    return charactersToUInt(m_data, m_length, ok);
}

int64_t StringImpl::toInt64(bool* ok)
{
    return charactersToInt64(m_data, m_length, ok);
}

uint64_t StringImpl::toUInt64(bool* ok)
{
    return charactersToUInt64(m_data, m_length, ok);
}

double StringImpl::toDouble(bool* ok)
{
    return charactersToDouble(m_data, m_length, ok);
}

float StringImpl::toFloat(bool* ok)
{
    return charactersToFloat(m_data, m_length, ok);
}

static bool equal(const UChar* a, const char* b, int length)
{
    ASSERT(length >= 0);
    while (length--) {
        unsigned char bc = *b++;
        if (*a++ != bc)
            return false;
    }
    return true;
}

static bool equalIgnoringCase(const UChar* a, const char* b, int length)
{
    ASSERT(length >= 0);
    while (length--) {
        unsigned char bc = *b++;
        if (foldCase(*a++) != foldCase(bc))
            return false;
    }
    return true;
}

static inline bool equalIgnoringCase(const UChar* a, const UChar* b, int length)
{
    ASSERT(length >= 0);
    return umemcasecmp(a, b, length) == 0;
}

int StringImpl::find(const char* chs, int index, bool caseSensitive)
{
    if (!chs || index < 0)
        return -1;

    int chsLength = strlen(chs);
    int n = m_length - index;
    if (n < 0)
        return -1;
    n -= chsLength - 1;
    if (n <= 0)
        return -1;

    const char* chsPlusOne = chs + 1;
    int chsLengthMinusOne = chsLength - 1;
    
    const UChar* ptr = m_data + index - 1;
    if (caseSensitive) {
        UChar c = *chs;
        do {
            if (*++ptr == c && equal(ptr + 1, chsPlusOne, chsLengthMinusOne))
                return m_length - chsLength - n + 1;
        } while (--n);
    } else {
        UChar lc = Unicode::foldCase(*chs);
        do {
            if (Unicode::foldCase(*++ptr) == lc && equalIgnoringCase(ptr + 1, chsPlusOne, chsLengthMinusOne))
                return m_length - chsLength - n + 1;
        } while (--n);
    }

    return -1;
}

int StringImpl::find(UChar c, int start)
{
    return WebCore::find(m_data, m_length, c, start);
}

int StringImpl::find(CharacterMatchFunctionPtr matchFunction, int start)
{
    return WebCore::find(m_data, m_length, matchFunction, start);
}

int StringImpl::find(StringImpl* str, int index, bool caseSensitive)
{
    /*
      We use a simple trick for efficiency's sake. Instead of
      comparing strings, we compare the sum of str with that of
      a part of this string. Only if that matches, we call memcmp
      or ucstrnicmp.
    */
    ASSERT(str);
    if (index < 0)
        index += m_length;
    int lstr = str->m_length;
    int lthis = m_length - index;
    if ((unsigned)lthis > m_length)
        return -1;
    int delta = lthis - lstr;
    if (delta < 0)
        return -1;

    const UChar* uthis = m_data + index;
    const UChar* ustr = str->m_data;
    unsigned hthis = 0;
    unsigned hstr = 0;
    if (caseSensitive) {
        for (int i = 0; i < lstr; i++) {
            hthis += uthis[i];
            hstr += ustr[i];
        }
        int i = 0;
        while (1) {
            if (hthis == hstr && memcmp(uthis + i, ustr, lstr * sizeof(UChar)) == 0)
                return index + i;
            if (i == delta)
                return -1;
            hthis += uthis[i + lstr];
            hthis -= uthis[i];
            i++;
        }
    } else {
        for (int i = 0; i < lstr; i++ ) {
            hthis += toASCIILower(uthis[i]);
            hstr += toASCIILower(ustr[i]);
        }
        int i = 0;
        while (1) {
            if (hthis == hstr && equalIgnoringCase(uthis + i, ustr, lstr))
                return index + i;
            if (i == delta)
                return -1;
            hthis += toASCIILower(uthis[i + lstr]);
            hthis -= toASCIILower(uthis[i]);
            i++;
        }
    }
}

int StringImpl::reverseFind(UChar c, int index)
{
    return WebCore::reverseFind(m_data, m_length, c, index);
}

int StringImpl::reverseFind(StringImpl* str, int index, bool caseSensitive)
{
    /*
     See StringImpl::find() for explanations.
     */
    ASSERT(str);
    int lthis = m_length;
    if (index < 0)
        index += lthis;
    
    int lstr = str->m_length;
    int delta = lthis - lstr;
    if ( index < 0 || index > lthis || delta < 0 )
        return -1;
    if ( index > delta )
        index = delta;
    
    const UChar *uthis = m_data;
    const UChar *ustr = str->m_data;
    unsigned hthis = 0;
    unsigned hstr = 0;
    int i;
    if (caseSensitive) {
        for ( i = 0; i < lstr; i++ ) {
            hthis += uthis[index + i];
            hstr += ustr[i];
        }
        i = index;
        while (1) {
            if (hthis == hstr && memcmp(uthis + i, ustr, lstr * sizeof(UChar)) == 0)
                return i;
            if (i == 0)
                return -1;
            i--;
            hthis -= uthis[i + lstr];
            hthis += uthis[i];
        }
    } else {
        for (i = 0; i < lstr; i++) {
            hthis += toASCIILower(uthis[index + i]);
            hstr += toASCIILower(ustr[i]);
        }
        i = index;
        while (1) {
            if (hthis == hstr && equalIgnoringCase(uthis + i, ustr, lstr) )
                return i;
            if (i == 0)
                return -1;
            i--;
            hthis -= toASCIILower(uthis[i + lstr]);
            hthis += toASCIILower(uthis[i]);
        }
    }
    
    // Should never get here.
    return -1;
}

bool StringImpl::endsWith(StringImpl* m_data, bool caseSensitive)
{
    ASSERT(m_data);
    int start = m_length - m_data->m_length;
    if (start >= 0)
        return (find(m_data, start, caseSensitive) == start);
    return false;
}

PassRefPtr<StringImpl> StringImpl::replace(UChar oldC, UChar newC)
{
    if (oldC == newC)
        return this;
    unsigned i;
    for (i = 0; i != m_length; ++i)
        if (m_data[i] == oldC)
            break;
    if (i == m_length)
        return this;

    StringBuffer data(m_length);
    for (i = 0; i != m_length; ++i) {
        UChar ch = m_data[i];
        if (ch == oldC)
            ch = newC;
        data[i] = ch;
    }
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::replace(unsigned position, unsigned lengthToReplace, StringImpl* str)
{
    position = min(position, length());
    lengthToReplace = min(lengthToReplace, length() - position);
    unsigned lengthToInsert = str ? str->length() : 0;
    if (!lengthToReplace && !lengthToInsert)
        return this;
    StringBuffer buffer(length() - lengthToReplace + lengthToInsert);
    memcpy(buffer.characters(), characters(), position * sizeof(UChar));
    if (str)
        memcpy(buffer.characters() + position, str->characters(), lengthToInsert * sizeof(UChar));
    memcpy(buffer.characters() + position + lengthToInsert, characters() + position + lengthToReplace,
        (length() - position - lengthToReplace) * sizeof(UChar));
    return adopt(buffer);
}

PassRefPtr<StringImpl> StringImpl::replace(UChar pattern, StringImpl* replacement)
{
    if (!replacement)
        return this;
        
    int repStrLength = replacement->length();
    int srcSegmentStart = 0;
    int matchCount = 0;
    
    // Count the matches
    while ((srcSegmentStart = find(pattern, srcSegmentStart)) >= 0) {
        ++matchCount;
        ++srcSegmentStart;
    }
    
    // If we have 0 matches, we don't have to do any more work
    if (!matchCount)
        return this;
    
    StringBuffer data(m_length - matchCount + (matchCount * repStrLength));
    
    // Construct the new data
    int srcSegmentEnd;
    int srcSegmentLength;
    srcSegmentStart = 0;
    int dstOffset = 0;
    
    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) >= 0) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        memcpy(data.characters() + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));
        dstOffset += srcSegmentLength;
        memcpy(data.characters() + dstOffset, replacement->m_data, repStrLength * sizeof(UChar));
        dstOffset += repStrLength;
        srcSegmentStart = srcSegmentEnd + 1;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    memcpy(data.characters() + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));

    ASSERT(dstOffset + srcSegmentLength == static_cast<int>(data.length()));

    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::replace(StringImpl* pattern, StringImpl* replacement)
{
    if (!pattern || !replacement)
        return this;

    int patternLength = pattern->length();
    if (!patternLength)
        return this;
        
    int repStrLength = replacement->length();
    int srcSegmentStart = 0;
    int matchCount = 0;
    
    // Count the matches
    while ((srcSegmentStart = find(pattern, srcSegmentStart)) >= 0) {
        ++matchCount;
        srcSegmentStart += patternLength;
    }
    
    // If we have 0 matches, we don't have to do any more work
    if (!matchCount)
        return this;
    
    StringBuffer data(m_length + matchCount * (repStrLength - patternLength));
    
    // Construct the new data
    int srcSegmentEnd;
    int srcSegmentLength;
    srcSegmentStart = 0;
    int dstOffset = 0;
    
    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) >= 0) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        memcpy(data.characters() + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));
        dstOffset += srcSegmentLength;
        memcpy(data.characters() + dstOffset, replacement->m_data, repStrLength * sizeof(UChar));
        dstOffset += repStrLength;
        srcSegmentStart = srcSegmentEnd + patternLength;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    memcpy(data.characters() + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));

    ASSERT(dstOffset + srcSegmentLength == static_cast<int>(data.length()));

    return adopt(data);
}

bool equal(StringImpl* a, StringImpl* b)
{
    return StringHash::equal(a, b);
}

bool equal(StringImpl* a, const char* b)
{
    if (!a)
        return !b;
    if (!b)
        return !a;

    unsigned length = a->length();
    const UChar* as = a->characters();
    for (unsigned i = 0; i != length; ++i) {
        unsigned char bc = b[i];
        if (!bc)
            return false;
        if (as[i] != bc)
            return false;
    }

    return !b[length];
}

bool equalIgnoringCase(StringImpl* a, StringImpl* b)
{
    return CaseFoldingHash::equal(a, b);
}

bool equalIgnoringCase(StringImpl* a, const char* b)
{
    if (!a)
        return !b;
    if (!b)
        return !a;

    unsigned length = a->length();
    const UChar* as = a->characters();

    // Do a faster loop for the case where all the characters are ASCII.
    UChar ored = 0;
    bool equal = true;
    for (unsigned i = 0; i != length; ++i) {
        char bc = b[i];
        if (!bc)
            return false;
        UChar ac = as[i];
        ored |= ac;
        equal = equal && (toASCIILower(ac) == toASCIILower(bc));
    }

    // Do a slower implementation for cases that include non-ASCII characters.
    if (ored & ~0x7F) {
        equal = true;
        for (unsigned i = 0; i != length; ++i) {
            unsigned char bc = b[i];
            equal = equal && (foldCase(as[i]) == foldCase(bc));
        }
    }

    return equal && !b[length];
}

Vector<char> StringImpl::ascii()
{
    Vector<char> buffer(m_length + 1);
    for (unsigned i = 0; i != m_length; ++i) {
        UChar c = m_data[i];
        if ((c >= 0x20 && c < 0x7F) || c == 0x00)
            buffer[i] = c;
        else
            buffer[i] = '?';
    }
    buffer[m_length] = '\0';
    return buffer;
}

WTF::Unicode::Direction StringImpl::defaultWritingDirection()
{
    for (unsigned i = 0; i < m_length; ++i) {
        WTF::Unicode::Direction charDirection = WTF::Unicode::direction(m_data[i]);
        if (charDirection == WTF::Unicode::LeftToRight)
            return WTF::Unicode::LeftToRight;
        if (charDirection == WTF::Unicode::RightToLeft || charDirection == WTF::Unicode::RightToLeftArabic)
            return WTF::Unicode::RightToLeft;
    }
    return WTF::Unicode::LeftToRight;
}

// This is a hot function because it's used when parsing HTML.
PassRefPtr<StringImpl> StringImpl::createStrippingNullCharacters(const UChar* characters, unsigned length)
{
    ASSERT(characters);
    ASSERT(length);

    // Optimize for the case where there are no Null characters by quickly
    // searching for nulls, and then using StringImpl::create, which will
    // memcpy the whole buffer.  This is faster than assigning character by
    // character during the loop. 

    // Fast case.
    int foundNull = 0;
    for (unsigned i = 0; !foundNull && i < length; i++) {
        int c = characters[i]; // more efficient than using UChar here (at least on Intel Mac OS)
        foundNull |= !c;
    }
    if (!foundNull)
        return StringImpl::create(characters, length);
    
    // Slow case.
    StringBuffer strippedCopy(length);
    unsigned strippedLength = 0;
    for (unsigned i = 0; i < length; i++) {
        if (int c = characters[i])
            strippedCopy[strippedLength++] = c;
    }
    ASSERT(strippedLength < length);  // Only take the slow case when stripping.
    strippedCopy.shrink(strippedLength);
    return adopt(strippedCopy);
}

PassRefPtr<StringImpl> StringImpl::adopt(StringBuffer& buffer)
{
    unsigned length = buffer.length();
    if (length == 0)
        return empty();
    return adoptRef(new StringImpl(buffer.release(), length, AdoptBuffer()));
}

PassRefPtr<StringImpl> StringImpl::adopt(Vector<UChar>& vector)
{
    size_t size = vector.size();
    if (size == 0)
        return empty();
    return adoptRef(new StringImpl(vector.releaseBuffer(), size, AdoptBuffer()));
}

int StringImpl::wordCount(int maxWordsToCount)
{
    unsigned wordCount = 0;
    unsigned i;
    bool atWord = false;
    for (i = 0; i < m_length; i++) {
        if (u_isspace(m_data[i])) {
            atWord = false;
        } else if (!atWord) {
            wordCount++;
            if (wordCount >= (unsigned)maxWordsToCount)
                return wordCount;
            atWord = true;
        }
    }
    return wordCount;
}

PassRefPtr<StringImpl> StringImpl::create(const UChar* characters, unsigned length)
{
    if (!characters || !length)
        return empty();
    return adoptRef(new StringImpl(characters, length));
}

PassRefPtr<StringImpl> StringImpl::create(const char* characters, unsigned length)
{
    if (!characters || !length)
        return empty();
    return adoptRef(new StringImpl(characters, length));
}

PassRefPtr<StringImpl> StringImpl::create(const char* string)
{
    if (!string)
        return empty();
    unsigned length = strlen(string);
    if (!length)
        return empty();
    return adoptRef(new StringImpl(string, length));
}

PassRefPtr<StringImpl> StringImpl::createWithTerminatingNullCharacter(const StringImpl& string)
{
    return adoptRef(new StringImpl(string, WithTerminatingNullCharacter()));
}

PassRefPtr<StringImpl> StringImpl::copy()
{
    return adoptRef(new StringImpl(m_data, m_length));
}

} // namespace WebCore
