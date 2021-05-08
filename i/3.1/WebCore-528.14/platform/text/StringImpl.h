/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef StringImpl_h
#define StringImpl_h

#include <limits.h>
#include <wtf/ASCIICType.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

#if PLATFORM(CF) || (PLATFORM(QT) && PLATFORM(DARWIN))
typedef const struct __CFString * CFStringRef;
#endif

#ifdef __OBJC__
@class NSString;
#endif

namespace WebCore {

class AtomicString;
class StringBuffer;

struct CStringTranslator;
struct HashAndCharactersTranslator;
struct StringHash;
struct UCharBufferTranslator;

enum TextCaseSensitivity { TextCaseSensitive, TextCaseInsensitive };

typedef bool (*CharacterMatchFunctionPtr)(UChar);

class StringImpl : public RefCounted<StringImpl> {
    friend class AtomicString;
    friend struct CStringTranslator;
    friend struct HashAndCharactersTranslator;
    friend struct UCharBufferTranslator;
private:
    friend class ThreadGlobalData;
    StringImpl();
    StringImpl(const UChar*, unsigned length);
    StringImpl(const char*, unsigned length);

    struct AdoptBuffer { };
    StringImpl(UChar*, unsigned length, AdoptBuffer);

    struct WithTerminatingNullCharacter { };
    StringImpl(const StringImpl&, WithTerminatingNullCharacter);

    // For AtomicString.
    StringImpl(const UChar*, unsigned length, unsigned hash);
    StringImpl(const char*, unsigned length, unsigned hash);

public:
    ~StringImpl();

    static PassRefPtr<StringImpl> create(const UChar*, unsigned length);
    static PassRefPtr<StringImpl> create(const char*, unsigned length);
    static PassRefPtr<StringImpl> create(const char*);

    static PassRefPtr<StringImpl> createWithTerminatingNullCharacter(const StringImpl&);

    static PassRefPtr<StringImpl> createStrippingNullCharacters(const UChar*, unsigned length);
    static PassRefPtr<StringImpl> adopt(StringBuffer&);
    static PassRefPtr<StringImpl> adopt(Vector<UChar>&);

    const UChar* characters() { return m_data; }
    unsigned length() { return m_length; }

    bool hasTerminatingNullCharacter() { return m_hasTerminatingNullCharacter; }

    unsigned hash() { if (m_hash == 0) m_hash = computeHash(m_data, m_length); return m_hash; }
    unsigned existingHash() const { ASSERT(m_hash); return m_hash; }
    static unsigned computeHash(const UChar*, unsigned len);
    static unsigned computeHash(const char*);
    
    // Makes a deep copy. Helpful only if you need to use a String on another thread.
    // Since StringImpl objects are immutable, there's no other reason to make a copy.
    PassRefPtr<StringImpl> copy();

    // Makes a deep copy like copy() but only for a substring.
    // (This ensures that you always get something suitable for a thread while subtring
    // may not.  For example, in the empty string case, substring returns empty() which
    // is not safe for another thread.)
    PassRefPtr<StringImpl> substringCopy(unsigned pos, unsigned len  = UINT_MAX);

    PassRefPtr<StringImpl> substring(unsigned pos, unsigned len = UINT_MAX);

    UChar operator[](unsigned i) { ASSERT(i < m_length); return m_data[i]; }
    UChar32 characterStartingAt(unsigned);

    bool containsOnlyWhitespace();

    int toIntStrict(bool* ok = 0, int base = 10);
    unsigned toUIntStrict(bool* ok = 0, int base = 10);
    int64_t toInt64Strict(bool* ok = 0, int base = 10);
    uint64_t toUInt64Strict(bool* ok = 0, int base = 10);

    int toInt(bool* ok = 0); // ignores trailing garbage
    unsigned toUInt(bool* ok = 0); // ignores trailing garbage
    int64_t toInt64(bool* ok = 0); // ignores trailing garbage
    uint64_t toUInt64(bool* ok = 0); // ignores trailing garbage

    double toDouble(bool* ok = 0);
    float toFloat(bool* ok = 0);

    bool isLower();
    PassRefPtr<StringImpl> lower();
    PassRefPtr<StringImpl> upper();
    PassRefPtr<StringImpl> secure(UChar aChar, bool last = true);
    PassRefPtr<StringImpl> capitalize(UChar previousCharacter);
    PassRefPtr<StringImpl> foldCase();

