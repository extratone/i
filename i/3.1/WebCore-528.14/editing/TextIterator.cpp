/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
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

#include "config.h"
#include "TextIterator.h"

#include "CharacterNames.h"
#include "Document.h"
#include "Element.h"
#include "HTMLNames.h"
#include "htmlediting.h"
#include "InlineTextBox.h"
#include "Position.h"
#include "Range.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderTextControl.h"
#include "VisiblePosition.h"
#include "visible_units.h"

#if USE(ICU_UNICODE) && !UCONFIG_NO_COLLATION
#include <unicode/usearch.h>
#endif

using namespace WTF::Unicode;
using namespace std;

namespace WebCore {

using namespace HTMLNames;

// Buffer that knows how to compare with a search target.
// Keeps enough of the previous text to be able to search in the future, but no more.
// Non-breaking spaces are always equal to normal spaces.
// Case folding is also done if <isCaseSensitive> is false.
class SearchBuffer : Noncopyable {
public:
    SearchBuffer(const String& target, bool isCaseSensitive);
    ~SearchBuffer();

    // Returns number of characters appended; guaranteed to be in the range [1, length].
    size_t append(const UChar*, size_t length);
    void reachedBreak();

    // Result is the size in characters of what was found.
    // And <startOffset> is the number of characters back to the start of what was found.
    size_t search(size_t& startOffset);
    bool atBreak() const;

#if USE(ICU_UNICODE) && !UCONFIG_NO_COLLATION

private:
    String m_target;
    Vector<UChar> m_buffer;
    size_t m_overlap;
    bool m_atBreak;

#else

private:
    void append(UChar, bool isCharacterStart);
    size_t length() const;

    String m_target;
    bool m_isCaseSensitive;

