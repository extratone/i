/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "InsertParagraphSeparatorCommand.h"

#include "Document.h"
#include "Logging.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "Text.h"
#include "htmlediting.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InsertLineBreakCommand.h"
#include "RenderObject.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

InsertParagraphSeparatorCommand::InsertParagraphSeparatorCommand(Document *document) 
    : CompositeEditCommand(document)
{
}

bool InsertParagraphSeparatorCommand::preservesTypingStyle() const
{
    return true;
}

void InsertParagraphSeparatorCommand::calculateStyleBeforeInsertion(const Position &pos)
{
    // It is only important to set a style to apply later if we're at the boundaries of
    // a paragraph. Otherwise, content that is moved as part of the work of the command
    // will lend their styles to the new paragraph without any extra work needed.
    VisiblePosition visiblePos(pos, VP_DEFAULT_AFFINITY);
    if (!isStartOfParagraph(visiblePos) && !isEndOfParagraph(visiblePos))
        return;
    
    m_style = styleAtPosition(pos);
}

void InsertParagraphSeparatorCommand::applyStyleAfterInsertion()
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (!m_style)
        return;

    CSSComputedStyleDeclaration endingStyle(endingSelection().start().node());
    endingStyle.diff(m_style.get());
    if (m_style->length() > 0)
        applyStyle(m_style.get());
}



