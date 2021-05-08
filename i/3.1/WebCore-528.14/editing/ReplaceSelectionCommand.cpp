/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "ReplaceSelectionCommand.h"

#include "ApplyStyleCommand.h"
#include "BeforeTextInsertedEvent.h"
#include "BreakBlockquoteCommand.h" 
#include "CSSComputedStyleDeclaration.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "EditingText.h"
#include "EventNames.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "HTMLInterchange.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "SelectionController.h"
#include "SmartReplace.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "markup.h"
#include "visible_units.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

enum EFragmentType { EmptyFragment, SingleTextNodeFragment, TreeFragment };

// --- ReplacementFragment helper class

class ReplacementFragment : Noncopyable {
public:
    ReplacementFragment(Document*, DocumentFragment*, bool matchStyle, const Selection&);

    Node* firstChild() const;
    Node* lastChild() const;

    bool isEmpty() const;
    
    bool hasInterchangeNewlineAtStart() const { return m_hasInterchangeNewlineAtStart; }
    bool hasInterchangeNewlineAtEnd() const { return m_hasInterchangeNewlineAtEnd; }
    
    void removeNode(PassRefPtr<Node>);
    void removeNodePreservingChildren(Node*);

private:
    PassRefPtr<Node> insertFragmentForTestRendering(Node* context);
    void removeUnrenderedNodes(Node*);
    void restoreTestRenderingNodesToFragment(Node*);
    void removeInterchangeNodes(Node*);
    
    void insertNodeBefore(PassRefPtr<Node> node, Node* refNode);

    RefPtr<Document> m_document;
    RefPtr<DocumentFragment> m_fragment;
    bool m_matchStyle;
    bool m_hasInterchangeNewlineAtStart;
    bool m_hasInterchangeNewlineAtEnd;
};

static bool isInterchangeNewlineNode(const Node *node)
{
    DEFINE_STATIC_LOCAL(String, interchangeNewlineClassString, (AppleInterchangeNewline));
    return node && node->hasTagName(brTag) && 
           static_cast<const Element *>(node)->getAttribute(classAttr) == interchangeNewlineClassString;
}

static bool isInterchangeConvertedSpaceSpan(const Node *node)
{
    DEFINE_STATIC_LOCAL(String, convertedSpaceSpanClassString, (AppleConvertedSpace));
    return node->isHTMLElement() && 
           static_cast<const HTMLElement *>(node)->getAttribute(classAttr) == convertedSpaceSpanClassString;
}

ReplacementFragment::ReplacementFragment(Document* document, DocumentFragment* fragment, bool matchStyle, const Selection& selection)
    : m_document(document),
      m_fragment(fragment),
      m_matchStyle(matchStyle), 
      m_hasInterchangeNewlineAtStart(false), 
      m_hasInterchangeNewlineAtEnd(false)
{
    if (!m_document)
        return;
    if (!m_fragment)
        return;
    if (!m_fragment->firstChild())
        return;
    
    Element* editableRoot = selection.rootEditableElement();
    ASSERT(editableRoot);
    if (!editableRoot)
        return;
    
    Node* shadowAncestorNode = editableRoot->shadowAncestorNode();
    
    if (!editableRoot->inlineEventListenerForType(eventNames().webkitBeforeTextInsertedEvent) &&
        // FIXME: Remove these checks once textareas and textfields actually register an event handler.
        !(shadowAncestorNode && shadowAncestorNode->renderer() && shadowAncestorNode->renderer()->isTextField()) &&
        !(shadowAncestorNode && shadowAncestorNode->renderer() && shadowAncestorNode->renderer()->isTextArea()) &&
        editableRoot->isContentRichlyEditable()) {
        removeInterchangeNodes(m_fragment.get());
        return;
    }

    Node* styleNode = selection.base().node();
    RefPtr<Node> holder = insertFragmentForTestRendering(styleNode);
    
    RefPtr<Range> range = Selection::selectionFromContentsOfNode(holder.get()).toRange();
    String text = plainText(range.get());
    // Give the root a chance to change the text.
    RefPtr<BeforeTextInsertedEvent> evt = BeforeTextInsertedEvent::create(text);
    ExceptionCode ec = 0;
    editableRoot->dispatchEvent(evt, ec);
    ASSERT(ec == 0);
    if (text != evt->text() || !editableRoot->isContentRichlyEditable()) {
        restoreTestRenderingNodesToFragment(holder.get());
        removeNode(holder);

        m_fragment = createFragmentFromText(selection.toRange().get(), evt->text());
        if (!m_fragment->firstChild())
            return;
        holder = insertFragmentForTestRendering(styleNode);
    }
    
    removeInterchangeNodes(holder.get());
    
    removeUnrenderedNodes(holder.get());
    restoreTestRenderingNodesToFragment(holder.get());
    removeNode(holder);
}

bool ReplacementFragment::isEmpty() const
{
    return (!m_fragment || !m_fragment->firstChild()) && !m_hasInterchangeNewlineAtStart && !m_hasInterchangeNewlineAtEnd;
}

Node *ReplacementFragment::firstChild() const 
{ 
    return m_fragment ? m_fragment->firstChild() : 0; 
}

Node *ReplacementFragment::lastChild() const 
{ 
    return m_fragment ? m_fragment->lastChild() : 0; 
}

void ReplacementFragment::removeNodePreservingChildren(Node *node)
{
    if (!node)
        return;

    while (RefPtr<Node> n = node->firstChild()) {
        removeNode(n);
        insertNodeBefore(n.release(), node);
    }
    removeNode(node);
}

void ReplacementFragment::removeNode(PassRefPtr<Node> node)
{
    if (!node)
        return;
    
    Node *parent = node->parentNode();
    if (!parent)
        return;
    
    ExceptionCode ec = 0;
    parent->removeChild(node.get(), ec);
    ASSERT(ec == 0);
}

void ReplacementFragment::insertNodeBefore(PassRefPtr<Node> node, Node* refNode)
{
    if (!node || !refNode)
        return;
        
    Node* parent = refNode->parentNode();
    if (!parent)
        return;
        
    ExceptionCode ec = 0;
    parent->insertBefore(node, refNode, ec);
    ASSERT(ec == 0);
}

