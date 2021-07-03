/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef EditorState_h
#define EditorState_h

#include "ArgumentCoders.h"
#include <WebCore/IntRect.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#include <WebCore/SelectionRect.h>
#endif

namespace WebKit {

enum TypingAttributes {
    AttributeNone = 0,
    AttributeBold = 1,
    AttributeItalics = 2,
    AttributeUnderline = 4
};

struct EditorState {
    bool shouldIgnoreCompositionSelectionChange { false };

    bool selectionIsNone { true }; // This will be false when there is a caret selection.
    bool selectionIsRange { false };
    bool isContentEditable { false };
    bool isContentRichlyEditable { false };
    bool isInPasswordField { false };
    bool isInPlugin { false };
    bool hasComposition { false };
    bool isMissingPostLayoutData { false };

#if PLATFORM(IOS)
    WebCore::IntRect firstMarkedRect;
    WebCore::IntRect lastMarkedRect;
    String markedText;

    struct PostLayoutData {
        WebCore::IntRect selectionClipRect;
        Vector<WebCore::SelectionRect> selectionRects;
        WebCore::IntRect caretRectAtStart;
        WebCore::IntRect caretRectAtEnd;
        String wordAtSelection;
        uint64_t selectedTextLength { 0 };
        UChar32 characterAfterSelection { 0 };
        UChar32 characterBeforeSelection { 0 };
        UChar32 twoCharacterBeforeSelection { 0 };
        uint32_t typingAttributes { AttributeNone };
        bool isReplaceAllowed { false };
        bool hasContent { false };

        void encode(IPC::ArgumentEncoder&) const;
        static bool decode(IPC::ArgumentDecoder&, PostLayoutData&);
    };

    const PostLayoutData& postLayoutData() const;
    PostLayoutData& postLayoutData();
#endif

#if PLATFORM(GTK)
    WebCore::IntRect cursorRect;
#endif

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, EditorState&);

#if PLATFORM(IOS)
private:
    PostLayoutData m_postLayoutData;
#endif
};

#if PLATFORM(IOS)
inline auto EditorState::postLayoutData() -> PostLayoutData&
{
    ASSERT_WITH_MESSAGE(!isMissingPostLayoutData, "Attempt to access post layout data before receiving it");
    return m_postLayoutData;
}

inline auto EditorState::postLayoutData() const -> const PostLayoutData&
{
    ASSERT_WITH_MESSAGE(!isMissingPostLayoutData, "Attempt to access post layout data before receiving it");
    return m_postLayoutData;
}
#endif

}

#endif // EditorState_h
