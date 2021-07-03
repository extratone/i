/*
 * Copyright (C) 2008, 2010, 2013 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CSSPreloadScanner.h"

#include "CachedResourceRequestInitiators.h"
#include "HTMLIdentifier.h"
#include "HTMLParserIdioms.h"

namespace WebCore {

CSSPreloadScanner::CSSPreloadScanner()
    : m_state(Initial)
    , m_requests(0)
{
}

CSSPreloadScanner::~CSSPreloadScanner()
{
}

void CSSPreloadScanner::reset()
{
    m_state = Initial;
    m_rule.clear();
    m_ruleValue.clear();
}

template<typename Char>
void CSSPreloadScanner::scanCommon(const Char* begin, const Char* end, PreloadRequestStream& requests)
{
    m_requests = &requests;
    for (const Char* it = begin; it != end && m_state != DoneParsingImportRules; ++it)
        tokenize(*it);
    m_requests = 0;
}

void CSSPreloadScanner::scan(const HTMLToken::DataVector& data, PreloadRequestStream& requests)
{
    scanCommon(data.data(), data.data() + data.size(), requests);
}

#if ENABLE(THREADED_HTML_PARSER)
void CSSPreloadScanner::scan(const HTMLIdentifier& identifier, PreloadRequestStream& requests)
{
    const StringImpl* data = identifier.asStringImpl();
    if (data->is8Bit()) {
        const LChar* begin = data->characters8();
        scanCommon(begin, begin + data->length(), requests);
        return;
    }
    const UChar* begin = data->characters16();
    scanCommon(begin, begin + data->length(), requests);
}
#endif

inline void CSSPreloadScanner::tokenize(UChar c)
{
    // We are just interested in @import rules, no need for real tokenization here
    // Searching for other types of resources is probably low payoff.
    switch (m_state) {
    case Initial:
        if (isHTMLSpace(c))
            break;
        if (c == '@')
            m_state = RuleStart;
        else if (c == '/')
            m_state = MaybeComment;
        else
            m_state = DoneParsingImportRules;
        break;
    case MaybeComment:
        if (c == '*')
            m_state = Comment;
        else
            m_state = Initial;
        break;
    case Comment:
        if (c == '*')
            m_state = MaybeCommentEnd;
        break;
    case MaybeCommentEnd:
        if (c == '*')
            break;
        if (c == '/')
            m_state = Initial;
        else
            m_state = Comment;
        break;
    case RuleStart:
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            m_rule.clear();
            m_ruleValue.clear();
            m_rule.append(c);
            m_state = Rule;
        } else
            m_state = Initial;
        break;
    case Rule:
        if (isHTMLSpace(c))
            m_state = AfterRule;
        else if (c == ';')
            m_state = Initial;
        else
            m_rule.append(c);
        break;
    case AfterRule:
        if (isHTMLSpace(c))
            break;
        if (c == ';')
            m_state = Initial;
        else if (c == '{')
            m_state = DoneParsingImportRules;
        else {
            m_state = RuleValue;
            m_ruleValue.append(c);
        }
        break;
    case RuleValue:
        if (isHTMLSpace(c))
            m_state = AfterRuleValue;
        else if (c == ';')
            emitRule();
        else
            m_ruleValue.append(c);
        break;
    case AfterRuleValue:
        if (isHTMLSpace(c))
            break;
        if (c == ';')
            emitRule();
        else if (c == '{')
            m_state = DoneParsingImportRules;
        else {
            // FIXME: media rules
            m_state = Initial;
        }
        break;
    case DoneParsingImportRules:
        ASSERT_NOT_REACHED();
        break;
    }
}

static String parseCSSStringOrURL(const UChar* characters, size_t length)
{
    size_t offset = 0;
    size_t reducedLength = length;

    while (reducedLength && isHTMLSpace(characters[offset])) {
        ++offset;
        --reducedLength;
    }
    while (reducedLength && isHTMLSpace(characters[offset + reducedLength - 1]))
        --reducedLength;

    if (reducedLength >= 5
            && (characters[offset] == 'u' || characters[offset] == 'U')
            && (characters[offset + 1] == 'r' || characters[offset + 1] == 'R')
            && (characters[offset + 2] == 'l' || characters[offset + 2] == 'L')
            && characters[offset + 3] == '('
            && characters[offset + reducedLength - 1] == ')') {
        offset += 4;
        reducedLength -= 5;
    }

    while (reducedLength && isHTMLSpace(characters[offset])) {
        ++offset;
        --reducedLength;
    }
    while (reducedLength && isHTMLSpace(characters[offset + reducedLength - 1]))
        --reducedLength;

    if (reducedLength < 2 || characters[offset] != characters[offset + reducedLength - 1] || !(characters[offset] == '\'' || characters[offset] == '"'))
        return String();
    offset++;
    reducedLength -= 2;

    while (reducedLength && isHTMLSpace(characters[offset])) {
        ++offset;
        --reducedLength;
    }
    while (reducedLength && isHTMLSpace(characters[offset + reducedLength - 1]))
        --reducedLength;

    return String(characters + offset, reducedLength);
}

template<unsigned referenceLength>
static inline bool ruleEqualIgnoringCase(const Vector<UChar>& rule, const char (&reference)[referenceLength])
{
    unsigned referenceCharactersLength = referenceLength - 1;
    return rule.size() == referenceCharactersLength && equalIgnoringCase(reference, rule.data(), referenceCharactersLength);
}

void CSSPreloadScanner::emitRule()
{
    if (ruleEqualIgnoringCase(m_rule, "import")) {
        String url = parseCSSStringOrURL(m_ruleValue.data(), m_ruleValue.size());
        if (!url.isEmpty()) {
            KURL baseElementURL; // FIXME: This should be passed in from the HTMLPreloadScaner via scan()!
            OwnPtr<PreloadRequest> request = PreloadRequest::create("css", url, baseElementURL, CachedResource::CSSStyleSheet);
            // FIXME: Should this be including the charset in the preload request?
            m_requests->append(request.release());
        }
        m_state = Initial;
    } else if (ruleEqualIgnoringCase(m_rule, "charset"))
        m_state = Initial;
    else
        m_state = DoneParsingImportRules;
    m_rule.clear();
    m_ruleValue.clear();
}

}