PassRefPtr<Node> ReplacementFragment::insertFragmentForTestRendering(Node* context)
{
    Node* body = m_document->body();
    if (!body)
        return 0;

    RefPtr<StyledElement> holder = createDefaultParagraphElement(m_document.get());
    // FIXME: There is a bug, <rdar://problem/6761932>, where holder gets display:none even though it's inserted into a body with 
    // display:block this is a workaround.
    holder->getInlineStyleDecl()->setProperty(CSSPropertyDisplay, CSSValueBlock);
    
    ExceptionCode ec = 0;

    // Copy the whitespace and user-select style from the context onto this element.
    // FIXME: We should examine other style properties to see if they would be appropriate to consider during the test rendering.
    Node* n = context;
    while (n && !n->isElementNode())
        n = n->parentNode();
    if (n) {
        RefPtr<CSSComputedStyleDeclaration> conFontStyle = computedStyle(n);
        CSSStyleDeclaration* style = holder->style();
        style->setProperty(CSSPropertyWhiteSpace, conFontStyle->getPropertyValue(CSSPropertyWhiteSpace), false, ec);
        ASSERT(ec == 0);
        style->setProperty(CSSPropertyWebkitUserSelect, conFontStyle->getPropertyValue(CSSPropertyWebkitUserSelect), false, ec);
        ASSERT(ec == 0);
    }
    
    holder->appendChild(m_fragment, ec);
    ASSERT(ec == 0);
    
    body->appendChild(holder.get(), ec);
    ASSERT(ec == 0);
    
    m_document->updateLayoutIgnorePendingStylesheets();
    
    return holder.release();
}

void ReplacementFragment::restoreTestRenderingNodesToFragment(Node *holder)
{
    if (!holder)
        return;
    
    ExceptionCode ec = 0;
    while (RefPtr<Node> node = holder->firstChild()) {
        holder->removeChild(node.get(), ec);
        ASSERT(ec == 0);
        m_fragment->appendChild(node.get(), ec);
        ASSERT(ec == 0);
    }
}

void ReplacementFragment::removeUnrenderedNodes(Node* holder)
{
    Vector<Node*> unrendered;

    for (Node* node = holder->firstChild(); node; node = node->traverseNextNode(holder))
        if (!isNodeRendered(node) && !isTableStructureNode(node))
            unrendered.append(node);

    size_t n = unrendered.size();
    for (size_t i = 0; i < n; ++i)
        removeNode(unrendered[i]);
}

void ReplacementFragment::removeInterchangeNodes(Node* container)
{
    // Interchange newlines at the "start" of the incoming fragment must be
    // either the first node in the fragment or the first leaf in the fragment.
    Node* node = container->firstChild();
    while (node) {
        if (isInterchangeNewlineNode(node)) {
            m_hasInterchangeNewlineAtStart = true;
            removeNode(node);
            break;
        }
        node = node->firstChild();
    }
    if (!container->hasChildNodes())
        return;
    // Interchange newlines at the "end" of the incoming fragment must be
    // either the last node in the fragment or the last leaf in the fragment.
    node = container->lastChild();
    while (node) {
        if (isInterchangeNewlineNode(node)) {
            m_hasInterchangeNewlineAtEnd = true;
            removeNode(node);
            break;
        }
        node = node->lastChild();
    }
    
    node = container->firstChild();
    while (node) {
        Node *next = node->traverseNextNode();
        if (isInterchangeConvertedSpaceSpan(node)) {
            RefPtr<Node> n = 0;
            while ((n = node->firstChild())) {
                removeNode(n);
                insertNodeBefore(n, node);
            }
            removeNode(node);
            if (n)
                next = n->traverseNextNode();
        }
        node = next;
    }
}

ReplaceSelectionCommand::ReplaceSelectionCommand(Document* document, PassRefPtr<DocumentFragment> fragment,
        bool selectReplacement, bool smartReplace, bool matchStyle, bool preventNesting, bool movingParagraph,
        EditAction editAction) 
    : CompositeEditCommand(document),
      m_selectReplacement(selectReplacement), 
      m_smartReplace(smartReplace),
      m_matchStyle(matchStyle),
      m_documentFragment(fragment),
      m_preventNesting(preventNesting),
      m_movingParagraph(movingParagraph),
      m_editAction(editAction),
      m_shouldMergeEnd(false)
{
}

static bool hasMatchingQuoteLevel(VisiblePosition endOfExistingContent, VisiblePosition endOfInsertedContent)
{
    Position existing = endOfExistingContent.deepEquivalent();
    Position inserted = endOfInsertedContent.deepEquivalent();
    bool isInsideMailBlockquote = nearestMailBlockquote(inserted.node());
    return isInsideMailBlockquote && (numEnclosingMailBlockquotes(existing) == numEnclosingMailBlockquotes(inserted));
}

bool ReplaceSelectionCommand::shouldMergeStart(bool selectionStartWasStartOfParagraph, bool fragmentHasInterchangeNewlineAtStart)
{
    VisiblePosition startOfInsertedContent(positionAtStartOfInsertedContent());
    VisiblePosition prev = startOfInsertedContent.previous(true);
    if (prev.isNull())
        return false;
    
    if (!m_movingParagraph && hasMatchingQuoteLevel(prev, positionAtEndOfInsertedContent()))
        return true;

    return !selectionStartWasStartOfParagraph && 
           !fragmentHasInterchangeNewlineAtStart &&
           isStartOfParagraph(startOfInsertedContent) && 
           !startOfInsertedContent.deepEquivalent().node()->hasTagName(brTag) &&
           shouldMerge(startOfInsertedContent, prev);
}

bool ReplaceSelectionCommand::shouldMergeEnd(bool selectionEndWasEndOfParagraph)
{
    VisiblePosition endOfInsertedContent(positionAtEndOfInsertedContent());
    VisiblePosition next = endOfInsertedContent.next(true);
    if (next.isNull())
        return false;

    return !selectionEndWasEndOfParagraph &&
           isEndOfParagraph(endOfInsertedContent) && 
           !endOfInsertedContent.deepEquivalent().node()->hasTagName(brTag) &&
           shouldMerge(endOfInsertedContent, next);
}