    PassRefPtr<StringImpl> stripWhiteSpace();
    PassRefPtr<StringImpl> simplifyWhiteSpace();

    PassRefPtr<StringImpl> removeCharacters(CharacterMatchFunctionPtr);

    int find(const char*, int index = 0, bool caseSensitive = true);
    int find(UChar, int index = 0);
    int find(CharacterMatchFunctionPtr, int index = 0);
    int find(StringImpl*, int index, bool caseSensitive = true);

    int reverseFind(UChar, int index);
    int reverseFind(StringImpl*, int index, bool caseSensitive = true);
    
    bool startsWith(StringImpl* m_data, bool caseSensitive = true) { return reverseFind(m_data, 0, caseSensitive) == 0; }
    bool endsWith(StringImpl*, bool caseSensitive = true);

    PassRefPtr<StringImpl> replace(UChar, UChar);
    PassRefPtr<StringImpl> replace(UChar, StringImpl*);
    PassRefPtr<StringImpl> replace(StringImpl*, StringImpl*);
    PassRefPtr<StringImpl> replace(unsigned index, unsigned len, StringImpl*);

    static StringImpl* empty();

    Vector<char> ascii();
    int wordCount(int maxWordsToCount = INT_MAX);

    WTF::Unicode::Direction defaultWritingDirection();

#if PLATFORM(CF) || (PLATFORM(QT) && PLATFORM(DARWIN))
    CFStringRef createCFString();
#endif
#ifdef __OBJC__
    operator NSString*();
#endif

private:
    unsigned m_length;
    const UChar* m_data;
    mutable unsigned m_hash;
    bool m_inTable;
    bool m_hasTerminatingNullCharacter;
};

bool equal(StringImpl*, StringImpl*);
bool equal(StringImpl*, const char*);
inline bool equal(const char* a, StringImpl* b) { return equal(b, a); }

bool equalIgnoringCase(StringImpl*, StringImpl*);
bool equalIgnoringCase(StringImpl*, const char*);
inline bool equalIgnoringCase(const char* a, StringImpl* b) { return equalIgnoringCase(b, a); }

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned phi = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
inline unsigned StringImpl::computeHash(const UChar* data, unsigned length)
{
    unsigned hash = phi;
    
    // Main loop.
    for (unsigned pairCount = length >> 1; pairCount; pairCount--) {
        hash += data[0];
        unsigned tmp = (data[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        data += 2;
        hash += hash >> 11;
    }
    
    // Handle end case.
    if (length & 1) {
        hash += data[0];
        hash ^= hash << 11;
        hash += hash >> 17;
    }

    // Force "avalanching" of final 127 bits.
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    // This avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet", using a value that is likely to be
    // effectively the same as 0 when the low bits are masked.
    hash |= !hash << 31;
    
    return hash;
}

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
inline unsigned StringImpl::computeHash(const char* data)
{
    // This hash is designed to work on 16-bit chunks at a time. But since the normal case
    // (above) is to hash UTF-16 characters, we just treat the 8-bit chars as if they
    // were 16-bit chunks, which should give matching results

    unsigned hash = phi;
    
    // Main loop
    for (;;) {
        unsigned char b0 = data[0];
        if (!b0)
            break;
        unsigned char b1 = data[1];
        if (!b1) {
            hash += b0;
            hash ^= hash << 11;
            hash += hash >> 17;
            break;
        }
        hash += b0;
        unsigned tmp = (b1 << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        data += 2;
        hash += hash >> 11;
    }
    
    // Force "avalanching" of final 127 bits.
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    // This avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet", using a value that is likely to be
    // effectively the same as 0 when the low bits are masked.
    hash |= !hash << 31;
    
    return hash;
}

static inline bool isSpaceOrNewline(UChar c)
{
    // Use isASCIISpace() for basic Latin-1.
    // This will include newlines, which aren't included in Unicode DirWS.
    return c <= 0x7F ? WTF::isASCIISpace(c) : WTF::Unicode::direction(c) == WTF::Unicode::WhiteSpaceNeutral;
}

}

namespace WTF {

    // WebCore::StringHash is the default hash for StringImpl* and RefPtr<StringImpl>
    template<typename T> struct DefaultHash;
    template<> struct DefaultHash<WebCore::StringImpl*> {
        typedef WebCore::StringHash Hash;
    };
    template<> struct DefaultHash<RefPtr<WebCore::StringImpl> > {
        typedef WebCore::StringHash Hash;
    };

}

#endif
