/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011-2013 Samsung Electronics
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextChecker.h"

#include "TextBreakIterator.h"
#include "TextCheckerState.h"
#include "WebProcessPool.h"
#include "WebTextChecker.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/TextCheckerEnchant.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

static WebCore::TextCheckerEnchant& enchantTextChecker()
{
    static NeverDestroyed<WebCore::TextCheckerEnchant> checker;
    return checker;
}

TextCheckerState& checkerState()
{
    static TextCheckerState textCheckerState;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        textCheckerState.isContinuousSpellCheckingEnabled = false;
        textCheckerState.isGrammarCheckingEnabled = false;
    });

    return textCheckerState;
}

const TextCheckerState& TextChecker::state()
{
    return checkerState();
}

static void updateStateForAllProcessPools()
{
    for (const auto& processPool : WebProcessPool::allProcessPools())
        processPool->textCheckerStateChanged();
}

bool TextChecker::isContinuousSpellCheckingAllowed()
{
    return true;
}

void TextChecker::setContinuousSpellCheckingEnabled(bool isContinuousSpellCheckingEnabled)
{
    if (checkerState().isContinuousSpellCheckingEnabled == isContinuousSpellCheckingEnabled)
        return;
    checkerState().isContinuousSpellCheckingEnabled = isContinuousSpellCheckingEnabled;
    updateStateForAllProcessPools();
}

void TextChecker::setGrammarCheckingEnabled(bool isGrammarCheckingEnabled)
{
    if (checkerState().isGrammarCheckingEnabled == isGrammarCheckingEnabled)
        return;
    checkerState().isGrammarCheckingEnabled = isGrammarCheckingEnabled;
    updateStateForAllProcessPools();
}

void TextChecker::continuousSpellCheckingEnabledStateChanged(bool enabled)
{
    checkerState().isContinuousSpellCheckingEnabled = enabled;
}

void TextChecker::grammarCheckingEnabledStateChanged(bool enabled)
{
    checkerState().isGrammarCheckingEnabled = enabled;
}

int64_t TextChecker::uniqueSpellDocumentTag(WebPageProxy*)
{
    return 0;
}

void TextChecker::closeSpellDocumentWithTag(int64_t /* tag */)
{
}

void TextChecker::checkSpellingOfString(int64_t /* spellDocumentTag */, StringView text, int32_t& misspellingLocation, int32_t& misspellingLength)
{
    misspellingLocation = -1;
    misspellingLength = 0;
    enchantTextChecker().checkSpellingOfString(text.toStringWithoutCopying(), misspellingLocation, misspellingLength);
}

void TextChecker::checkGrammarOfString(int64_t /* spellDocumentTag */, StringView /* text */, Vector<WebCore::GrammarDetail>& /* grammarDetails */, int32_t& /* badGrammarLocation */, int32_t& /* badGrammarLength */)
{
}

bool TextChecker::spellingUIIsShowing()
{
    return false;
}

void TextChecker::toggleSpellingUIIsShowing()
{
}

void TextChecker::updateSpellingUIWithMisspelledWord(int64_t /* spellDocumentTag */, const String& /* misspelledWord */)
{
}

void TextChecker::updateSpellingUIWithGrammarString(int64_t /* spellDocumentTag */, const String& /* badGrammarPhrase */, const GrammarDetail& /* grammarDetail */)
{
}

void TextChecker::getGuessesForWord(int64_t /* spellDocumentTag */, const String& word, const String& /* context */, Vector<String>& guesses)
{
    guesses = enchantTextChecker().getGuessesForWord(word);
}

void TextChecker::learnWord(int64_t /* spellDocumentTag */, const String& word)
{
    enchantTextChecker().learnWord(word);
}

void TextChecker::ignoreWord(int64_t /* spellDocumentTag */, const String& word)
{
    enchantTextChecker().ignoreWord(word);
}

void TextChecker::requestCheckingOfString(PassRefPtr<TextCheckerCompletion> completion)
{
    if (!completion)
        return;

    TextCheckingRequestData request = completion->textCheckingRequestData();
    ASSERT(request.sequence() != unrequestedTextCheckingSequence);
    ASSERT(request.mask() != TextCheckingTypeNone);

    completion->didFinishCheckingText(checkTextOfParagraph(completion->spellDocumentTag(), request.text(), request.mask()));
}

#if USE(UNIFIED_TEXT_CHECKING)
static unsigned nextWordOffset(StringView text, unsigned currentOffset)
{
    // FIXME: avoid creating textIterator object here, it could be passed as a parameter.
    //        isTextBreak() leaves the iterator pointing to the first boundary position at
    //        or after "offset" (ubrk_isBoundary side effect).
    //        For many word separators, the method doesn't properly determine the boundaries
    //        without resetting the iterator.
    TextBreakIterator* textIterator = wordBreakIterator(text);
    if (!textIterator)
        return currentOffset;

    unsigned wordOffset = currentOffset;
    while (wordOffset < text.length() && isTextBreak(textIterator, wordOffset))
        ++wordOffset;

    // Do not treat the word's boundary as a separator.
    if (!currentOffset && wordOffset == 1)
        return currentOffset;

    // Omit multiple separators.
    if ((wordOffset - currentOffset) > 1)
        --wordOffset;

    return wordOffset;
}

Vector<TextCheckingResult> TextChecker::checkTextOfParagraph(int64_t spellDocumentTag, StringView text, uint64_t checkingTypes)
{
    if (!(checkingTypes & TextCheckingTypeSpelling))
        return Vector<TextCheckingResult>();

    TextBreakIterator* textIterator = wordBreakIterator(text);
    if (!textIterator)
        return Vector<TextCheckingResult>();

    // Omit the word separators at the beginning/end of the text to don't unnecessarily
    // involve the client to check spelling for them.
    unsigned offset = nextWordOffset(text, 0);
    unsigned lengthStrip = text.length();
    while (lengthStrip > 0 && isTextBreak(textIterator, lengthStrip - 1))
        --lengthStrip;

    Vector<TextCheckingResult> paragraphCheckingResult;
    while (offset < lengthStrip) {
        int32_t misspellingLocation = -1;
        int32_t misspellingLength = 0;
        checkSpellingOfString(spellDocumentTag, text.substring(offset, lengthStrip - offset), misspellingLocation, misspellingLength);
        if (!misspellingLength)
            break;

        TextCheckingResult misspellingResult;
        misspellingResult.type = TextCheckingTypeSpelling;
        misspellingResult.location = offset + misspellingLocation;
        misspellingResult.length = misspellingLength;
        paragraphCheckingResult.append(misspellingResult);
        offset += misspellingLocation + misspellingLength;
        // Generally, we end up checking at the word separator, move to the adjacent word.
        offset = nextWordOffset(text.substring(0, lengthStrip), offset);
    }
    return paragraphCheckingResult;
}
#endif

void TextChecker::setSpellCheckingLanguages(const Vector<String>& languages)
{
    enchantTextChecker().updateSpellCheckingLanguages(languages);
}

Vector<String> TextChecker::loadedSpellCheckingLanguages()
{
    return enchantTextChecker().loadedSpellCheckingLanguages();
}

} // namespace WebKit