static bool isMailPasteAsQuotationNode(const Node* node)
{
    return node && node->hasTagName(blockquoteTag) && node->isElementNode() && static_cast<const Element*>(node)->getAttribute(classAttr) == ApplePasteAsQuotation;
}

// Wrap CompositeEditCommand::removeNodePreservingChildren() so we can update the nodes we track
void ReplaceSelectionCommand::removeNodePreservingChildren(Node* node)
{
    if (m_firstNodeInserted == node)
        m_firstNodeInserted = node->traverseNextNode();
    if (m_lastLeafInserted == node)
        m_lastLeafInserted = node->lastChild() ? node->lastChild() : node->traverseNextSibling();
    CompositeEditCommand::removeNodePreservingChildren(node);
}

// Wrap CompositeEditCommand::removeNodeAndPruneAncestors() so we can update the nodes we track
void ReplaceSelectionCommand::removeNodeAndPruneAncestors(Node* node)
{
    // prepare in case m_firstNodeInserted and/or m_lastLeafInserted get removed
    // FIXME: shouldn't m_lastLeafInserted be adjusted using traversePreviousNode()?
    Node* afterFirst = m_firstNodeInserted ? m_firstNodeInserted->traverseNextSibling() : 0;
    Node* afterLast = m_lastLeafInserted ? m_lastLeafInserted->traverseNextSibling() : 0;
    
    CompositeEditCommand::removeNodeAndPruneAncestors(node);
    
    // adjust m_firstNodeInserted and m_lastLeafInserted since either or both may have been removed
    if (m_lastLeafInserted && !m_lastLeafInserted->inDocument())
        m_lastLeafInserted = afterLast;
    if (m_firstNodeInserted && !m_firstNodeInserted->inDocument())
        m_firstNodeInserted = m_lastLeafInserted && m_lastLeafInserted->inDocument() ? afterFirst : 0;
}

bool ReplaceSelectionCommand::shouldMerge(const VisiblePosition& source, const VisiblePosition& destination)
{
    if (source.isNull() || destination.isNull())
        return false;
        
    Node* sourceNode = source.deepEquivalent().node();
    Node* destinationNode = destination.deepEquivalent().node();
    Node* sourceBlock = enclosingBlock(sourceNode);
    return !enclosingNodeOfType(source.deepEquivalent(), &isMailPasteAsQuotationNode) &&
           sourceBlock && (!sourceBlock->hasTagName(blockquoteTag) || isMailBlockquote(sourceBlock))  &&
           enclosingListChild(sourceBlock) == enclosingListChild(destinationNode) &&
           enclosingTableCell(source.deepEquivalent()) == enclosingTableCell(destination.deepEquivalent()) &&
           // Don't merge to or from a position before or after a block because it would
           // be a no-op and cause infinite recursion.
           !isBlock(sourceNode) && !isBlock(destinationNode);
}

// Style rules that match just inserted elements could change their appearance, like
// a div inserted into a document with div { display:inline; }.
void ReplaceSelectionCommand::negateStyleRulesThatAffectAppearance()
{
    for (RefPtr<Node> node = m_firstNodeInserted.get(); node; node = node->traverseNextNode()) {
        // FIXME: <rdar://problem/5371536> Style rules that match pasted content can change it's appearance
        if (isStyleSpan(node.get())) {
            HTMLElement* e = static_cast<HTMLElement*>(node.get());
            // There are other styles that style rules can give to style spans,
            // but these are the two important ones because they'll prevent
            // inserted content from appearing in the right paragraph.
            // FIXME: Hyatt is concerned that selectively using display:inline will give inconsistent
            // results. We already know one issue because td elements ignore their display property
            // in quirks mode (which Mail.app is always in). We should look for an alternative.
            if (isBlock(e))
                e->getInlineStyleDecl()->setProperty(CSSPropertyDisplay, CSSValueInline);
            if (e->renderer() && e->renderer()->style()->floating() != FNONE)
                e->getInlineStyleDecl()->setProperty(CSSPropertyFloat, CSSValueNone);
        }
        if (node == m_lastLeafInserted)
            break;
    }
}

void ReplaceSelectionCommand::removeUnrenderedTextNodesAtEnds()
{
    document()->updateLayoutIgnorePendingStylesheets();
    if (!m_lastLeafInserted->renderer() && 
        m_lastLeafInserted->isTextNode() && 
        !enclosingNodeWithTag(Position(m_lastLeafInserted.get(), 0), selectTag) && 
        !enclosingNodeWithTag(Position(m_lastLeafInserted.get(), 0), scriptTag)) {
        if (m_firstNodeInserted == m_lastLeafInserted) {
            removeNode(m_lastLeafInserted.get());
            m_lastLeafInserted = 0;
            m_firstNodeInserted = 0;
            return;
        }
        RefPtr<Node> previous = m_lastLeafInserted->traversePreviousNode();
        removeNode(m_lastLeafInserted.get());
        m_lastLeafInserted = previous;
    }
    
    // We don't have to make sure that m_firstNodeInserted isn't inside a select or script element, because
    // it is a top level node in the fragment and the user can't insert into those elements.
    if (!m_firstNodeInserted->renderer() && 
        m_firstNodeInserted->isTextNode()) {
        if (m_firstNodeInserted == m_lastLeafInserted) {
            removeNode(m_firstNodeInserted.get());
            m_firstNodeInserted = 0;
            m_lastLeafInserted = 0;
            return;
        }
        RefPtr<Node> next = m_firstNodeInserted->traverseNextSibling();
        removeNode(m_firstNodeInserted.get());
        m_firstNodeInserted = next;
    }
}

void ReplaceSelectionCommand::handlePasteAsQuotationNode()
{
    Node* node = m_firstNodeInserted.get();
    if (isMailPasteAsQuotationNode(node))
        removeNodeAttribute(static_cast<Element*>(node), classAttr);
}

VisiblePosition ReplaceSelectionCommand::positionAtEndOfInsertedContent()
{
    Node* lastNode = m_lastLeafInserted.get();
    Node* enclosingSelect = enclosingNodeWithTag(Position(lastNode, 0), selectTag);
    if (enclosingSelect)
        lastNode = enclosingSelect;
    return VisiblePosition(Position(lastNode, maxDeepOffset(lastNode)));
}