    Vector<UChar> m_buffer;
    Vector<bool> m_isCharacterStartBuffer;
    bool m_isBufferFull;
    size_t m_cursor;

#endif
};

// --------

TextIterator::TextIterator()
    : m_startContainer(0)
    , m_startOffset(0)
    , m_endContainer(0)
    , m_endOffset(0)
    , m_positionNode(0)
    , m_lastCharacter(0)
    , m_emitCharactersBetweenAllVisiblePositions(false)
    , m_enterTextControls(false)
{
}

TextIterator::TextIterator(const Range* r, bool emitCharactersBetweenAllVisiblePositions, bool enterTextControls) 
    : m_inShadowContent(false)
    , m_startContainer(0) 
    , m_startOffset(0)
    , m_endContainer(0)
    , m_endOffset(0)
    , m_positionNode(0)
    , m_emitCharactersBetweenAllVisiblePositions(emitCharactersBetweenAllVisiblePositions)
    , m_enterTextControls(enterTextControls)
{
    if (!r)
        return;

    // get and validate the range endpoints
    Node* startContainer = r->startContainer();
    if (!startContainer)
        return;
    int startOffset = r->startOffset();
    Node* endContainer = r->endContainer();
    int endOffset = r->endOffset();

    // Callers should be handing us well-formed ranges. If we discover that this isn't
    // the case, we could consider changing this assertion to an early return.
    ASSERT(r->boundaryPointsValid());

    // remember range - this does not change
    m_startContainer = startContainer;
    m_startOffset = startOffset;
    m_endContainer = endContainer;
    m_endOffset = endOffset;

    for (Node* n = startContainer; n; n = n->parentNode()) {
        if (n->isShadowNode()) {
            m_inShadowContent = true;
            break;
        }
    }

    // set up the current node for processing
    m_node = r->firstNode();
    if (m_node == 0)
        return;
    m_offset = m_node == m_startContainer ? m_startOffset : 0;
    m_handledNode = false;
    m_handledChildren = false;

    // calculate first out of bounds node
    m_pastEndNode = r->pastLastNode();

    // initialize node processing state
    m_needAnotherNewline = false;
    m_textBox = 0;

    // initialize record of previous node processing
    m_haveEmitted = false;
    m_lastTextNode = 0;
    m_lastTextNodeEndedWithCollapsedSpace = false;
    m_lastCharacter = 0;

#ifndef NDEBUG
    // need this just because of the assert in advance()
    m_positionNode = m_node;
#endif

    // identify the first run
    advance();
}

void TextIterator::advance()
{
    // reset the run information
    m_positionNode = 0;
    m_textLength = 0;

    // handle remembered node that needed a newline after the text node's newline
    if (m_needAnotherNewline) {
        // Emit the extra newline, and position it *inside* m_node, after m_node's 
        // contents, in case it's a block, in the same way that we position the first 
        // newline.  The range for the emitted newline should start where the line 
        // break begins.
        // FIXME: It would be cleaner if we emitted two newlines during the last 
        // iteration, instead of using m_needAnotherNewline.
        Node* baseNode = m_node->lastChild() ? m_node->lastChild() : m_node;
        emitCharacter('\n', baseNode->parentNode(), baseNode, 1, 1);
        m_needAnotherNewline = false;
        return;
    }

    // handle remembered text box
    if (m_textBox) {
        handleTextBox();
        if (m_positionNode)
            return;
    }

    while (m_node && m_node != m_pastEndNode) {
        // if the range ends at offset 0 of an element, represent the
        // position, but not the content, of that element e.g. if the
        // node is a blockflow element, emit a newline that
        // precedes the element
        if (m_node == m_endContainer && m_endOffset == 0) {
            representNodeOffsetZero();
            m_node = 0;
            return;
        }
        
        RenderObject *renderer = m_node->renderer();
        if (!renderer) {
            m_handledNode = true;
            m_handledChildren = true;
        } else {
            // handle current node according to its type
            if (!m_handledNode) {
                if (renderer->isText() && m_node->nodeType() == Node::TEXT_NODE) // FIXME: What about CDATA_SECTION_NODE?
                    m_handledNode = handleTextNode();
                else if (renderer && (renderer->isImage() || renderer->isWidget() || (renderer->element() && renderer->element()->isControl())))
                    m_handledNode = handleReplacedElement();
                else
                    m_handledNode = handleNonTextNode();
                if (m_positionNode)
                    return;
            }
        }

        // find a new current node to handle in depth-first manner,
        // calling exitNode() as we come back thru a parent node
        Node *next = m_handledChildren ? 0 : m_node->firstChild();
        m_offset = 0;
        if (!next) {
            next = m_node->nextSibling();
            if (!next) {
                bool pastEnd = m_node->traverseNextNode() == m_pastEndNode;
                Node* parentNode = m_node->parentNode();
                if (!parentNode && m_inShadowContent) {
                    m_inShadowContent = false;
                    parentNode = m_node->shadowParentNode();
                }
                while (!next && parentNode) {
                    if (pastEnd && parentNode == m_endContainer || m_endContainer->isDescendantOf(parentNode))
                        return;
                    bool haveRenderer = m_node->renderer();
                    m_node = parentNode;
                    parentNode = m_node->parentNode();
                    if (!parentNode && m_inShadowContent) {
                        m_inShadowContent = false;
                        parentNode = m_node->shadowParentNode();
                    }
                    if (haveRenderer)
                        exitNode();
                    if (m_positionNode) {
                        m_handledNode = true;
                        m_handledChildren = true;
                        return;
                    }
                    next = m_node->nextSibling();
                }
            }
        }

        // set the new current node
        m_node = next;
        m_handledNode = false;
        m_handledChildren = false;

        // how would this ever be?
        if (m_positionNode)
            return;
    }
}

static inline bool compareBoxStart(const InlineTextBox *first, const InlineTextBox *second)
{
    return first->start() < second->start();
}

bool TextIterator::handleTextNode()
{
    RenderText* renderer = toRenderText(m_node->renderer());
    if (renderer->style()->visibility() != VISIBLE)
        return false;
        
    m_lastTextNode = m_node;
    String str = renderer->text();

    // handle pre-formatted text
    if (!renderer->style()->collapseWhiteSpace()) {
        int runStart = m_offset;
        if (m_lastTextNodeEndedWithCollapsedSpace) {
            emitCharacter(' ', m_node, 0, runStart, runStart);
            return false;
        }
        int strLength = str.length();
        int end = (m_node == m_endContainer) ? m_endOffset : INT_MAX;
        int runEnd = min(strLength, end);

        if (runStart >= runEnd)
            return true;

        emitText(m_node, runStart, runEnd);
        return true;
    }

    if (!renderer->firstTextBox() && str.length() > 0) {
        m_lastTextNodeEndedWithCollapsedSpace = true; // entire block is collapsed space
        return true;
    }

    // Used when text boxes are out of order (Hebrew/Arabic w/ embeded LTR text)
    if (renderer->containsReversedText()) {
        m_sortedTextBoxes.clear();
        for (InlineTextBox * textBox = renderer->firstTextBox(); textBox; textBox = textBox->nextTextBox()) {
            m_sortedTextBoxes.append(textBox);
        }
        std::sort(m_sortedTextBoxes.begin(), m_sortedTextBoxes.end(), compareBoxStart); 
        m_sortedTextBoxesPosition = 0;
    }
    
    m_textBox = renderer->containsReversedText() ? m_sortedTextBoxes[0] : renderer->firstTextBox();
    handleTextBox();
    return true;
}

void TextIterator::handleTextBox()
{    
    RenderText *renderer = toRenderText(m_node->renderer());
    String str = renderer->text();
    int start = m_offset;
    int end = (m_node == m_endContainer) ? m_endOffset : INT_MAX;
    while (m_textBox) {
        int textBoxStart = m_textBox->start();
        int runStart = max(textBoxStart, start);

        // Check for collapsed space at the start of this run.
        InlineTextBox *firstTextBox = renderer->containsReversedText() ? m_sortedTextBoxes[0] : renderer->firstTextBox();
        bool needSpace = m_lastTextNodeEndedWithCollapsedSpace
            || (m_textBox == firstTextBox && textBoxStart == runStart && runStart > 0);
        if (needSpace && !isCollapsibleWhitespace(m_lastCharacter) && m_lastCharacter) {
            if (m_lastTextNode == m_node && runStart > 0 && str[runStart - 1] == ' ') {
                unsigned spaceRunStart = runStart - 1;
                while (spaceRunStart > 0 && str[spaceRunStart - 1] == ' ')
                    --spaceRunStart;
                emitText(m_node, spaceRunStart, spaceRunStart + 1);
            } else
                emitCharacter(' ', m_node, 0, runStart, runStart);
            return;
        }
        int textBoxEnd = textBoxStart + m_textBox->len();
        int runEnd = min(textBoxEnd, end);
        
        // Determine what the next text box will be, but don't advance yet
        InlineTextBox *nextTextBox = 0;
        if (renderer->containsReversedText()) {
            if (m_sortedTextBoxesPosition + 1 < m_sortedTextBoxes.size())
                nextTextBox = m_sortedTextBoxes[m_sortedTextBoxesPosition + 1];
        } else 
            nextTextBox = m_textBox->nextTextBox();

        if (runStart < runEnd) {
            // Handle either a single newline character (which becomes a space),
            // or a run of characters that does not include a newline.
            // This effectively translates newlines to spaces without copying the text.
            if (str[runStart] == '\n') {
                emitCharacter(' ', m_node, 0, runStart, runStart + 1);
                m_offset = runStart + 1;
            } else {
                int subrunEnd = str.find('\n', runStart);
                if (subrunEnd == -1 || subrunEnd > runEnd)
                    subrunEnd = runEnd;
    
                m_offset = subrunEnd;
                emitText(m_node, runStart, subrunEnd);
            }

            // If we are doing a subrun that doesn't go to the end of the text box,
            // come back again to finish handling this text box; don't advance to the next one.
            if (m_positionEndOffset < textBoxEnd)
                return;

            // Advance and return
            int nextRunStart = nextTextBox ? nextTextBox->start() : str.length();
            if (nextRunStart > runEnd)
                m_lastTextNodeEndedWithCollapsedSpace = true; // collapsed space between runs or at the end
            m_textBox = nextTextBox;
            if (renderer->containsReversedText())
                ++m_sortedTextBoxesPosition;
            return;
        }
        // Advance and continue
        m_textBox = nextTextBox;
        if (renderer->containsReversedText())
            ++m_sortedTextBoxesPosition;
    }
}

bool TextIterator::handleReplacedElement()
{
    RenderObject* renderer = m_node->renderer();
    if (renderer->style()->visibility() != VISIBLE)
        return false;

    if (m_lastTextNodeEndedWithCollapsedSpace) {
        emitCharacter(' ', m_lastTextNode->parentNode(), m_lastTextNode, 1, 1);
        return false;
    }

    if (m_enterTextControls && (renderer->isTextArea() || renderer->isTextField())) {
        m_node = static_cast<RenderTextControl*>(renderer)->innerTextElement();
        m_offset = 0;
        m_inShadowContent = true;
        return false;
    }

    m_haveEmitted = true;

    if (m_emitCharactersBetweenAllVisiblePositions) {
        // We want replaced elements to behave like punctuation for boundary 
        // finding, and to simply take up space for the selection preservation 
        // code in moveParagraphs, so we use a comma.
        emitCharacter(',', m_node->parentNode(), m_node, 0, 1);
        return true;
    }

    m_positionNode = m_node->parentNode();
    m_positionOffsetBaseNode = m_node;
    m_positionStartOffset = 0;
    m_positionEndOffset = 1;

    m_textCharacters = 0;
    m_textLength = 0;

    m_lastCharacter = 0;

    return true;
}

static bool shouldEmitTabBeforeNode(Node* node)
{
    RenderObject* r = node->renderer();
    
    // Table cells are delimited by tabs.
    if (!r || !isTableCell(node))
        return false;
    
    // Want a tab before every cell other than the first one
    RenderTableCell* rc = static_cast<RenderTableCell*>(r);
    RenderTable* t = rc->table();
    return t && (t->cellBefore(rc) || t->cellAbove(rc));
}

static bool shouldEmitNewlineForNode(Node* node)
{
    // br elements are represented by a single newline.
    RenderObject* r = node->renderer();
    if (!r)
        return node->hasTagName(brTag);
        
    return r->isBR();
}

static bool shouldEmitNewlinesBeforeAndAfterNode(Node* node)
{
    // Block flow (versus inline flow) is represented by having
    // a newline both before and after the element.
    RenderObject* r = node->renderer();
    if (!r) {
        return (node->hasTagName(blockquoteTag)
                || node->hasTagName(ddTag)
                || node->hasTagName(divTag)
                || node->hasTagName(dlTag)
                || node->hasTagName(dtTag)
                || node->hasTagName(h1Tag)
                || node->hasTagName(h2Tag)
                || node->hasTagName(h3Tag)
                || node->hasTagName(h4Tag)
                || node->hasTagName(h5Tag)
                || node->hasTagName(h6Tag)
                || node->hasTagName(hrTag)
                || node->hasTagName(liTag)
                || node->hasTagName(listingTag)
                || node->hasTagName(olTag)
                || node->hasTagName(pTag)
                || node->hasTagName(preTag)
                || node->hasTagName(trTag)
                || node->hasTagName(ulTag));
    }
    
    // Need to make an exception for table cells, because they are blocks, but we
    // want them tab-delimited rather than having newlines before and after.
    if (isTableCell(node))
        return false;
    
    // Need to make an exception for table row elements, because they are neither
    // "inline" or "RenderBlock", but we want newlines for them.
    if (r->isTableRow()) {
        RenderTable* t = static_cast<RenderTableRow*>(r)->table();
        if (t && !t->isInline())
            return true;
    }
    
    return !r->isInline() && r->isRenderBlock() && !r->isFloatingOrPositioned() && !r->isBody();
}

static bool shouldEmitNewlineAfterNode(Node* node)
{
    // FIXME: It should be better but slower to create a VisiblePosition here.
    if (!shouldEmitNewlinesBeforeAndAfterNode(node))
        return false;
    // Check if this is the very last renderer in the document.
    // If so, then we should not emit a newline.
    while ((node = node->traverseNextSibling()))
        if (node->renderer())
            return true;
    return false;
}

static bool shouldEmitNewlineBeforeNode(Node* node)
{
    return shouldEmitNewlinesBeforeAndAfterNode(node); 
}

static bool shouldEmitExtraNewlineForNode(Node* node)
{
    // When there is a significant collapsed bottom margin, emit an extra
    // newline for a more realistic result.  We end up getting the right
    // result even without margin collapsing. For example: <div><p>text</p></div>
    // will work right even if both the <div> and the <p> have bottom margins.
    RenderObject* r = node->renderer();
    if (!r || !r->isBox())
        return false;
    
    // NOTE: We only do this for a select set of nodes, and fwiw WinIE appears
    // not to do this at all
    if (node->hasTagName(h1Tag)
        || node->hasTagName(h2Tag)
        || node->hasTagName(h3Tag)
        || node->hasTagName(h4Tag)
        || node->hasTagName(h5Tag)
        || node->hasTagName(h6Tag)
        || node->hasTagName(pTag)) {
        RenderStyle* style = r->style();
        if (style) {
            int bottomMargin = toRenderBox(r)->collapsedMarginBottom();
            int fontSize = style->fontDescription().computedPixelSize();
            if (bottomMargin * 2 >= fontSize)
                return true;
        }
    }
    
    return false;
}

// Whether or not we should emit a character as we enter m_node (if it's a container) or as we hit it (if it's atomic).
bool TextIterator::shouldRepresentNodeOffsetZero()
{
    if (m_emitCharactersBetweenAllVisiblePositions && m_node->renderer() && m_node->renderer()->isTable())
        return true;
        
    // Leave element positioned flush with start of a paragraph
    // (e.g. do not insert tab before a table cell at the start of a paragraph)
    if (m_lastCharacter == '\n')
        return false;
    
    // Otherwise, show the position if we have emitted any characters
    if (m_haveEmitted)
        return true;
    
    // We've not emitted anything yet. Generally, there is no need for any positioning then.
    // The only exception is when the element is visually not in the same line as
    // the start of the range (e.g. the range starts at the end of the previous paragraph).
    // NOTE: Creating VisiblePositions and comparing them is relatively expensive, so we
    // make quicker checks to possibly avoid that. Another check that we could make is
    // is whether the inline vs block flow changed since the previous visible element.
    // I think we're already in a special enough case that that won't be needed, tho.

    // No character needed if this is the first node in the range.
    if (m_node == m_startContainer)
        return false;
    
    // If we are outside the start container's subtree, assume we need to emit.
    // FIXME: m_startContainer could be an inline block
    if (!m_node->isDescendantOf(m_startContainer))
        return true;

    // If we started as m_startContainer offset 0 and the current node is a descendant of
    // the start container, we already had enough context to correctly decide whether to
    // emit after a preceding block. We chose not to emit (m_haveEmitted is false),
    // so don't second guess that now.
    // NOTE: Is this really correct when m_node is not a leftmost descendant? Probably
    // immaterial since we likely would have already emitted something by now.
    if (m_startOffset == 0)
        return false;
        
    // If this node is unrendered or invisible the VisiblePosition checks below won't have much meaning.
    // Additionally, if the range we are iterating over contains huge sections of unrendered content, 
    // we would create VisiblePositions on every call to this function without this check.
    if (!m_node->renderer() || m_node->renderer()->style()->visibility() != VISIBLE)
        return false;
    
    // The currPos.isNotNull() check is needed because positions in non-html content
    // (like svg) do not have visible positions, and we don't want to emit for them either.
    VisiblePosition startPos = VisiblePosition(m_startContainer, m_startOffset, DOWNSTREAM);
    VisiblePosition currPos = VisiblePosition(m_node, 0, DOWNSTREAM);
    return currPos.isNotNull() && !inSameLine(startPos, currPos);
}

bool TextIterator::shouldEmitSpaceBeforeAndAfterNode(Node* node)
{
    return node->renderer() && node->renderer()->isTable() && (node->renderer()->isInline() || m_emitCharactersBetweenAllVisiblePositions);
}

void TextIterator::representNodeOffsetZero()
{
    // Emit a character to show the positioning of m_node.
    
    // When we haven't been emitting any characters, shouldRepresentNodeOffsetZero() can 
    // create VisiblePositions, which is expensive.  So, we perform the inexpensive checks
    // on m_node to see if it necessitates emitting a character first and will early return 
    // before encountering shouldRepresentNodeOffsetZero()s worse case behavior.
    if (shouldEmitTabBeforeNode(m_node)) {
        if (shouldRepresentNodeOffsetZero())
            emitCharacter('\t', m_node->parentNode(), m_node, 0, 0);
    } else if (shouldEmitNewlineBeforeNode(m_node)) {
        if (shouldRepresentNodeOffsetZero())
            emitCharacter('\n', m_node->parentNode(), m_node, 0, 0);
    } else if (shouldEmitSpaceBeforeAndAfterNode(m_node)) {
        if (shouldRepresentNodeOffsetZero())
            emitCharacter(' ', m_node->parentNode(), m_node, 0, 0);
    }
}

bool TextIterator::handleNonTextNode()
{
    if (shouldEmitNewlineForNode(m_node))
        emitCharacter('\n', m_node->parentNode(), m_node, 0, 1);
    else if (m_emitCharactersBetweenAllVisiblePositions && m_node->renderer() && m_node->renderer()->isHR())
        emitCharacter(' ', m_node->parentNode(), m_node, 0, 1);
    else
        representNodeOffsetZero();

    return true;
}

void TextIterator::exitNode()
{
    // prevent emitting a newline when exiting a collapsed block at beginning of the range
    // FIXME: !m_haveEmitted does not necessarily mean there was a collapsed block... it could
    // have been an hr (e.g.). Also, a collapsed block could have height (e.g. a table) and
    // therefore look like a blank line.
    if (!m_haveEmitted)
        return;
        
    // Emit with a position *inside* m_node, after m_node's contents, in 
    // case it is a block, because the run should start where the 
    // emitted character is positioned visually.
    Node* baseNode = m_node->lastChild() ? m_node->lastChild() : m_node;
    // FIXME: This shouldn't require the m_lastTextNode to be true, but we can't change that without making
    // the logic in _web_attributedStringFromRange match.  We'll get that for free when we switch to use
    // TextIterator in _web_attributedStringFromRange.
    // See <rdar://problem/5428427> for an example of how this mismatch will cause problems.
    if (m_lastTextNode && shouldEmitNewlineAfterNode(m_node)) {
        // use extra newline to represent margin bottom, as needed
        bool addNewline = shouldEmitExtraNewlineForNode(m_node);
        
        // FIXME: We need to emit a '\n' as we leave an empty block(s) that
        // contain a VisiblePosition when doing selection preservation.
        if (m_lastCharacter != '\n') {
            // insert a newline with a position following this block's contents.
            emitCharacter('\n', baseNode->parentNode(), baseNode, 1, 1);
            // remember whether to later add a newline for the current node
            ASSERT(!m_needAnotherNewline);
            m_needAnotherNewline = addNewline;
        } else if (addNewline)
            // insert a newline with a position following this block's contents.
            emitCharacter('\n', baseNode->parentNode(), baseNode, 1, 1);
    }
    
    // If nothing was emitted, see if we need to emit a space.
    if (!m_positionNode && shouldEmitSpaceBeforeAndAfterNode(m_node))
        emitCharacter(' ', baseNode->parentNode(), baseNode, 1, 1);
}

void TextIterator::emitCharacter(UChar c, Node *textNode, Node *offsetBaseNode, int textStartOffset, int textEndOffset)
{
    m_haveEmitted = true;
    
    // remember information with which to construct the TextIterator::range()
    // NOTE: textNode is often not a text node, so the range will specify child nodes of positionNode
    m_positionNode = textNode;
    m_positionOffsetBaseNode = offsetBaseNode;
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;
 
    // remember information with which to construct the TextIterator::characters() and length()
    m_singleCharacterBuffer = c;
    m_textCharacters = &m_singleCharacterBuffer;
    m_textLength = 1;

    // remember some iteration state
    m_lastTextNodeEndedWithCollapsedSpace = false;
    m_lastCharacter = c;
}

void TextIterator::emitText(Node* textNode, int textStartOffset, int textEndOffset)
{
    RenderText* renderer = toRenderText(m_node->renderer());
    String str = renderer->text();
    ASSERT(str.characters());

    m_positionNode = textNode;
    m_positionOffsetBaseNode = 0;
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;
    m_textCharacters = str.characters() + textStartOffset;
    m_textLength = textEndOffset - textStartOffset;
    m_lastCharacter = str[textEndOffset - 1];

    m_lastTextNodeEndedWithCollapsedSpace = false;
    m_haveEmitted = true;
}

PassRefPtr<Range> TextIterator::range() const
{
    // use the current run information, if we have it
    if (m_positionNode) {
        if (m_positionOffsetBaseNode) {
            int index = m_positionOffsetBaseNode->nodeIndex();
            m_positionStartOffset += index;
            m_positionEndOffset += index;
            m_positionOffsetBaseNode = 0;
        }
        return Range::create(m_positionNode->document(), m_positionNode, m_positionStartOffset, m_positionNode, m_positionEndOffset);
    }

    // otherwise, return the end of the overall range we were given
    if (m_endContainer)
        return Range::create(m_endContainer->document(), m_endContainer, m_endOffset, m_endContainer, m_endOffset);
        
    return 0;
}
    
Node* TextIterator::node() const
{
    RefPtr<Range> textRange = range();
    if (!textRange)
        return 0;

    Node* node = textRange->startContainer();
    if (!node)
        return 0;
    if (node->offsetInCharacters())
        return node;
    
    return node->childNode(textRange->startOffset());
}

// --------

SimplifiedBackwardsTextIterator::SimplifiedBackwardsTextIterator() : m_positionNode(0)
{
}

SimplifiedBackwardsTextIterator::SimplifiedBackwardsTextIterator(const Range *r)
{
    m_positionNode = 0;

    if (!r)
        return;

    Node* startNode = r->startContainer();
    if (!startNode)
        return;
    Node* endNode = r->endContainer();
    int startOffset = r->startOffset();
    int endOffset = r->endOffset();

    if (!startNode->offsetInCharacters()) {
        if (startOffset >= 0 && startOffset < static_cast<int>(startNode->childNodeCount())) {
            startNode = startNode->childNode(startOffset);
            startOffset = 0;
        }
    }
    if (!endNode->offsetInCharacters()) {
        if (endOffset > 0 && endOffset <= static_cast<int>(endNode->childNodeCount())) {
            endNode = endNode->childNode(endOffset - 1);
            endOffset = endNode->offsetInCharacters() ? endNode->maxCharacterOffset() : endNode->childNodeCount();
        }
    }

    m_node = endNode;
    m_offset = endOffset;
    m_handledNode = false;
    m_handledChildren = endOffset == 0;

    m_startNode = startNode;
    m_startOffset = startOffset;
    m_endNode = endNode;
    m_endOffset = endOffset;
    
#ifndef NDEBUG
    // Need this just because of the assert.
    m_positionNode = endNode;
#endif

    m_lastTextNode = 0;
    m_lastCharacter = '\n';
    
    if (startOffset == 0 || !startNode->firstChild()) {
        m_pastStartNode = startNode->previousSibling();
        while (!m_pastStartNode && startNode->parentNode()) {
            startNode = startNode->parentNode();
            m_pastStartNode = startNode->previousSibling();
        }
    } else
        m_pastStartNode = startNode->childNode(startOffset - 1);

    advance();
}

void SimplifiedBackwardsTextIterator::advance()
{
    ASSERT(m_positionNode);

    m_positionNode = 0;
    m_textLength = 0;

    while (m_node && m_node != m_pastStartNode) {
        // Don't handle node if we start iterating at [node, 0].
        if (!m_handledNode && !(m_node == m_endNode && m_endOffset == 0)) {
            RenderObject *renderer = m_node->renderer();
            if (renderer && renderer->isText() && m_node->nodeType() == Node::TEXT_NODE) {
                // FIXME: What about CDATA_SECTION_NODE?
                if (renderer->style()->visibility() == VISIBLE && m_offset > 0)
                    m_handledNode = handleTextNode();
            } else if (renderer && (renderer->isImage() || renderer->isWidget())) {
                if (renderer->style()->visibility() == VISIBLE && m_offset > 0)
                    m_handledNode = handleReplacedElement();
            } else
                m_handledNode = handleNonTextNode();
            if (m_positionNode)
                return;
        }

        Node* next = m_handledChildren ? 0 : m_node->lastChild();
        if (!next) {
            // Exit empty containers as we pass over them or containers
            // where [container, 0] is where we started iterating.
            if (!m_handledNode &&
                canHaveChildrenForEditing(m_node) && 
                m_node->parentNode() && 
                (!m_node->lastChild() || m_node == m_endNode && m_endOffset == 0)) {
                exitNode();
                if (m_positionNode) {
                    m_handledNode = true;
                    m_handledChildren = true;
                    return;
                }            
            }
            // Exit all other containers.
            next = m_node->previousSibling();
            while (!next) {
                if (!m_node->parentNode())
                    break;
                m_node = m_node->parentNode();
                exitNode();
                if (m_positionNode) {
                    m_handledNode = true;
                    m_handledChildren = true;
                    return;
                }
                next = m_node->previousSibling();
            }
        }
        
        m_node = next;
        m_offset = m_node ? caretMaxOffset(m_node) : 0;
        m_handledNode = false;
        m_handledChildren = false;
        
        if (m_positionNode)
            return;
    }
}

bool SimplifiedBackwardsTextIterator::handleTextNode()
{
    m_lastTextNode = m_node;

    RenderText *renderer = toRenderText(m_node->renderer());
    String str = renderer->text();

    if (!renderer->firstTextBox() && str.length() > 0)
        return true;

    m_positionEndOffset = m_offset;

    m_offset = (m_node == m_startNode) ? m_startOffset : 0;
    m_positionNode = m_node;
    m_positionStartOffset = m_offset;
    m_textLength = m_positionEndOffset - m_positionStartOffset;
    m_textCharacters = str.characters() + m_positionStartOffset;

    m_lastCharacter = str[m_positionEndOffset - 1];

    return true;
}

bool SimplifiedBackwardsTextIterator::handleReplacedElement()
{
    unsigned index = m_node->nodeIndex();
    // We want replaced elements to behave like punctuation for boundary 
    // finding, and to simply take up space for the selection preservation 
    // code in moveParagraphs, so we use a comma.  Unconditionally emit
    // here because this iterator is only used for boundary finding.
    emitCharacter(',', m_node->parentNode(), index, index + 1);
    return true;
}

bool SimplifiedBackwardsTextIterator::handleNonTextNode()
{    
    // We can use a linefeed in place of a tab because this simple iterator is only used to
    // find boundaries, not actual content.  A linefeed breaks words, sentences, and paragraphs.
    if (shouldEmitNewlineForNode(m_node) ||
        shouldEmitNewlineAfterNode(m_node) ||
        shouldEmitTabBeforeNode(m_node)) {
        unsigned index = m_node->nodeIndex();
        // The start of this emitted range is wrong, ensuring correctness would require
        // VisiblePositions and so would be slow.  previousBoundary expects this.
        emitCharacter('\n', m_node->parentNode(), index + 1, index + 1);
    }
    
    return true;
}

void SimplifiedBackwardsTextIterator::exitNode()
{
    if (shouldEmitNewlineForNode(m_node) ||
        shouldEmitNewlineBeforeNode(m_node) ||
        shouldEmitTabBeforeNode(m_node))
        // The start of this emitted range is wrong, ensuring correctness would require
        // VisiblePositions and so would be slow.  previousBoundary expects this.
        emitCharacter('\n', m_node, 0, 0);
}

void SimplifiedBackwardsTextIterator::emitCharacter(UChar c, Node *node, int startOffset, int endOffset)
{
    m_singleCharacterBuffer = c;
    m_positionNode = node;
    m_positionStartOffset = startOffset;
    m_positionEndOffset = endOffset;
    m_textCharacters = &m_singleCharacterBuffer;
    m_textLength = 1;
    m_lastCharacter = c;
}

PassRefPtr<Range> SimplifiedBackwardsTextIterator::range() const
{
    if (m_positionNode)
        return Range::create(m_positionNode->document(), m_positionNode, m_positionStartOffset, m_positionNode, m_positionEndOffset);
    
    return Range::create(m_startNode->document(), m_startNode, m_startOffset, m_startNode, m_startOffset);
}

// --------

CharacterIterator::CharacterIterator()
    : m_offset(0)
    , m_runOffset(0)
    , m_atBreak(true)
{
}

CharacterIterator::CharacterIterator(const Range *r, bool emitCharactersBetweenAllVisiblePositions, bool enterTextControls)
    : m_offset(0)
    , m_runOffset(0)
    , m_atBreak(true)
    , m_textIterator(r, emitCharactersBetweenAllVisiblePositions, enterTextControls)
{
    while (!atEnd() && m_textIterator.length() == 0)
        m_textIterator.advance();
}

PassRefPtr<Range> CharacterIterator::range() const
{
    RefPtr<Range> r = m_textIterator.range();
    if (!m_textIterator.atEnd()) {
        if (m_textIterator.length() <= 1) {
            ASSERT(m_runOffset == 0);
        } else {
            Node* n = r->startContainer();
            ASSERT(n == r->endContainer());
            int offset = r->startOffset() + m_runOffset;
            ExceptionCode ec = 0;
            r->setStart(n, offset, ec);
            r->setEnd(n, offset + 1, ec);
            ASSERT(!ec);
        }
    }
    return r.release();
}

void CharacterIterator::advance(int count)
{
    if (count <= 0) {
        ASSERT(count == 0);
        return;
    }
    
    m_atBreak = false;

    // easy if there is enough left in the current m_textIterator run
    int remaining = m_textIterator.length() - m_runOffset;
    if (count < remaining) {
        m_runOffset += count;
        m_offset += count;
        return;
    }

    // exhaust the current m_textIterator run
    count -= remaining;
    m_offset += remaining;
    
    // move to a subsequent m_textIterator run
    for (m_textIterator.advance(); !atEnd(); m_textIterator.advance()) {
        int runLength = m_textIterator.length();
        if (runLength == 0)
            m_atBreak = true;
        else {
            // see whether this is m_textIterator to use
            if (count < runLength) {
                m_runOffset = count;
                m_offset += count;
                return;
            }
            
            // exhaust this m_textIterator run
            count -= runLength;
            m_offset += runLength;
        }
    }

    // ran to the end of the m_textIterator... no more runs left
    m_atBreak = true;
    m_runOffset = 0;
}

String CharacterIterator::string(int numChars)
{
    Vector<UChar> result;
    result.reserveInitialCapacity(numChars);
    while (numChars > 0 && !atEnd()) {
        int runSize = min(numChars, length());
        result.append(characters(), runSize);
        numChars -= runSize;
        advance(runSize);
    }
    return String::adopt(result);
}

static PassRefPtr<Range> characterSubrange(CharacterIterator& it, int offset, int length)
{
    it.advance(offset);
    RefPtr<Range> start = it.range();

    if (length > 1)
        it.advance(length - 1);
    RefPtr<Range> end = it.range();

    return Range::create(start->startContainer()->document(), 
        start->startContainer(), start->startOffset(), 
        end->endContainer(), end->endOffset());
}

BackwardsCharacterIterator::BackwardsCharacterIterator()
    : m_offset(0)
    , m_runOffset(0)
    , m_atBreak(true)
{
}

BackwardsCharacterIterator::BackwardsCharacterIterator(const Range* range)
    : m_offset(0)
    , m_runOffset(0)
    , m_atBreak(true)
    , m_textIterator(range)
{
    while (!atEnd() && !m_textIterator.length())
        m_textIterator.advance();
}

PassRefPtr<Range> BackwardsCharacterIterator::range() const
{
    RefPtr<Range> r = m_textIterator.range();
    if (!m_textIterator.atEnd()) {
        if (m_textIterator.length() <= 1)
            ASSERT(m_runOffset == 0);
        else {
            Node* n = r->startContainer();
            ASSERT(n == r->endContainer());
            int offset = r->endOffset() - m_runOffset;
            ExceptionCode ec = 0;
            r->setStart(n, offset - 1, ec);
            r->setEnd(n, offset, ec);
            ASSERT(!ec);
        }
    }
    return r.release();
}

void BackwardsCharacterIterator::advance(int count)
{
    if (count <= 0) {
        ASSERT(!count);
        return;
    }

    m_atBreak = false;

    int remaining = m_textIterator.length() - m_runOffset;
    if (count < remaining) {
        m_runOffset += count;
        m_offset += count;
        return;
    }

    count -= remaining;
    m_offset += remaining;

    for (m_textIterator.advance(); !atEnd(); m_textIterator.advance()) {
        int runLength = m_textIterator.length();
        if (runLength == 0)
            m_atBreak = true;
        else {
            if (count < runLength) {
                m_runOffset = count;
                m_offset += count;
                return;
            }
            
            count -= runLength;
            m_offset += runLength;
        }
    }

    m_atBreak = true;
    m_runOffset = 0;
}

// --------

WordAwareIterator::WordAwareIterator()
: m_previousText(0), m_didLookAhead(false)
{
}

WordAwareIterator::WordAwareIterator(const Range *r)
: m_previousText(0), m_didLookAhead(false), m_textIterator(r)
{
    m_didLookAhead = true;  // so we consider the first chunk from the text iterator
    advance();              // get in position over the first chunk of text
}

// We're always in one of these modes:
// - The current chunk in the text iterator is our current chunk
//      (typically its a piece of whitespace, or text that ended with whitespace)
// - The previous chunk in the text iterator is our current chunk
//      (we looked ahead to the next chunk and found a word boundary)
// - We built up our own chunk of text from many chunks from the text iterator

// FIXME: Perf could be bad for huge spans next to each other that don't fall on word boundaries

void WordAwareIterator::advance()
{
    m_previousText = 0;
    m_buffer.clear();      // toss any old buffer we built up

    // If last time we did a look-ahead, start with that looked-ahead chunk now
    if (!m_didLookAhead) {
        ASSERT(!m_textIterator.atEnd());
        m_textIterator.advance();
    }
    m_didLookAhead = false;

    // Go to next non-empty chunk 
    while (!m_textIterator.atEnd() && m_textIterator.length() == 0)
        m_textIterator.advance();
    m_range = m_textIterator.range();

    if (m_textIterator.atEnd())
        return;
    
    while (1) {
        // If this chunk ends in whitespace we can just use it as our chunk.
        if (isSpaceOrNewline(m_textIterator.characters()[m_textIterator.length() - 1]))
            return;

        // If this is the first chunk that failed, save it in previousText before look ahead
        if (m_buffer.isEmpty()) {
            m_previousText = m_textIterator.characters();
            m_previousLength = m_textIterator.length();
        }

        // Look ahead to next chunk.  If it is whitespace or a break, we can use the previous stuff
        m_textIterator.advance();
        if (m_textIterator.atEnd() || m_textIterator.length() == 0 || isSpaceOrNewline(m_textIterator.characters()[0])) {
            m_didLookAhead = true;
            return;
        }

        if (m_buffer.isEmpty()) {
            // Start gobbling chunks until we get to a suitable stopping point
            m_buffer.append(m_previousText, m_previousLength);
            m_previousText = 0;
        }
        m_buffer.append(m_textIterator.characters(), m_textIterator.length());
        int exception = 0;
        m_range->setEnd(m_textIterator.range()->endContainer(), m_textIterator.range()->endOffset(), exception);
    }
}

int WordAwareIterator::length() const
{
    if (!m_buffer.isEmpty())
        return m_buffer.size();
    if (m_previousText)
        return m_previousLength;
    return m_textIterator.length();
}

const UChar* WordAwareIterator::characters() const
{
    if (!m_buffer.isEmpty())
        return m_buffer.data();
    if (m_previousText)
        return m_previousText;
    return m_textIterator.characters();
}

// --------

#if USE(ICU_UNICODE) && !UCONFIG_NO_COLLATION

static const size_t minimumSearchBufferSize = 8192;

#ifndef NDEBUG
static bool searcherInUse;
#endif

static UStringSearch* createSearcher()
{
    // Provide a non-empty pattern and non-empty text so usearch_open will not fail,
    // but it doesn't matter exactly what it is, since we don't perform any searches
    // without setting both the pattern and the text.

    // Pass empty string for the locale for now to get the Unicode Collation Algorithm,
    // rather than something locale-specific.

    UErrorCode status = U_ZERO_ERROR;
    UStringSearch* searcher = usearch_open(&newlineCharacter, 1, &newlineCharacter, 1, "", 0, &status);
    ASSERT(status == U_ZERO_ERROR);
    return searcher;
}

static UStringSearch* searcher()
{
    static UStringSearch* searcher = createSearcher();
    return searcher;
}

static inline void lockSearcher()
{
#ifndef NDEBUG
    ASSERT(!searcherInUse);
    searcherInUse = true;
#endif
}

static inline void unlockSearcher()
{
#ifndef NDEBUG
    ASSERT(searcherInUse);
    searcherInUse = false;
#endif
}

inline SearchBuffer::SearchBuffer(const String& target, bool isCaseSensitive)
    : m_target(target)
    , m_atBreak(true)
{
    ASSERT(!m_target.isEmpty());

    size_t targetLength = target.length();
    m_buffer.reserveInitialCapacity(max(targetLength * 8, minimumSearchBufferSize));
    m_overlap = m_buffer.capacity() / 4;

    // Grab the single global searcher.
    // If we ever have a reason to do more than once search buffer at once, we'll have
    // to move to multiple searchers.
    lockSearcher();

    UStringSearch* searcher = WebCore::searcher();
    UCollator* collator = usearch_getCollator(searcher);

    UCollationStrength strength = isCaseSensitive ? UCOL_TERTIARY : UCOL_PRIMARY;
    if (ucol_getStrength(collator) != strength) {
        ucol_setStrength(collator, strength);
        usearch_reset(searcher);
    }

    UErrorCode status = U_ZERO_ERROR;
    usearch_setPattern(searcher, m_target.characters(), targetLength, &status);
    ASSERT(status == U_ZERO_ERROR);
}

inline SearchBuffer::~SearchBuffer()
{
    unlockSearcher();
}

inline size_t SearchBuffer::append(const UChar* characters, size_t length)
{
    ASSERT(length);

    if (m_atBreak) {
        m_buffer.shrink(0);
        m_atBreak = false;
    } else if (m_buffer.size() == m_buffer.capacity()) {
        memcpy(m_buffer.data(), m_buffer.data() + m_buffer.size() - m_overlap, m_overlap * sizeof(UChar));
        m_buffer.shrink(m_overlap);
    }

    size_t usableLength = min(m_buffer.capacity() - m_buffer.size(), length);
    ASSERT(usableLength);
    m_buffer.append(characters, usableLength);
    return usableLength;
}

inline bool SearchBuffer::atBreak() const
{
    return m_atBreak;
}

inline void SearchBuffer::reachedBreak()
{
    m_atBreak = true;
}

inline size_t SearchBuffer::search(size_t& start)
{
    size_t size = m_buffer.size();
    if (m_atBreak) {
        if (!size)
            return 0;
    } else {
        if (size != m_buffer.capacity())
            return 0;
    }

    UStringSearch* searcher = WebCore::searcher();

    UErrorCode status = U_ZERO_ERROR;
    usearch_setText(searcher, m_buffer.data(), size, &status);
    ASSERT(status == U_ZERO_ERROR);

    int matchStart = usearch_first(searcher, &status);
    ASSERT(status == U_ZERO_ERROR);
    if (!(matchStart >= 0 && static_cast<size_t>(matchStart) < size)) {
        ASSERT(matchStart == USEARCH_DONE);
        return 0;
    }

    // Matches that start in the overlap area are only tentative.
    // The same match may appear later, matching more characters,
    // possibly including a combining character that's not yet in the buffer.
    if (!m_atBreak && static_cast<size_t>(matchStart) >= size - m_overlap) {
        memcpy(m_buffer.data(), m_buffer.data() + size - m_overlap, m_overlap * sizeof(UChar));
        m_buffer.shrink(m_overlap);
        return 0;
    }

    size_t newSize = size - (matchStart + 1);
    memmove(m_buffer.data(), m_buffer.data() + matchStart + 1, newSize * sizeof(UChar));
    m_buffer.shrink(newSize);

    start = size - matchStart;
    return usearch_getMatchedLength(searcher);
}

#else // !ICU_UNICODE

inline SearchBuffer::SearchBuffer(const String& target, bool isCaseSensitive)
    : m_target(isCaseSensitive ? target : target.foldCase())
    , m_isCaseSensitive(isCaseSensitive)
    , m_buffer(m_target.length())
    , m_isCharacterStartBuffer(m_target.length())
    , m_isBufferFull(false)
    , m_cursor(0)
{
    ASSERT(!m_target.isEmpty());
    m_target.replace(noBreakSpace, ' ');
}

inline SearchBuffer::~SearchBuffer()
{
}

inline void SearchBuffer::reachedBreak()
{
    m_cursor = 0;
    m_isBufferFull = false;
}

inline bool SearchBuffer::atBreak() const
{
    return !m_cursor && !m_isBufferFull;
}

inline void SearchBuffer::append(UChar c, bool isStart)
{
    m_buffer[m_cursor] = c == noBreakSpace ? ' ' : c;
    m_isCharacterStartBuffer[m_cursor] = isStart;
    if (++m_cursor == m_target.length()) {
        m_cursor = 0;
        m_isBufferFull = true;
    }
}

inline size_t SearchBuffer::append(const UChar* characters, size_t length)
{
    ASSERT(length);
    if (m_isCaseSensitive) {
        append(characters[0], true);
        return 1;
    }
    const int maxFoldedCharacters = 16; // sensible maximum is 3, this should be more than enough
    UChar foldedCharacters[maxFoldedCharacters];
    bool error;
    int numFoldedCharacters = foldCase(foldedCharacters, maxFoldedCharacters, characters, 1, &error);
    ASSERT(!error);
    ASSERT(numFoldedCharacters);
    ASSERT(numFoldedCharacters <= maxFoldedCharacters);
    if (!error && numFoldedCharacters) {
        numFoldedCharacters = min(numFoldedCharacters, maxFoldedCharacters);
        append(foldedCharacters[0], true);
        for (int i = 1; i < numFoldedCharacters; ++i)
            append(foldedCharacters[i], false);
    }
    return 1;
}

inline size_t SearchBuffer::search(size_t& start)
{
    if (!m_isBufferFull)
        return 0;
    if (!m_isCharacterStartBuffer[m_cursor])
        return 0;

    size_t tailSpace = m_target.length() - m_cursor;
    if (memcmp(&m_buffer[m_cursor], m_target.characters(), tailSpace * sizeof(UChar)) != 0)
        return 0;
    if (memcmp(&m_buffer[0], m_target.characters() + tailSpace, m_cursor * sizeof(UChar)) != 0)
        return 0;

    start = length();

    // Now that we've found a match once, we don't want to find it again, because those
    // are the SearchBuffer semantics, allowing for a buffer where you append more than one
    // character at a time. To do this we take advantage of m_isCharacterStartBuffer, but if
    // we want to get rid of that in the future we could track this with a separate boolean
    // or even move the characters to the start of the buffer and set m_isBufferFull to false.
    m_isCharacterStartBuffer[m_cursor] = false;

    return start;
}

// Returns the number of characters that were appended to the buffer (what we are searching in).
// That's not necessarily the same length as the passed-in target string, because case folding
// can make two strings match even though they're not the same length.
size_t SearchBuffer::length() const
{
    size_t bufferSize = m_target.length();
    size_t length = 0;
    for (size_t i = 0; i < bufferSize; ++i)
        length += m_isCharacterStartBuffer[i];
    return length;
}

#endif // !ICU_UNICODE

// --------

int TextIterator::rangeLength(const Range *r, bool forSelectionPreservation)
{
    int length = 0;
    for (TextIterator it(r, forSelectionPreservation); !it.atEnd(); it.advance())
        length += it.length();
    
    return length;
}

PassRefPtr<Range> TextIterator::subrange(Range* entireRange, int characterOffset, int characterCount)
{
    CharacterIterator entireRangeIterator(entireRange);
    return characterSubrange(entireRangeIterator, characterOffset, characterCount);
}

PassRefPtr<Range> TextIterator::rangeFromLocationAndLength(Element *scope, int rangeLocation, int rangeLength, bool forSelectionPreservation)
{
    RefPtr<Range> resultRange = scope->document()->createRange();

    int docTextPosition = 0;
    int rangeEnd = rangeLocation + rangeLength;
    bool startRangeFound = false;

    RefPtr<Range> textRunRange;

    TextIterator it(rangeOfContents(scope).get(), forSelectionPreservation);
    
    // FIXME: the atEnd() check shouldn't be necessary, workaround for <http://bugs.webkit.org/show_bug.cgi?id=6289>.
    if (rangeLocation == 0 && rangeLength == 0 && it.atEnd()) {
        textRunRange = it.range();
        
        ExceptionCode ec = 0;
        resultRange->setStart(textRunRange->startContainer(), 0, ec);
        ASSERT(!ec);
        resultRange->setEnd(textRunRange->startContainer(), 0, ec);
        ASSERT(!ec);
        
        return resultRange.release();
    }

    for (; !it.atEnd(); it.advance()) {
        int len = it.length();
        textRunRange = it.range();
        
        bool foundStart = rangeLocation >= docTextPosition && rangeLocation <= docTextPosition + len;
        bool foundEnd = rangeEnd >= docTextPosition && rangeEnd <= docTextPosition + len;
        
        // Fix textRunRange->endPosition(), but only if foundStart || foundEnd, because it is only
        // in those cases that textRunRange is used.
        if (foundStart || foundEnd) {
            // FIXME: This is a workaround for the fact that the end of a run is often at the wrong
            // position for emitted '\n's.
            if (len == 1 && it.characters()[0] == '\n') {
                Position runStart = textRunRange->startPosition();
                Position runEnd = rangeCompliantEquivalent(VisiblePosition(runStart).next().deepEquivalent());
                if (runEnd.isNotNull()) {
                    ExceptionCode ec = 0;
                    textRunRange->setEnd(runEnd.node(), runEnd.offset(), ec);
                    ASSERT(!ec);
                }
            }
        }

        if (foundStart) {
            startRangeFound = true;
            int exception = 0;
            if (textRunRange->startContainer()->isTextNode()) {
                int offset = rangeLocation - docTextPosition;
                resultRange->setStart(textRunRange->startContainer(), offset + textRunRange->startOffset(), exception);
            } else {
                if (rangeLocation == docTextPosition)
                    resultRange->setStart(textRunRange->startContainer(), textRunRange->startOffset(), exception);
                else
                    resultRange->setStart(textRunRange->endContainer(), textRunRange->endOffset(), exception);
            }
        }

        if (foundEnd) {
            int exception = 0;
            if (textRunRange->startContainer()->isTextNode()) {
                int offset = rangeEnd - docTextPosition;
                resultRange->setEnd(textRunRange->startContainer(), offset + textRunRange->startOffset(), exception);
            } else {
                if (rangeEnd == docTextPosition)
                    resultRange->setEnd(textRunRange->startContainer(), textRunRange->startOffset(), exception);
                else
                    resultRange->setEnd(textRunRange->endContainer(), textRunRange->endOffset(), exception);
            }
            docTextPosition += len;
            break;
        }
        docTextPosition += len;
    }
    
    if (!startRangeFound)
        return 0;
    
    if (rangeLength != 0 && rangeEnd > docTextPosition) { // rangeEnd is out of bounds
        int exception = 0;
        resultRange->setEnd(textRunRange->endContainer(), textRunRange->endOffset(), exception);
    }
    
    return resultRange.release();
}

// --------
    
UChar* plainTextToMallocAllocatedBuffer(const Range* r, unsigned& bufferLength, bool isDisplayString) 
{
    UChar* result = 0;

    // Do this in pieces to avoid massive reallocations if there is a large amount of text.
    // Use system malloc for buffers since they can consume lots of memory and current TCMalloc is unable return it back to OS.
    static const unsigned cMaxSegmentSize = 1 << 16;
    bufferLength = 0;
    typedef pair<UChar*, unsigned> TextSegment;
    Vector<TextSegment>* textSegments = 0;
    Vector<UChar> textBuffer;
    textBuffer.reserveInitialCapacity(cMaxSegmentSize);
    for (TextIterator it(r); !it.atEnd(); it.advance()) {
        if (textBuffer.size() && textBuffer.size() + it.length() > cMaxSegmentSize) {
            UChar* newSegmentBuffer = static_cast<UChar*>(malloc(textBuffer.size() * sizeof(UChar)));
            if (!newSegmentBuffer)
                goto exit;
            memcpy(newSegmentBuffer, textBuffer.data(), textBuffer.size() * sizeof(UChar));
            if (!textSegments)
                textSegments = new Vector<TextSegment>;
            textSegments->append(make_pair(newSegmentBuffer, (unsigned)textBuffer.size()));
            textBuffer.clear();
        }
        textBuffer.append(it.characters(), it.length());
        bufferLength += it.length();
    }

    if (!bufferLength)
        return 0;

    // Since we know the size now, we can make a single buffer out of the pieces with one big alloc
    result = static_cast<UChar*>(malloc(bufferLength * sizeof(UChar)));
    if (!result)
        goto exit;

    {
        UChar* resultPos = result;
        if (textSegments) {
            unsigned size = textSegments->size();
            for (unsigned i = 0; i < size; ++i) {
                const TextSegment& segment = textSegments->at(i);
                memcpy(resultPos, segment.first, segment.second * sizeof(UChar));
                resultPos += segment.second;
            }
        }
        memcpy(resultPos, textBuffer.data(), textBuffer.size() * sizeof(UChar));
    }

exit:
    if (textSegments) {
        unsigned size = textSegments->size();
        for (unsigned i = 0; i < size; ++i)
            free(textSegments->at(i).first);
        delete textSegments;
    }
    
    if (isDisplayString && r->ownerDocument())
        r->ownerDocument()->displayBufferModifiedByEncoding(result, bufferLength);

    return result;
}

String plainText(const Range* r)
{
    unsigned length;
    UChar* buf = plainTextToMallocAllocatedBuffer(r, length, false);
    if (!buf)
        return "";
    String result(buf, length);
    free(buf);
    return result;
}

static inline bool isAllCollapsibleWhitespace(const String& string)
{
    const UChar* characters = string.characters();
    unsigned length = string.length();
    for (unsigned i = 0; i < length; ++i) {
        if (!isCollapsibleWhitespace(characters[i]))
            return false;
    }
    return true;
}

static PassRefPtr<Range> collapsedToBoundary(const Range* range, bool forward)
{
    ExceptionCode ec = 0;
    RefPtr<Range> result = range->cloneRange(ec);
    ASSERT(!ec);
    result->collapse(!forward, ec);
    ASSERT(!ec);
    return result.release();
}

static size_t findPlainText(CharacterIterator& it, const String& target, bool forward, bool caseSensitive, size_t& matchStart)
{
    matchStart = 0;
    size_t matchLength = 0;

    SearchBuffer buffer(target, caseSensitive);

    while (!it.atEnd()) {
        it.advance(buffer.append(it.characters(), it.length()));
tryAgain:
        size_t matchStartOffset;
        if (size_t newMatchLength = buffer.search(matchStartOffset)) {
            // Note that we found a match, and where we found it.
            size_t lastCharacterInBufferOffset = it.characterOffset();
            ASSERT(lastCharacterInBufferOffset >= matchStartOffset);
            matchStart = lastCharacterInBufferOffset - matchStartOffset;
            matchLength = newMatchLength;
            // If searching forward, stop on the first match.
            // If searching backward, don't stop, so we end up with the last match.
            if (forward)
                break;
            goto tryAgain;
        }
        if (it.atBreak() && !buffer.atBreak()) {
            buffer.reachedBreak();
            goto tryAgain;
        }
    }

    return matchLength;
}

PassRefPtr<Range> findPlainText(const Range* range, const String& target, bool forward, bool caseSensitive)
{
    // We can't search effectively for a string that's entirely made of collapsible
    // whitespace, so we won't even try. This also takes care of the empty string case.
    if (isAllCollapsibleWhitespace(target))
        return collapsedToBoundary(range, forward);

    // First, find the text.
    size_t matchStart;
    size_t matchLength;
    {
        CharacterIterator findIterator(range, false, true);
        matchLength = findPlainText(findIterator, target, forward, caseSensitive, matchStart);
        if (!matchLength)
            return collapsedToBoundary(range, forward);
    }

    // Then, find the document position of the start and the end of the text.
    CharacterIterator computeRangeIterator(range, false, true);
    return characterSubrange(computeRangeIterator, matchStart, matchLength);
}

}
