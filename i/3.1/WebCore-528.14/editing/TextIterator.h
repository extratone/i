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

#ifndef TextIterator_h
#define TextIterator_h

#include "InlineTextBox.h"
#include "Range.h"
#include <wtf/Vector.h>

namespace WebCore {

// FIXME: Can't really answer this question correctly without knowing the white-space mode.
// FIXME: Move this somewhere else in the editing directory. It doesn't belong here.
inline bool isCollapsibleWhitespace(UChar c)
{
    switch (c) {
        case ' ':
        case '\n':
            return true;
        default:
            return false;
    }
}

String plainText(const Range*);
UChar* plainTextToMallocAllocatedBuffer(const Range*, unsigned& bufferLength, bool isDisplayString);
PassRefPtr<Range> findPlainText(const Range*, const String&, bool forward, bool caseSensitive);

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow.  The text comes back in
// chunks so as to optimize for performance of the iteration.

class TextIterator {
public:
    TextIterator();
    explicit TextIterator(const Range*, bool emitCharactersBetweenAllVisiblePositions = false, bool enterTextControls = false);
    
    bool atEnd() const { return !m_positionNode; }
    void advance();
    
    int length() const { return m_textLength; }
    const UChar* characters() const { return m_textCharacters; }
    
    PassRefPtr<Range> range() const;
    Node* node() const;
     
    static int rangeLength(const Range*, bool spacesForReplacedElements = false);
    static PassRefPtr<Range> rangeFromLocationAndLength(Element* scope, int rangeLocation, int rangeLength, bool spacesForReplacedElements = false);
    static PassRefPtr<Range> subrange(Range* entireRange, int characterOffset, int characterCount);
    
private:
    void exitNode();
    bool shouldRepresentNodeOffsetZero();
    bool shouldEmitSpaceBeforeAndAfterNode(Node*);
    void representNodeOffsetZero();
    bool handleTextNode();
    bool handleReplacedElement();
    bool handleNonTextNode();
    void handleTextBox();
    void emitCharacter(UChar, Node *textNode, Node *offsetBaseNode, int textStartOffset, int textEndOffset);
    void emitText(Node *textNode, int textStartOffset, int textEndOffset);
    
    // Current position, not necessarily of the text being returned, but position
    // as we walk through the DOM tree.
    Node *m_node;
    int m_offset;
    bool m_handledNode;
    bool m_handledChildren;
    bool m_inShadowContent;
    
    // The range.
    Node *m_startContainer;
    int m_startOffset;
    Node *m_endContainer;
    int m_endOffset;
    Node *m_pastEndNode;
    
    // The current text and its position, in the form to be returned from the iterator.
    Node *m_positionNode;
    mutable Node *m_positionOffsetBaseNode;
    mutable int m_positionStartOffset;
    mutable int m_positionEndOffset;
    const UChar* m_textCharacters;
    int m_textLength;
    
    // Used when there is still some pending text from the current node; when these
    // are false and 0, we go back to normal iterating.
    bool m_needAnotherNewline;
    InlineTextBox *m_textBox;
    
    // Used to do the whitespace collapsing logic.
    Node *m_lastTextNode;    
    bool m_lastTextNodeEndedWithCollapsedSpace;
    UChar m_lastCharacter;
    
    // Used for whitespace characters that aren't in the DOM, so we can point at them.
    UChar m_singleCharacterBuffer;
    
    // Used when text boxes are out of order (Hebrew/Arabic w/ embeded LTR text)
    Vector<InlineTextBox*> m_sortedTextBoxes;
    size_t m_sortedTextBoxesPosition;
    
    // Used when deciding whether to emit a "positioning" (e.g. newline) before any other content
    bool m_haveEmitted;
    