VisiblePosition ReplaceSelectionCommand::positionAtStartOfInsertedContent()
{
    // Return the inserted content's first VisiblePosition.
    return VisiblePosition(nextCandidate(positionBeforeNode(m_firstNodeInserted.get())));
}

// Remove style spans before insertion if they are unnecessary.  It's faster because we'll 
// avoid doing a layout.
static bool handleStyleSpansBeforeInsertion(ReplacementFragment& fragment, const Position& insertionPos)
{
    Node* topNode = fragment.firstChild();
    
    // Handling this case is more complicated (see handleStyleSpans) and doesn't receive the optimization.
    if (isMailPasteAsQuotationNode(topNode))
        return false;
    
    // Either there are no style spans in the fragment or a WebKit client has added content to the fragment
    // before inserting it.  Look for and handle style spans after insertion.
    if (!isStyleSpan(topNode))
        return false;
    
    Node* sourceDocumentStyleSpan = topNode;
    RefPtr<Node> copiedRangeStyleSpan = sourceDocumentStyleSpan->firstChild();
    
    RefPtr<CSSMutableStyleDeclaration> styleAtInsertionPos = rangeCompliantEquivalent(insertionPos).computedStyle()->copyInheritableProperties();
    String styleText = styleAtInsertionPos->cssText();
    
    if (styleText == static_cast<Element*>(sourceDocumentStyleSpan)->getAttribute(styleAttr)) {
        fragment.removeNodePreservingChildren(sourceDocumentStyleSpan);
        if (!isStyleSpan(copiedRangeStyleSpan.get()))
            return true;
    }
        
    if (isStyleSpan(copiedRangeStyleSpan.get()) && styleText == static_cast<Element*>(copiedRangeStyleSpan.get())->getAttribute(styleAttr)) {
        fragment.removeNodePreservingChildren(copiedRangeStyleSpan.get());
        return true;
    }
    
    return false;
}

// At copy time, WebKit wraps copied content in a span that contains the source document's 
// default styles.  If the copied Range inherits any other styles from its ancestors, we put 
// those styles on a second span.
// This function removes redundant styles from those spans, and removes the spans if all their 
// styles are redundant. 
// We should remove the Apple-style-span class when we're done, see <rdar://problem/5685600>.
// We should remove styles from spans that are overridden by all of their children, either here
// or at copy time.
void ReplaceSelectionCommand::handleStyleSpans()
{
    Node* sourceDocumentStyleSpan = 0;
    Node* copiedRangeStyleSpan = 0;
    // The style span that contains the source document's default style should be at
    // the top of the fragment, but Mail sometimes adds a wrapper (for Paste As Quotation),
    // so search for the top level style span instead of assuming it's at the top.
    for (Node* node = m_firstNodeInserted.get(); node; node = node->traverseNextNode()) {
        if (isStyleSpan(node)) {
            sourceDocumentStyleSpan = node;
            // If the copied Range's common ancestor had user applied inheritable styles
            // on it, they'll be on a second style span, just below the one that holds the 
            // document defaults.
            if (isStyleSpan(node->firstChild()))
                copiedRangeStyleSpan = node->firstChild();
            break;
        }
    }
    
    // There might not be any style spans if we're pasting from another application or if 
    // we are here because of a document.execCommand("InsertHTML", ...) call.
    if (!sourceDocumentStyleSpan)
        return;
        
    RefPtr<CSSMutableStyleDeclaration> sourceDocumentStyle = static_cast<HTMLElement*>(sourceDocumentStyleSpan)->getInlineStyleDecl()->copy();
    Node* context = sourceDocumentStyleSpan->parentNode();
    
    // If Mail wraps the fragment with a Paste as Quotation blockquote, styles from that element are
    // allowed to override those from the source document, see <rdar://problem/4930986>.
    if (isMailPasteAsQuotationNode(context)) {
        RefPtr<CSSMutableStyleDeclaration> blockquoteStyle = computedStyle(context)->copyInheritableProperties();
        RefPtr<CSSMutableStyleDeclaration> parentStyle = computedStyle(context->parentNode())->copyInheritableProperties();
        parentStyle->diff(blockquoteStyle.get());

        CSSMutableStyleDeclaration::const_iterator end = blockquoteStyle->end();
        for (CSSMutableStyleDeclaration::const_iterator it = blockquoteStyle->begin(); it != end; ++it) {
            const CSSProperty& property = *it;
            sourceDocumentStyle->removeProperty(property.id());
        }        

        context = context->parentNode();
    }
    
    RefPtr<CSSMutableStyleDeclaration> contextStyle = computedStyle(context)->copyInheritableProperties();
    contextStyle->diff(sourceDocumentStyle.get());
    
    // Remove block properties in the span's style. This prevents properties that probably have no effect 
    // currently from affecting blocks later if the style is cloned for a new block element during a future 
    // editing operation.
    // FIXME: They *can* have an effect currently if blocks beneath the style span aren't individually marked
    // with block styles by the editing engine used to style them.  WebKit doesn't do this, but others might.
    sourceDocumentStyle->removeBlockProperties();
    
    // The styles on sourceDocumentStyleSpan are all redundant, and there is no copiedRangeStyleSpan
    // to consider.  We're finished.
    if (sourceDocumentStyle->length() == 0 && !copiedRangeStyleSpan) {
        removeNodePreservingChildren(sourceDocumentStyleSpan);
        return;
    }
    
    // There are non-redundant styles on sourceDocumentStyleSpan, but there is no
    // copiedRangeStyleSpan.  Clear the redundant styles from sourceDocumentStyleSpan
    // and return.
    if (sourceDocumentStyle->length() > 0 && !copiedRangeStyleSpan) {
        setNodeAttribute(static_cast<Element*>(sourceDocumentStyleSpan), styleAttr, sourceDocumentStyle->cssText());
        return;
    }
    
    RefPtr<CSSMutableStyleDeclaration> copiedRangeStyle = static_cast<HTMLElement*>(copiedRangeStyleSpan)->getInlineStyleDecl()->copy();
    
    // We're going to put sourceDocumentStyleSpan's non-redundant styles onto copiedRangeStyleSpan,
    // as long as they aren't overridden by ones on copiedRangeStyleSpan.
    sourceDocumentStyle->merge(copiedRangeStyle.get(), true);
    copiedRangeStyle = sourceDocumentStyle;
    
    removeNodePreservingChildren(sourceDocumentStyleSpan);
    
    // Remove redundant styles.
    context = copiedRangeStyleSpan->parentNode();
    contextStyle = computedStyle(context)->copyInheritableProperties();
    contextStyle->diff(copiedRangeStyle.get());
    
    // See the comments above about removing block properties.
    copiedRangeStyle->removeBlockProperties();

    // All the styles on copiedRangeStyleSpan are redundant, remove it.
    if (copiedRangeStyle->length() == 0) {
        removeNodePreservingChildren(copiedRangeStyleSpan);
        return;
    }
    
    // Clear the redundant styles from the span's style attribute.
    // FIXME: If font-family:-webkit-monospace is non-redundant, then the font-size should stay, even if it
    // appears redundant.
    setNodeAttribute(static_cast<Element*>(copiedRangeStyleSpan), styleAttr, copiedRangeStyle->cssText());
}

