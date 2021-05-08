/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 
#include "config.h"
#include "StringBuilder.h"

#include "StringBuffer.h"

namespace WebCore {

void StringBuilder::append(const String& string)
{
    if (string.isNull())
        return;

    if (m_totalLength == UINT_MAX)
        m_totalLength = string.length();
    else
        m_totalLength += string.length();

    if (!string.isEmpty())
        m_strings.append(string);
}

void StringBuilder::append(UChar c)
{
    if (m_totalLength == UINT_MAX)
        m_totalLength = 1;
    else
        m_totalLength += 1;

    m_strings.append(String(&c, 1));
}

void StringBuilder::append(char c)
{
    if (m_totalLength == UINT_MAX)
        m_totalLength = 1;
    else
        m_totalLength += 1;

    m_strings.append(String(&c, 1));
}

String StringBuilder::toString() const
{
    if (isNull())
        return String();

    unsigned count = m_strings.size();

    if (!count)
        return String(StringImpl::empty());
    if (count == 1)
        return m_strings[0];

    StringBuffer buffer(m_totalLength);

    UChar* p = buffer.characters();
    for (unsigned i = 0; i < count; ++i) {
        StringImpl* string = m_strings[i].impl();
        unsigned length = string->length(); 
        memcpy(p, string->characters(), length * 2);
        p += length;
    }

    ASSERT(p == m_totalLength + buffer.characters());

    return String::adopt(buffer);
}

}