    // Used by selection preservation code.  There should be one character emitted between every VisiblePosition
    // in the Range used to create the TextIterator.
    // FIXME <rdar://problem/6028818>: This functionality should eventually be phased out when we rewrite 
    // moveParagraphs to not clone/destroy moved content.
    bool m_emitCharactersBetweenAllVisiblePositions;
    bool m_enterTextControls;
};

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow.  The text comes back in
// chunks so as to optimize for performance of the iteration.
class SimplifiedBackwardsTextIterator {
public:
    SimplifiedBackwardsTextIterator();
    explicit SimplifiedBackwardsTextIterator(const Range *);
    
    bool atEnd() const { return !m_positionNode; }
    void advance();
    
    int length() const { return m_textLength; }
    const UChar* characters() const { return m_textCharacters; }
    
    PassRefPtr<Range> range() const;
        
private:
    void exitNode();
    bool handleTextNode();
    bool handleReplacedElement();
    bool handleNonTextNode();
    void emitCharacter(UChar, Node *Node, int startOffset, int endOffset);
    
    // Current position, not necessarily of the text being returned, but position
    // as we walk through the DOM tree.
    Node* m_node;
    int m_offset;
    bool m_handledNode;
    bool m_handledChildren;
    
    // End of the range.
    Node* m_startNode;
    int m_startOffset;
    // Start of the range.
    Node* m_endNode;
    int m_endOffset;
    
    // The current text and its position, in the form to be returned from the iterator.
    Node* m_positionNode;
    int m_positionStartOffset;
    int m_positionEndOffset;
    const UChar* m_textCharacters;
    int m_textLength;

    // Used to do the whitespace logic.
    Node* m_lastTextNode;    
    UChar m_lastCharacter;
    
    // Used for whitespace characters that aren't in the DOM, so we can point at them.
    UChar m_singleCharacterBuffer;
    
    // The node after the last node this iterator should process.
    Node* m_pastStartNode;
};

// Builds on the text iterator, adding a character position so we can walk one
// character at a time, or faster, as needed. Useful for searching.
class CharacterIterator {
public:
    CharacterIterator();
    explicit CharacterIterator(const Range* r, bool emitCharactersBetweenAllVisiblePositions = false, bool enterTextControls = false);
    
    void advance(int numCharacters);
    
    bool atBreak() const { return m_atBreak; }
    bool atEnd() const { return m_textIterator.atEnd(); }
    
    int length() const { return m_textIterator.length() - m_runOffset; }
    const UChar* characters() const { return m_textIterator.characters() + m_runOffset; }
    String string(int numChars);
    
    int characterOffset() const { return m_offset; }
    PassRefPtr<Range> range() const;
        
private:
    int m_offset;
    int m_runOffset;
    bool m_atBreak;
    
    TextIterator m_textIterator;
};
    
class BackwardsCharacterIterator {
public:
    BackwardsCharacterIterator();
    explicit BackwardsCharacterIterator(const Range*);

    void advance(int);

    bool atEnd() const { return m_textIterator.atEnd(); }

    PassRefPtr<Range> range() const;

private:
    int m_offset;
    int m_runOffset;
    bool m_atBreak;

    SimplifiedBackwardsTextIterator m_textIterator;
};

// Very similar to the TextIterator, except that the chunks of text returned are "well behaved",
// meaning they never end split up a word.  This is useful for spellcheck or (perhaps one day) searching.
class WordAwareIterator {
public:
    WordAwareIterator();
    explicit WordAwareIterator(const Range *r);

    bool atEnd() const { return !m_didLookAhead && m_textIterator.atEnd(); }
    void advance();
    
    int length() const;
    const UChar* characters() const;
    
    // Range of the text we're currently returning
    PassRefPtr<Range> range() const { return m_range; }

private:
    // text from the previous chunk from the textIterator
    const UChar* m_previousText;
    int m_previousLength;

    // many chunks from textIterator concatenated
    Vector<UChar> m_buffer;
    
    // Did we have to look ahead in the textIterator to confirm the current chunk?
    bool m_didLookAhead;

    RefPtr<Range> m_range;

    TextIterator m_textIterator;
};

}

#endif