void ReplaceSelectionCommand::mergeEndIfNeeded()
{
    if (!m_shouldMergeEnd)
        return;

    VisiblePosition startOfInsertedContent(positionAtStartOfInsertedContent());
    VisiblePosition endOfInsertedContent(positionAtEndOfInsertedContent());
    
    // Bail to avoid infinite recursion.
    if (m_movingParagraph) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    // Merging two paragraphs will destroy the moved one's block styles.  Always move the end of inserted forward 
    // to preserve the block style of the paragraph already in the document, unless the paragraph to move would 
    // include the what was the start of the selection that was pasted into, so that we preserve that paragraph's
    // block styles.
    bool mergeForward = !(inSameParagraph(startOfInsertedContent, endOfInsertedContent) && !isStartOfParagraph(startOfInsertedContent));
    
    VisiblePosition destination = mergeForward ? endOfInsertedContent.next() : endOfInsertedContent;
    VisiblePosition startOfParagraphToMove = mergeForward ? startOfParagraph(endOfInsertedContent) : endOfInsertedContent.next();

    moveParagraph(startOfParagraphToMove, endOfParagraph(startOfParagraphToMove), destination);
    // Merging forward will remove m_lastLeafInserted from the document.
    // FIXME: Maintain positions for the start and end of inserted content instead of keeping nodes.  The nodes are
    // only ever used to create positions where inserted content starts/ends.  Also, we sometimes insert content
    // directly into text nodes already in the document, in which case tracking inserted nodes is inadequate.
    if (mergeForward) {
        m_lastLeafInserted = destination.previous().deepEquivalent().node();
        if (!m_firstNodeInserted->inDocument())
            m_firstNodeInserted = endingSelection().visibleStart().deepEquivalent().node();
    }
}