void InsertParagraphSeparatorCommand::doApply()
{
    bool splitText = false;
    if (endingSelection().isNone())
        return;
    
    Position pos = endingSelection().start();
        
    EAffinity affinity = endingSelection().affinity();
        
    // Delete the current selection.
    if (endingSelection().isRange()) {
        calculateStyleBeforeInsertion(pos);
        deleteSelection(false, true);
        pos = endingSelection().start();
        affinity = endingSelection().affinity();
    }
    
    // FIXME: Turn into an InsertLineBreak in other cases where we don't want to do the splitting/cloning that
    // InsertParagraphSeparator does.
    Node* block = pos.node()->enclosingBlockFlowElement();
    if (!block ||
        !block->parentNode() ||
        block->renderer() && block->renderer()->isTableCell()) {
        EditCommandPtr cmd(new InsertLineBreakCommand(document())); 
        applyCommandToComposite(cmd);
        return;
    }
    
    // Use the leftmost candidate.
    pos = pos.upstream();
    if (!pos.inRenderedContent())
        pos = pos.downstream();

    // Adjust the insertion position after the delete
    pos = positionAvoidingSpecialElementBoundary(pos);
    VisiblePosition visiblePos(pos, affinity);
    calculateStyleBeforeInsertion(pos);

    //---------------------------------------------------------------------
    // Handle special case of typing return on an empty list item
    if (breakOutOfEmptyListItem())
        return;

    //---------------------------------------------------------------------
    // Prepare for more general cases.
    Node *startNode = pos.node();
    Node *startBlock = startNode->enclosingBlockFlowElement();

    bool isFirstInBlock = isStartOfBlock(visiblePos);
    bool isLastInBlock = isEndOfBlock(visiblePos);
    bool nestNewBlock = false;

    // Create block to be inserted.
    RefPtr<Node> blockToInsert;
    if (startBlock == startBlock->rootEditableElement()) {
        blockToInsert = static_pointer_cast<Node>(createDefaultParagraphElement(document()));
        nestNewBlock = true;
    } else
        blockToInsert = startBlock->cloneNode(false);
    
    //---------------------------------------------------------------------
    // Handle case when position is in the last visible position in its block,
    // including when the block is empty. 
    if (isLastInBlock) {
        if (nestNewBlock) {
            if (isFirstInBlock) {
                // block is empty: create an empty paragraph to
                // represent the content before the new one.
                RefPtr<Node> extraBlock = createDefaultParagraphElement(document());
                appendNode(extraBlock.get(), startBlock);
                appendBlockPlaceholder(extraBlock.get());
            }
            appendNode(blockToInsert.get(), startBlock);
        } else
            insertNodeAfter(blockToInsert.get(), startBlock);

        appendBlockPlaceholder(blockToInsert.get());
        setEndingSelection(Position(blockToInsert.get(), 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        return;
    }

    //---------------------------------------------------------------------
    // Handle case when position is in the first visible position in its block, and
    // similar case where previous position is in another, presumeably nested, block.
    if (isFirstInBlock || !inSameBlock(visiblePos, visiblePos.previous())) {
        pos = pos.downstream();
        Position refPos;
        Node *refNode;
        if (isFirstInBlock && !nestNewBlock)
            refNode = startBlock;
        else if (pos.node() == startBlock && nestNewBlock) {
            refNode = startBlock->childNode(pos.offset());
            ASSERT(refNode); // must be true or we'd be in the end of block case
        } else
            refNode = pos.node();

        insertNodeBefore(blockToInsert.get(), refNode);
        appendBlockPlaceholder(blockToInsert.get());
        setEndingSelection(Position(blockToInsert.get(), 0), DOWNSTREAM);
        applyStyleAfterInsertion();
        setEndingSelection(pos, DOWNSTREAM);
        return;
    }

    //---------------------------------------------------------------------
    // Handle the (more complicated) general case,

    Position upstreamPos = pos.upstream();
    if (upstreamPos.node()->hasTagName(brTag)) {
        RefPtr<Element> br = createBreakElement(document());
        insertNodeAfter(br.get(), upstreamPos.node());
        pos = positionAfterNode(br.get());
    }
    
    // Move downstream. Typing style code will take care of carrying along the 
    // style of the upstream position.
    pos = pos.downstream();
    startNode = pos.node();

    // Build up list of ancestors in between the start node and the start block.
    DeprecatedPtrList<Node> ancestors;
    if (startNode != startBlock) {
        for (Node *n = startNode->parentNode(); n && n != startBlock; n = n->parentNode())
            ancestors.prepend(n);
    }

    // Make sure we do not cause a rendered space to become unrendered.
    // FIXME: We need the affinity for pos, but pos.downstream() does not give it
    Position leadingWhitespace = pos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY);
    // FIXME: leadingWhitespacePosition is returning the position before preserved newlines for positions
    // after the preserved newline, causing the newline to be turned into a nbsp.
    if (leadingWhitespace.isNotNull()) {
        Text* textNode = static_cast<Text*>(leadingWhitespace.node());
        ASSERT(!textNode->renderer() || textNode->renderer()->style()->collapseWhiteSpace());
        replaceTextInNode(textNode, leadingWhitespace.offset(), 1, nonBreakingSpaceString());
    }
    
    // Split at pos if in the middle of a text node.
    if (startNode->isTextNode()) {
        Text *textNode = static_cast<Text *>(startNode);
        bool atEnd = (unsigned)pos.offset() >= textNode->length();
        if (pos.offset() > 0 && !atEnd) {
            splitTextNode(textNode, pos.offset());
            pos = Position(startNode, 0);
            visiblePos = VisiblePosition(pos);
            splitText = true;
        }
    }

    // Put the added block in the tree.
    if (nestNewBlock)
        appendNode(blockToInsert.get(), startBlock);
    else
        insertNodeAfter(blockToInsert.get(), startBlock);

    // Make clones of ancestors in between the start node and the start block.
    RefPtr<Node> parent = blockToInsert;
    for (DeprecatedPtrListIterator<Node> it(ancestors); it.current(); ++it) {
        RefPtr<Node> child = it.current()->cloneNode(false); // shallow clone
        appendNode(child.get(), parent.get());
        parent = child.release();
    }

    // If the paragraph separator was inserted at the end of a paragraph, an empty line must be
    // created.  All of the nodes, starting at visiblePos, are about to be added to the new paragraph 
    // element.  If the first node to be inserted won't be one that will hold an empty line open, add a br.
    if (isEndOfParagraph(visiblePos) && !startNode->hasTagName(brTag) &&
        !(startNode->renderer()->style()->preserveNewline() && visiblePos.characterAfter() == '\n'))
        appendNode(createBreakElement(document()).get(), blockToInsert.get());
        
    // Move the start node and the siblings of the start node.
    if (startNode != startBlock) {
        Node *n = startNode;
        if (pos.offset() >= startNode->caretMaxOffset())
            n = startNode->nextSibling();

        while (n && n != blockToInsert) {
            Node *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent.get());
            n = next;
        }
    }            

    // Move everything after the start node.
    Node *leftParent = ancestors.last();
    while (leftParent && leftParent != startBlock) {
        parent = parent->parentNode();
        Node *n = leftParent->nextSibling();
        while (n && n != blockToInsert) {
            Node *next = n->nextSibling();
            removeNode(n);
            appendNode(n, parent.get());
            n = next;
        }
        leftParent = leftParent->parentNode();
    }

    // Handle whitespace that occurs after the split
    if (splitText) {
        updateLayout();
        pos = Position(startNode, 0);
        if (!pos.isRenderedCharacter()) {
            // Clear out all whitespace and insert one non-breaking space
            ASSERT(startNode);
            ASSERT(startNode->isTextNode());
            ASSERT(!startNode->renderer() || startNode->renderer()->style()->collapseWhiteSpace());
            deleteInsignificantTextDownstream(pos);
            insertTextIntoNode(static_cast<Text*>(startNode), 0, nonBreakingSpaceString());
        }
    }

    setEndingSelection(Position(blockToInsert.get(), 0), DOWNSTREAM);
    applyStyleAfterInsertion();
}

} // namespace WebCore
