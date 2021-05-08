/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved
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

#ifndef StringHash_h
#define StringHash_h

#include "AtomicString.h"
#include "PlatformString.h"
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

    // FIXME: We should really figure out a way to put the computeHash function that's
    // currently a member function of StringImpl into this file so we can be a little
    // closer to having all the nearly-identical hash functions in one place.

    struct StringHash {
        static unsigned hash(StringImpl* key) { return key->hash(); }
        static bool equal(StringImpl* a, StringImpl* b)
        {
            if (a == b)
                return true;
            if (!a || !b)
                return false;

            unsigned aLength = a->length();
            unsigned bLength = b->length();
            if (aLength != bLength)
                return false;

            const uint32_t* aChars = reinterpret_cast<const uint32_t*>(a->characters());
            const uint32_t* bChars = reinterpret_cast<const uint32_t*>(b->characters());

            unsigned halfLength = aLength >> 1;
            for (unsigned i = 0; i != halfLength; ++i)
                if (*aChars++ != *bChars++)
                    return false;

            if (aLength & 1 && *reinterpret_cast<const uint16_t*>(aChars) != *reinterpret_cast<const uint16_t*>(bChars))
                return false;

            return true;
        }

        static unsigned hash(const RefPtr<StringImpl>& key) { return key->hash(); }
        static bool equal(const RefPtr<StringImpl>& a, const RefPtr<StringImpl>& b)
        {
            return equal(a.get(), b.get());
        }

        static unsigned hash(const String& key) { return key.impl()->hash(); }
        static bool equal(const String& a, const String& b)
        {
            return equal(a.impl(), b.impl());
        }

        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    class CaseFoldingHash {
    public:
        // Paul Hsieh's SuperFastHash
        // http://www.azillionmonkeys.com/qed/hash.html
        static unsigned hash(const UChar* data, unsigned length)
        {
            unsigned l = length;
            const UChar* s = data;
            uint32_t hash = WTF::stringHashingStartValue;
            uint32_t tmp;
            
            int rem = l & 1;
            l >>= 1;
            
            // Main loop.
            for (; l > 0; l--) {
                hash += WTF::Unicode::foldCase(s[0]);
                tmp = (WTF::Unicode::foldCase(s[1]) << 11) ^ hash;
                hash = (hash << 16) ^ tmp;
                s += 2;
                hash += hash >> 11;
            }
            
            // Handle end case.
            if (rem) {
                hash += WTF::Unicode::foldCase(s[0]);
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

        static unsigned hash(StringImpl* str)
        {
            return hash(str->characters(), str->length());
        }
        
        static unsigned hash(const char* str, unsigned length)
        {
            // This hash is designed to work on 16-bit chunks at a time. But since the normal case
            // (above) is to hash UTF-16 characters, we just treat the 8-bit chars as if they
            // were 16-bit chunks, which will give matching results.

            unsigned l = length;
            const char* s = str;
            uint32_t hash = WTF::stringHashingStartValue;
            uint32_t tmp;
            
            int rem = l & 1;
            l >>= 1;
            
            // Main loop
            for (; l > 0; l--) {
                hash += WTF::Unicode::foldCase(s[0]);
                tmp = (WTF::Unicode::foldCase(s[1]) << 11) ^ hash;
                hash = (hash << 16) ^ tmp;
                s += 2;
                hash += hash >> 11;
            }
            
            // Handle end case
            if (rem) {
                hash += WTF::Unicode::foldCase(s[0]);
                hash ^= hash << 11;
                hash += hash >> 17;
            }
            
            // Force "avalanching" of final 127 bits
            hash ^= hash << 3;
            hash += hash >> 5;
            hash ^= hash << 2;
            hash += hash >> 15;
            hash ^= hash << 10;
            
            // this avoids ever returning a hash code of 0, since that is used to
            // signal "hash not computed yet", using a value that is likely to be
            // effectively the same as 0 when the low bits are masked
            hash |= !hash << 31;
            
            return hash;
        }
        
        static bool equal(StringImpl* a, StringImpl* b)
        {
            if (a == b)
                return true;
            if (!a || !b)
                return false;
            unsigned length = a->length();
            if (length != b->length())
                return false;
            return WTF::Unicode::umemcasecmp(a->characters(), b->characters(), length) == 0;
        }

        static unsigned hash(const RefPtr<StringImpl>& key) 
        {
            return hash(key.get());
        }

        static bool equal(const RefPtr<StringImpl>& a, const RefPtr<StringImpl>& b)
        {
            return equal(a.get(), b.get());
        }

        static unsigned hash(const String& key)
        {
            return hash(key.impl());
        }
        static unsigned hash(const AtomicString& key)
        {
            return hash(key.impl());
        }
        static bool equal(const String& a, const String& b)
        {
            return equal(a.impl(), b.impl());
        }
        static bool equal(const AtomicString& a, const AtomicString& b)
        {
            return (a == b) || equal(a.impl(), b.impl());
        }

        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    // This hash can be used in cases where the key is a hash of a string, but we don't
    // want to store the string. It's not really specific to string hashing, but all our
    // current uses of it are for strings.
    struct AlreadyHashed : IntHash<unsigned> {
        static unsigned hash(unsigned key) { return key; }

        // To use a hash value as a key for a hash table, we need to eliminate the
        // "deleted" value, which is negative one. That could be done by changing
        // the string hash function to never generate negative one, but this works
        // and is still relatively efficient.
        static unsigned avoidDeletedValue(unsigned hash)
        {
            ASSERT(hash);
            unsigned newHash = hash | (!(hash + 1) << 31);
            ASSERT(newHash);
            ASSERT(newHash != 0xFFFFFFFF);
            return newHash;
        }
    };

}

namespace WTF {

    template<> struct HashTraits<WebCore::String> : GenericHashTraits<WebCore::String> {
        static const bool emptyValueIsZero = true;
        static void constructDeletedValue(WebCore::String& slot) { new (&slot) WebCore::String(HashTableDeletedValue); }
        static bool isDeletedValue(const WebCore::String& slot) { return slot.isHashTableDeletedValue(); }
    };

}

#endif