void ReplaceSelectionCommand::doApply()
{
    Selection selection = endingSelection();
    ASSERT(selection.isCaretOrRange());
    ASSERT(selection.start().node());
    if (selection.isNone() || !selection.start().node())
        return;
    
    bool selectionIsPlainText = !selection.isContentRichlyEditable();
    // In plain text only regions, we create style-less fragments, so the inserted content will automatically
    // match the style of the surrounding area and so we can avoid unnecessary work below for m_matchStyle.
    if (selectionIsPlainText)
        m_matchStyle = false;
    
    Element* currentRoot = selection.rootEditableElement();
    ReplacementFragment fragment(document(), m_documentFragment.get(), m_matchStyle, selection);
    
    if (performTrivialReplace(fragment))
        return;
    
    if (m_matchStyle)
        m_insertionStyle = styleAtPosition(selection.start());
    
    VisiblePosition visibleStart = selection.visibleStart();
    VisiblePosition visibleEnd = selection.visibleEnd();
    
    bool selectionEndWasEndOfParagraph = isEndOfParagraph(visibleEnd);
    bool selectionStartWasStartOfParagraph = isStartOfParagraph(visibleStart);
    bool selectionStartWasStartOfBlock = isStartOfBlock(visibleStart); 
    bool selectionEndWasEndOfBlock = isEndOfBlock(visibleEnd);
    
    Node* startBlock = enclosingBlock(visibleStart.deepEquivalent().node());
    
    Position insertionPos = selection.start();
    bool startIsInsideMailBlockquote = nearestMailBlockquote(insertionPos.node());
    
    bool inEmptyParagraph = selectionStartWasStartOfParagraph && selectionEndWasEndOfParagraph;
    bool inEmptyBlock = selectionStartWasStartOfBlock && selectionEndWasEndOfBlock;
    
    // FIXME: Might be more clear if we used "m_nest" instead of "m_preventNesting".
    if ((inEmptyParagraph && !inEmptyBlock && !startIsInsideMailBlockquote) ||
        startBlock == currentRoot ||
        startBlock && startBlock->renderer() && startBlock->renderer()->isListItem() ||
        // In plain text areas where newlines are preserved, we don't create fragments that will create any block nesting, 
        // so there's nothing to prevent.
        selectionIsPlainText && insertionPos.node()->renderer()->style()->preserveNewline())
        m_preventNesting = false;
    
    // When inserting into an empty block, displace that block to prevent nesting under certain circumstances.
    bool displaceStartBlockWithInsertedContent = m_preventNesting && inEmptyBlock;
    if (displaceStartBlockWithInsertedContent) {
        Node* parentOfStartBlock = startBlock->parent();
        // FIXME: We should probably do some of these same checks to determine m_preventNesting.
        if (!parentOfStartBlock ||
            startBlock->renderer()->style()->floating() != FNONE ||
            startBlock->renderer()->style()->hasBorder() ||
            startBlock->renderer()->style()->hasMargin() ||
            startBlock->renderer()->style()->hasPadding() ||
            startBlock->renderer()->style()->textAlign() != parentOfStartBlock->renderer()->style()->textAlign() ||
            startBlock->renderer()->style()->direction() != parentOfStartBlock->renderer()->style()->direction()) {
            m_preventNesting = false;
            displaceStartBlockWithInsertedContent = false;
        }
    }
    
    if (selection.isRange()) {
        // When the end of the selection being pasted into is at the end of a paragraph, and that selection
        // spans multiple blocks, not merging may leave an empty line.
        // When the start of the selection being pasted into is at the start of a block, not merging 
        // will leave hanging block(s).
        // Merge blocks if the start of the selection was in a Mail blockquote, since we handle  
        // that case specially to prevent nesting. 
        bool mergeBlocksAfterDelete = startIsInsideMailBlockquote || isEndOfParagraph(visibleEnd) || isStartOfBlock(visibleStart);
        // FIXME: We should only expand to include fully selected special elements if we are copying a 
        // selection and pasting it on top of itself.
        deleteSelection(false, mergeBlocksAfterDelete, true, false);
        visibleStart = endingSelection().visibleStart();
        if (fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfDocument(visibleStart))
                    setEndingSelection(visibleStart.next());
            } else
                insertParagraphSeparator();
        }
        insertionPos = endingSelection().start();
    } else {
        ASSERT(selection.isCaret());
        if (fragment.hasInterchangeNewlineAtStart()) {
            VisiblePosition next = visibleStart.next(true);
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart) && next.isNotNull())
                setEndingSelection(next);
            else 
                insertParagraphSeparator();
        }
        // We split the current paragraph in two to avoid nesting the blocks from the fragment inside the current block.
        // For example paste <div>foo</div><div>bar</div><div>baz</div> into <div>x^x</div>, where ^ is the caret.  
        // As long as the  div styles are the same, visually you'd expect: <div>xbar</div><div>bar</div><div>bazx</div>, 
        // not <div>xbar<div>bar</div><div>bazx</div></div>.
        // Don't do this if the selection started in a Mail blockquote.
        if (m_preventNesting && !startIsInsideMailBlockquote && !isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
            insertParagraphSeparator();
            setEndingSelection(endingSelection().visibleStart().previous());
        }
        insertionPos = endingSelection().start();
    }
    
    if (startIsInsideMailBlockquote && m_preventNesting) { 
        // We don't want any of the pasted content to end up nested in a Mail blockquote, so first break 
        // out of any surrounding Mail blockquotes. 
        applyCommandToComposite(BreakBlockquoteCommand::create(document())); 
        // This will leave a br between the split. 
        Node* br = endingSelection().start().node(); 
        ASSERT(br->hasTagName(brTag)); 
        // Insert content between the two blockquotes, but remove the br (since it was just a placeholder). 
        insertionPos = positionBeforeNode(br); 
        removeNode(br);
    }
    
    // Inserting content could cause whitespace to collapse, e.g. inserting <div>foo</div> into hello^ world.
    prepareWhitespaceAtPositionForSplit(insertionPos);
    
    // NOTE: This would be an incorrect usage of downstream() if downstream() were changed to mean the last position after 
    // p that maps to the same visible position as p (since in the case where a br is at the end of a block and collapsed 
    // away, there are positions after the br which map to the same visible position as [br, 0]).  
    Node* endBR = insertionPos.downstream().node()->hasTagName(brTag) ? insertionPos.downstream().node() : 0;
    VisiblePosition originalVisPosBeforeEndBR;
    if (endBR)
        originalVisPosBeforeEndBR = VisiblePosition(endBR, 0, DOWNSTREAM).previous();
    
    startBlock = enclosingBlock(insertionPos.node());
    
    bool insertedBeforeStartBlock = false;
    // Adjust insertionPos to prevent nesting.
    // If the start was in a Mail blockquote, we will have already handled adjusting insertionPos above.
    if (m_preventNesting && startBlock && !startIsInsideMailBlockquote) {
        ASSERT(startBlock != currentRoot);
        VisiblePosition visibleInsertionPos(insertionPos);
        if (isEndOfBlock(visibleInsertionPos) && !(isStartOfBlock(visibleInsertionPos) && fragment.hasInterchangeNewlineAtEnd()))
            insertionPos = positionAfterNode(startBlock);
        else if (isStartOfBlock(visibleInsertionPos)) {
            insertedBeforeStartBlock = true;
            insertionPos = positionBeforeNode(startBlock);
        }
    }

    // Paste into run of tabs splits the tab span.
    insertionPos = positionOutsideTabSpan(insertionPos);
    
    // Paste at start or end of link goes outside of link.
    insertionPos = positionAvoidingSpecialElementBoundary(insertionPos);
    
    // FIXME: Can this wait until after the operation has been performed?  There doesn't seem to be
    // any work performed after this that queries or uses the typing style.
    if (Frame* frame = document()->frame())
        frame->clearTypingStyle();
    
    bool handledStyleSpans = handleStyleSpansBeforeInsertion(fragment, insertionPos);
    
    // FIXME: When pasting rich content we're often prevented from heading down the fast path by style spans.  Try
    // again here if they've been removed.
    
    // We're finished if there is nothing to add.
    if (fragment.isEmpty() || !fragment.firstChild())
        return;
    
    // 1) Insert the content.
    // 2) Remove redundant styles and style tags, this inner <b> for example: <b>foo <b>bar</b> baz</b>.
    // 3) Merge the start of the added content with the content before the position being pasted into.
    // 4) Do one of the following: a) expand the last br if the fragment ends with one and it collapsed,
    // b) merge the last paragraph of the incoming fragment with the paragraph that contained the 
    // end of the selection that was pasted into, or c) handle an interchange newline at the end of the 
    // incoming fragment.
    // 5) Add spaces for smart replace.
    // 6) Select the replacement if requested, and match style if requested.
    
    VisiblePosition startOfInsertedContent, endOfInsertedContent;
    
    RefPtr<Node> refNode = fragment.firstChild();
    RefPtr<Node> node = refNode->nextSibling();
    
    fragment.removeNode(refNode);
    insertNodeAtAndUpdateNodesInserted(refNode, insertionPos);
    
    while (node) {
        Node* next = node->nextSibling();
        fragment.removeNode(node);
        insertNodeAfterAndUpdateNodesInserted(node, refNode.get());
        refNode = node;
        node = next;
    }
    
    // We don't use startBlock again so it's safe to remove here.
    if (displaceStartBlockWithInsertedContent)
        removeNode(startBlock);
    
    removeUnrenderedTextNodesAtEnds();
    
    negateStyleRulesThatAffectAppearance();
    
    if (!handledStyleSpans)
        handleStyleSpans();
    
    // Mutation events (bug 20161) may have already removed the inserted content
    if (!m_firstNodeInserted || !m_firstNodeInserted->inDocument())
        return;
    
    endOfInsertedContent = positionAtEndOfInsertedContent();
    startOfInsertedContent = positionAtStartOfInsertedContent();
    
    // We inserted before or displaced the startBlock to prevent nesting, and the content before the startBlock wasn't in its own block and
    // didn't have a br after it, so the inserted content ended up in the same paragraph.
    // FIXME: We should do this before we insert, for clarity and so that all insertions happen together for speed.
    if (insertedBeforeStartBlock || displaceStartBlockWithInsertedContent) {
        if (!isStartOfParagraph(startOfInsertedContent))
            insertNodeAt(createBreakElement(document()).get(), startOfInsertedContent.deepEquivalent());
    }
    
    Position lastPositionToSelect;
    
    bool interchangeNewlineAtEnd = fragment.hasInterchangeNewlineAtEnd();

    if (shouldRemoveEndBR(endBR, originalVisPosBeforeEndBR))
        removeNodeAndPruneAncestors(endBR);
    
    // Determine whether or not we should merge the end of inserted content with what's after it before we do
    // the start merge so that the start merge doesn't effect our decision.
    m_shouldMergeEnd = shouldMergeEnd(selectionEndWasEndOfParagraph);
    
    if (shouldMergeStart(selectionStartWasStartOfParagraph, fragment.hasInterchangeNewlineAtStart())) {
        // Bail to avoid infinite recursion.
        if (m_movingParagraph) {
            // setting display:inline does not work for td elements in quirks mode
            ASSERT(m_firstNodeInserted->hasTagName(tdTag));
            return;
        }
        VisiblePosition destination = startOfInsertedContent.previous();
        VisiblePosition startOfParagraphToMove = startOfInsertedContent;
        
        // Merging the the first paragraph of inserted content with the content that came
        // before the selection that was pasted into would also move content after 
        // the selection that was pasted into if: only one paragraph was being pasted, 
        // and it was not wrapped in a block, the selection that was pasted into ended 
        // at the end of a block and the next paragraph didn't start at the start of a block.
        // Insert a line break just after the inserted content to separate it from what 
        // comes after and prevent that from happening.
        VisiblePosition endOfInsertedContent = positionAtEndOfInsertedContent();
        if (startOfParagraph(endOfInsertedContent) == startOfParagraphToMove)
            insertNodeAt(createBreakElement(document()).get(), endOfInsertedContent.deepEquivalent());
        
        // FIXME: Maintain positions for the start and end of inserted content instead of keeping nodes.  The nodes are
        // only ever used to create positions where inserted content starts/ends.
        moveParagraph(startOfParagraphToMove, endOfParagraph(startOfParagraphToMove), destination);
        m_firstNodeInserted = endingSelection().visibleStart().deepEquivalent().downstream().node();
        if (!m_lastLeafInserted->inDocument())
            m_lastLeafInserted = endingSelection().visibleEnd().deepEquivalent().upstream().node();
    }
            
    endOfInsertedContent = positionAtEndOfInsertedContent();
    startOfInsertedContent = positionAtStartOfInsertedContent();
    
    if (interchangeNewlineAtEnd) {
        VisiblePosition next = endOfInsertedContent.next(true);

        if (selectionEndWasEndOfParagraph || !isEndOfParagraph(endOfInsertedContent) || next.isNull()) {
            if (!isStartOfParagraph(endOfInsertedContent)) {
                setEndingSelection(endOfInsertedContent);
                // Use a default paragraph element (a plain div) for the empty paragraph, using the last paragraph
                // block's style seems to annoy users.
                insertParagraphSeparator(true);

                // Select up to the paragraph separator that was added.
                lastPositionToSelect = endingSelection().visibleStart().deepEquivalent();
                updateNodesInserted(lastPositionToSelect.node());
            }
        } else {
            // Select up to the beginning of the next paragraph.
            lastPositionToSelect = next.deepEquivalent().downstream();
        }
        
    } else
        mergeEndIfNeeded();
    
    handlePasteAsQuotationNode();
    
    endOfInsertedContent = positionAtEndOfInsertedContent();
    startOfInsertedContent = positionAtStartOfInsertedContent();
    
    // Add spaces for smart replace.
    if (m_smartReplace && currentRoot) {
        // Disable smart replace for password fields.
        Node* start = currentRoot->shadowAncestorNode();
        if (start->hasTagName(inputTag) && static_cast<HTMLInputElement*>(start)->inputType() == HTMLInputElement::PASSWORD)
            m_smartReplace = false;
    }
    if (m_smartReplace) {
        bool needsTrailingSpace = !isEndOfParagraph(endOfInsertedContent) &&
                                  !isCharacterSmartReplaceExempt(endOfInsertedContent.characterAfter(), false);
        if (needsTrailingSpace) {
            RenderObject* renderer = m_lastLeafInserted->renderer();
            bool collapseWhiteSpace = !renderer || renderer->style()->collapseWhiteSpace();
            Node* endNode = positionAtEndOfInsertedContent().deepEquivalent().upstream().node();
            if (endNode->isTextNode()) {
                Text* text = static_cast<Text*>(endNode);
                insertTextIntoNode(text, text->length(), collapseWhiteSpace ? nonBreakingSpaceString() : " ");
            } else {
                RefPtr<Node> node = document()->createEditingTextNode(collapseWhiteSpace ? nonBreakingSpaceString() : " ");
                insertNodeAfterAndUpdateNodesInserted(node, endNode);
            }
        }
    
        bool needsLeadingSpace = !isStartOfParagraph(startOfInsertedContent) &&
                                 !isCharacterSmartReplaceExempt(startOfInsertedContent.previous().characterAfter(), true);
        if (needsLeadingSpace) {
            RenderObject* renderer = m_lastLeafInserted->renderer();
            bool collapseWhiteSpace = !renderer || renderer->style()->collapseWhiteSpace();
            Node* startNode = positionAtStartOfInsertedContent().deepEquivalent().downstream().node();
            if (startNode->isTextNode()) {
                Text* text = static_cast<Text*>(startNode);
                insertTextIntoNode(text, 0, collapseWhiteSpace ? nonBreakingSpaceString() : " ");
            } else {
                RefPtr<Node> node = document()->createEditingTextNode(collapseWhiteSpace ? nonBreakingSpaceString() : " ");
                // Don't updateNodesInserted.  Doing so would set m_lastLeafInserted to be the node containing the 
                // leading space, but m_lastLeafInserted is supposed to mark the end of pasted content.
                insertNodeBefore(node, startNode);
                // FIXME: Use positions to track the start/end of inserted content.
                m_firstNodeInserted = node;
            }
        }
    }
    
    completeHTMLReplacement(lastPositionToSelect);
}

bool ReplaceSelectionCommand::shouldRemoveEndBR(Node* endBR, const VisiblePosition& originalVisPosBeforeEndBR)
{
    if (!endBR || !endBR->inDocument())
        return false;
        
    VisiblePosition visiblePos(Position(endBR, 0));
    
    // Don't remove the br if nothing was inserted.
    if (visiblePos.previous() == originalVisPosBeforeEndBR)
        return false;
    
    // Remove the br if it is collapsed away and so is unnecessary.
    if (!document()->inStrictMode() && isEndOfBlock(visiblePos) && !isStartOfParagraph(visiblePos))
        return true;
        
    // A br that was originally holding a line open should be displaced by inserted content or turned into a line break.
    // A br that was originally acting as a line break should still be acting as a line break, not as a placeholder.
    return isStartOfParagraph(visiblePos) && isEndOfParagraph(visiblePos);
}

void ReplaceSelectionCommand::completeHTMLReplacement(const Position &lastPositionToSelect)
{
    Position start;
    Position end;

    // FIXME: This should never not be the case.
    if (m_firstNodeInserted && m_firstNodeInserted->inDocument() && m_lastLeafInserted && m_lastLeafInserted->inDocument()) {
        
        start = positionAtStartOfInsertedContent().deepEquivalent();
        end = positionAtEndOfInsertedContent().deepEquivalent();
        
        // FIXME (11475): Remove this and require that the creator of the fragment to use nbsps.
        rebalanceWhitespaceAt(start);
        rebalanceWhitespaceAt(end);

        if (m_matchStyle) {
            ASSERT(m_insertionStyle);
            applyStyle(m_insertionStyle.get(), start, end);
        }    
        
        if (lastPositionToSelect.isNotNull())
            end = lastPositionToSelect;
    } else if (lastPositionToSelect.isNotNull())
        start = end = lastPositionToSelect;
    else
        return;
    
    if (m_selectReplacement)
        setEndingSelection(Selection(start, end, SEL_DEFAULT_AFFINITY));
    else
        setEndingSelection(Selection(end, SEL_DEFAULT_AFFINITY));
}

EditAction ReplaceSelectionCommand::editingAction() const
{
    return m_editAction;
}

void ReplaceSelectionCommand::insertNodeAfterAndUpdateNodesInserted(PassRefPtr<Node> insertChild, Node* refChild)
{
    Node* nodeToUpdate = insertChild.get(); // insertChild will be cleared when passed
    insertNodeAfter(insertChild, refChild);
    updateNodesInserted(nodeToUpdate);
}

void ReplaceSelectionCommand::insertNodeAtAndUpdateNodesInserted(PassRefPtr<Node> insertChild, const Position& p)
{
    Node* nodeToUpdate = insertChild.get(); // insertChild will be cleared when passed
    insertNodeAt(insertChild, p);
    updateNodesInserted(nodeToUpdate);
}

void ReplaceSelectionCommand::insertNodeBeforeAndUpdateNodesInserted(PassRefPtr<Node> insertChild, Node* refChild)
{
    Node* nodeToUpdate = insertChild.get(); // insertChild will be cleared when passed
    insertNodeBefore(insertChild, refChild);
    updateNodesInserted(nodeToUpdate);
}

void ReplaceSelectionCommand::updateNodesInserted(Node *node)
{
    if (!node)
        return;

    if (!m_firstNodeInserted)
        m_firstNodeInserted = node;
    
    if (node == m_lastLeafInserted)
        return;
    
    m_lastLeafInserted = node->lastDescendant();
}

// During simple pastes, where we're just pasting a text node into a run of text, we insert the text node
// directly into the text node that holds the selection.  This is much faster than the generalized code in
// ReplaceSelectionCommand, and works around <https://bugs.webkit.org/show_bug.cgi?id=6148> since we don't 
// split text nodes.
bool ReplaceSelectionCommand::performTrivialReplace(const ReplacementFragment& fragment)
{
    if (!fragment.firstChild() || fragment.firstChild() != fragment.lastChild() || !fragment.firstChild()->isTextNode())
        return false;
        
    // FIXME: Would be nice to handle smart replace in the fast path.
    if (m_smartReplace || fragment.hasInterchangeNewlineAtStart() || fragment.hasInterchangeNewlineAtEnd())
        return false;
    
    Text* textNode = static_cast<Text*>(fragment.firstChild());
    // Our fragment creation code handles tabs, spaces, and newlines, so we don't have to worry about those here.
    String text(textNode->data());
    
    Position start = endingSelection().start();
    Position end = endingSelection().end();
    
    if (start.node() != end.node() || !start.node()->isTextNode())
        return false;
        
    replaceTextInNode(static_cast<Text*>(start.node()), start.offset(), end.offset() - start.offset(), text);
    
    end = Position(start.node(), start.offset() + text.length());
    
    Selection selectionAfterReplace(m_selectReplacement ? start : end, end);
    
    setEndingSelection(selectionAfterReplace);
    
    return true;
}

} // namespace WebCore
