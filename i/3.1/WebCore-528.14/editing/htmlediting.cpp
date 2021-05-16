/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "htmlediting.h"

#include "CharacterNames.h"
#include "Document.h"
#include "EditingText.h"
#include "HTMLBRElement.h"
#include "HTMLDivElement.h"
#include "HTMLElementFactory.h"
#include "HTMLInterchange.h"
#include "HTMLLIElement.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLUListElement.h"
#include "PositionIterator.h"
#include "RenderObject.h"
#include "Range.h"
#include "Selection.h"
#include "Text.h"
#include "TextIterator.h"
#include "VisiblePosition.h"
#include "visible_units.h"
#include <wtf/StdLibExtras.h>

#if ENABLE(WML)
#include "WMLNames.h"
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// Atomic means that the node has no children, or has children which are ignored for the
// purposes of editing.
bool isAtomicNode(const Node *node)
{
    return node && (!node->hasChildNodes() || editingIgnoresContent(node));
}

// Returns true for nodes that either have no content, or have content that is ignored (skipped 
// over) while editing.  There are no VisiblePositions inside these nodes.
bool editingIgnoresContent(const Node* node)
{
    return !canHaveChildrenForEditing(node) && !node->isTextNode();
}

bool canHaveChildrenForEditing(const Node* node)
{
    return !node->hasTagName(hrTag) &&
           !node->hasTagName(brTag) &&
           !node->hasTagName(imgTag) &&
           !node->hasTagName(buttonTag) &&
           !node->hasTagName(inputTag) &&
           !node->hasTagName(textareaTag) &&
           !node->hasTagName(objectTag) &&
           !node->hasTagName(iframeTag) &&
           !node->hasTagName(embedTag) &&
           !node->hasTagName(appletTag) &&
           !node->hasTagName(selectTag) &&
#if ENABLE(WML)
           !node->hasTagName(WMLNames::doTag) &&
#endif
           !node->isTextNode();
}

// Compare two positions, taking into account the possibility that one or both
// could be inside a shadow tree. Only works for non-null values.
int comparePositions(const Position& a, const Position& b)
{
    Node* nodeA = a.node();
    ASSERT(nodeA);
    Node* nodeB = b.node();
    ASSERT(nodeB);
    int offsetA = a.offset();
    int offsetB = b.offset();

    Node* shadowAncestorA = nodeA->shadowAncestorNode();
    if (shadowAncestorA == nodeA)
        shadowAncestorA = 0;
    Node* shadowAncestorB = nodeB->shadowAncestorNode();
    if (shadowAncestorB == nodeB)
        shadowAncestorB = 0;

    int bias = 0;
    if (shadowAncestorA != shadowAncestorB) {
        if (shadowAncestorA) {
            nodeA = shadowAncestorA;
            offsetA = 0;
            bias = 1;
        }
        if (shadowAncestorB) {
            nodeB = shadowAncestorB;
            offsetB = 0;
            bias = -1;
        }
    }

    int result = Range::compareBoundaryPoints(nodeA, offsetA, nodeB, offsetB);
    return result ? result : bias;
}

Node* highestEditableRoot(const Position& position)
{
    Node* node = position.node();
    if (!node)
        return 0;
        
    Node* highestRoot = editableRootForPosition(position);
    if (!highestRoot)
        return 0;
    
    node = highestRoot;
    while (node) {
        if (node->isContentEditable())
            highestRoot = node;
        if (node->hasTagName(bodyTag))
            break;
        node = node->parentNode();
    }
    
    return highestRoot;
}

Node* lowestEditableAncestor(Node* node)
{
    if (!node)
        return 0;
    
    Node *lowestRoot = 0;
    while (node) {
        if (node->isContentEditable())
            return node->rootEditableElement();
        if (node->hasTagName(bodyTag))
            break;
        node = node->parentNode();
    }
    
    return lowestRoot;
}

bool isEditablePosition(const Position& p)
{
    Node* node = p.node();
    if (!node)
        return false;
        
    if (node->renderer() && node->renderer()->isTable())
        node = node->parentNode();
    
    return node->isContentEditable();
}

bool isRichlyEditablePosition(const Position& p)
{
    Node* node = p.node();
    if (!node)
        return false;
        
    if (node->renderer() && node->renderer()->isTable())
        node = node->parentNode();
    
    return node->isContentRichlyEditable();
}

Element* editableRootForPosition(const Position& p)
{
    Node* node = p.node();
    if (!node)
        return 0;
        
    if (node->renderer() && node->renderer()->isTable())
        node = node->parentNode();
    
    return node->rootEditableElement();
}

bool isContentEditable(const Node* node)
{
    return node->isContentEditable();
}

Position nextCandidate(const Position& position)
{
    PositionIterator p = position;
    while (!p.atEnd()) {
        p.increment();
        if (p.isCandidate())
            return p;
    }
    return Position();
}

Position nextVisuallyDistinctCandidate(const Position& position)
{
    Position p = position;
    Position downstreamStart = p.downstream();
    while (!p.atEnd()) {
        p = p.next(UsingComposedCharacters);
        if (p.isCandidate() && p.downstream() != downstreamStart)
            return p;
    }
    return Position();
}

Position previousCandidate(const Position& position)
{
    PositionIterator p = position;
    while (!p.atStart()) {
        p.decrement();
        if (p.isCandidate())
            return p;
    }
    return Position();
}

Position previousVisuallyDistinctCandidate(const Position& position)
{
    Position p = position;
    Position downstreamStart = p.downstream();
    while (!p.atStart()) {
        p = p.previous(UsingComposedCharacters);
        if (p.isCandidate() && p.downstream() != downstreamStart)
            return p;
    }
    return Position();
}

VisiblePosition firstEditablePositionAfterPositionInRoot(const Position& position, Node* highestRoot)
{
    // position falls before highestRoot.
    if (comparePositions(position, Position(highestRoot, 0)) == -1 && highestRoot->isContentEditable())
        return VisiblePosition(Position(highestRoot, 0));
        
    Position p = position;
    
    if (Node* shadowAncestor = p.node()->shadowAncestorNode())
        if (shadowAncestor != p.node())
            p = Position(shadowAncestor, maxDeepOffset(shadowAncestor));
    
    while (p.node() && !isEditablePosition(p) && p.node()->isDescendantOf(highestRoot))
        p = isAtomicNode(p.node()) ? positionAfterNode(p.node()) : nextVisuallyDistinctCandidate(p);
    
    if (p.node() && !p.node()->isDescendantOf(highestRoot))
        return VisiblePosition();
    
    return VisiblePosition(p);
}

VisiblePosition lastEditablePositionBeforePositionInRoot(const Position& position, Node* highestRoot)
{
    // When position falls after highestRoot, the result is easy to compute.
    if (comparePositions(position, Position(highestRoot, maxDeepOffset(highestRoot))) == 1)
        return VisiblePosition(Position(highestRoot, maxDeepOffset(highestRoot)));
        
    Position p = position;
    
    if (Node* shadowAncestor = p.node()->shadowAncestorNode())
        if (shadowAncestor != p.node())
            p = Position(shadowAncestor, 0);
    
    while (p.node() && !isEditablePosition(p) && p.node()->isDescendantOf(highestRoot))
        p = isAtomicNode(p.node()) ? positionBeforeNode(p.node()) : previousVisuallyDistinctCandidate(p);
    
    if (p.node() && !p.node()->isDescendantOf(highestRoot))
        return VisiblePosition();
    
    return VisiblePosition(p);
}

// Whether or not content before and after this node will collapse onto the same line as it.
bool isBlock(const Node* node)
{
    return node && node->renderer() && !node->renderer()->isInline();
}

// FIXME: Deploy this in all of the places where enclosingBlockFlow/enclosingBlockFlowOrTableElement are used.
// FIXME: Pass a position to this function.  The enclosing block of [table, x] for example, should be the 
// block that contains the table and not the table, and this function should be the only one responsible for 
// knowing about these kinds of special cases.
Node* enclosingBlock(Node* node)
{
    return static_cast<Element*>(enclosingNodeOfType(Position(node, 0), isBlock));
}

Position rangeCompliantEquivalent(const Position& pos)
{
    if (pos.isNull())
        return Position();

    Node *node = pos.node();
    
    if (pos.offset() <= 0) {
        if (node->parentNode() && (editingIgnoresContent(node) || isTableElement(node)))
            return positionBeforeNode(node);
        return Position(node, 0);
    }
    
    if (node->offsetInCharacters())
        return Position(node, min(node->maxCharacterOffset(), pos.offset()));
    
    int maxCompliantOffset = node->childNodeCount();
    if (pos.offset() > maxCompliantOffset) {
        if (node->parentNode())
            return positionAfterNode(node);
        
        // there is no other option at this point than to
        // use the highest allowed position in the node
        return Position(node, maxCompliantOffset);
    } 

    // Editing should never generate positions like this.
    if ((pos.offset() < maxCompliantOffset) && editingIgnoresContent(node)) {
        ASSERT_NOT_REACHED();
        return node->parentNode() ? positionBeforeNode(node) : Position(node, 0);
    }
    
    if (pos.offset() == maxCompliantOffset && (editingIgnoresContent(node) || isTableElement(node)))
        return positionAfterNode(node);
    
    return Position(pos);
}

Position rangeCompliantEquivalent(const VisiblePosition& vpos)
{
    return rangeCompliantEquivalent(vpos.deepEquivalent());
}

// This method is used to create positions in the DOM. It returns the maximum valid offset
// in a node.  It returns 1 for some elements even though they do not have children, which
// creates technically invalid DOM Positions.  Be sure to call rangeCompliantEquivalent
// on a Position before using it to create a DOM Range, or an exception will be thrown.
int maxDeepOffset(const Node *node)
{
    ASSERT(node);
    if (!node)
        return 0;
    if (node->offsetInCharacters())
        return node->maxCharacterOffset();
        
    if (node->hasChildNodes())
        return node->childNodeCount();
    
    // NOTE: This should preempt the childNodeCount for, e.g., select nodes
    if (editingIgnoresContent(node))
        return 1;

    return 0;
}

String stringWithRebalancedWhitespace(const String& string, bool startIsStartOfParagraph, bool endIsEndOfParagraph)
{
    DEFINE_STATIC_LOCAL(String, twoSpaces, ("  "));
    DEFINE_STATIC_LOCAL(String, nbsp, ("\xa0"));
    DEFINE_STATIC_LOCAL(String, pattern, (" \xa0"));

    String rebalancedString = string;

    rebalancedString.replace(noBreakSpace, ' ');
    rebalancedString.replace('\n', ' ');
    rebalancedString.replace('\t', ' ');
    
    rebalancedString.replace(twoSpaces, pattern);
    
    if (startIsStartOfParagraph && rebalancedString[0] == ' ')
        rebalancedString.replace(0, 1, nbsp);
    int end = rebalancedString.length() - 1;
    if (endIsEndOfParagraph && rebalancedString[end] == ' ')
        rebalancedString.replace(end, 1, nbsp);    

    return rebalancedString;
}

bool isTableStructureNode(const Node *node)
{
    RenderObject *r = node->renderer();
    return (r && (r->isTableCell() || r->isTableRow() || r->isTableSection() || r->isTableCol()));
}

const String& nonBreakingSpaceString()
{
    DEFINE_STATIC_LOCAL(String, nonBreakingSpaceString, (&noBreakSpace, 1));
    return nonBreakingSpaceString;
}

// FIXME: need to dump this
bool isSpecialElement(const Node *n)
{
    if (!n)
        return false;
        
    if (!n->isHTMLElement())
        return false;

    if (n->isLink())
        return true;

    RenderObject *renderer = n->renderer();
    if (!renderer)
        return false;
        
    if (renderer->style()->display() == TABLE || renderer->style()->display() == INLINE_TABLE)
        return true;

    if (renderer->style()->isFloating())
        return true;

    if (renderer->style()->position() != StaticPosition)
        return true;
        
    return false;
}

// Checks if a string is a valid tag for the FormatBlockCommand function of execCommand. Expects lower case strings.
bool validBlockTag(const String& blockTag)
{
    if (blockTag == "address" ||
        blockTag == "blockquote" ||
        blockTag == "dd" ||
        blockTag == "div" ||
        blockTag == "dl" ||
        blockTag == "dt" ||
        blockTag == "h1" ||
        blockTag == "h2" ||
        blockTag == "h3" ||
        blockTag == "h4" ||
        blockTag == "h5" ||
        blockTag == "h6" ||
        blockTag == "p" ||
        blockTag == "pre")
        return true;
    return false;
}

static Node* firstInSpecialElement(const Position& pos)
{
    Node* rootEditableElement = pos.node()->rootEditableElement();
    for (Node* n = pos.node(); n && n->rootEditableElement() == rootEditableElement; n = n->parentNode())
        if (isSpecialElement(n)) {
            VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);
            VisiblePosition firstInElement = VisiblePosition(n, 0, DOWNSTREAM);
            if (isTableElement(n) && vPos == firstInElement.next())
                return n;
            if (vPos == firstInElement)
                return n;
        }
    return 0;
}

static Node* lastInSpecialElement(const Position& pos)
{
    Node* rootEditableElement = pos.node()->rootEditableElement();
    for (Node* n = pos.node(); n && n->rootEditableElement() == rootEditableElement; n = n->parentNode())
        if (isSpecialElement(n)) {
            VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);
            VisiblePosition lastInElement = VisiblePosition(n, n->childNodeCount(), DOWNSTREAM);
            if (isTableElement(n) && vPos == lastInElement.previous())
                return n;
            if (vPos == lastInElement)
                return n;
        }
    return 0;
}

bool isFirstVisiblePositionInSpecialElement(const Position& pos)
{
    return firstInSpecialElement(pos);
}

Position positionBeforeContainingSpecialElement(const Position& pos, Node** containingSpecialElement)
{
    Node* n = firstInSpecialElement(pos);
    if (!n)
        return pos;
    Position result = positionBeforeNode(n);
    if (result.isNull() || result.node()->rootEditableElement() != pos.node()->rootEditableElement())
        return pos;
    if (containingSpecialElement)
        *containingSpecialElement = n;
    return result;
}

bool isLastVisiblePositionInSpecialElement(const Position& pos)
{
    return lastInSpecialElement(pos);
}

Position positionAfterContainingSpecialElement(const Position& pos, Node **containingSpecialElement)
{
    Node* n = lastInSpecialElement(pos);
    if (!n)
        return pos;
    Position result = positionAfterNode(n);
    if (result.isNull() || result.node()->rootEditableElement() != pos.node()->rootEditableElement())
        return pos;
    if (containingSpecialElement)
        *containingSpecialElement = n;
    return result;
}

Position positionOutsideContainingSpecialElement(const Position &pos, Node **containingSpecialElement)
{
    if (isFirstVisiblePositionInSpecialElement(pos))
        return positionBeforeContainingSpecialElement(pos, containingSpecialElement);
    if (isLastVisiblePositionInSpecialElement(pos))
        return positionAfterContainingSpecialElement(pos, containingSpecialElement);
    return pos;
}

Node* isFirstPositionAfterTable(const VisiblePosition& visiblePosition)
{
    Position upstream(visiblePosition.deepEquivalent().upstream());
    if (upstream.node() && upstream.node()->renderer() && upstream.node()->renderer()->isTable() && upstream.offset() == maxDeepOffset(upstream.node()))
        return upstream.node();
    
    return 0;
}

Node* isLastPositionBeforeTable(const VisiblePosition& visiblePosition)
{
    Position downstream(visiblePosition.deepEquivalent().downstream());
    if (downstream.node() && downstream.node()->renderer() && downstream.node()->renderer()->isTable() && downstream.offset() == 0)
        return downstream.node();
    
    return 0;
}

Position positionBeforeNode(const Node *node)
{
    return Position(node->parentNode(), node->nodeIndex());
}

Position positionAfterNode(const Node *node)
{
    return Position(node->parentNode(), node->nodeIndex() + 1);
}

bool isListElement(Node *n)
{
    return (n && (n->hasTagName(ulTag) || n->hasTagName(olTag) || n->hasTagName(dlTag)));
}

Node* enclosingNodeWithTag(const Position& p, const QualifiedName& tagName)
{
    if (p.isNull())
        return 0;
        
    Node* root = highestEditableRoot(p);
    for (Node* n = p.node(); n; n = n->parentNode()) {
        if (root && !isContentEditable(n))
            continue;
        if (n->hasTagName(tagName))
            return n;
        if (n == root)
            return 0;
    }
    
    return 0;
}

Node* enclosingNodeOfType(const Position& p, bool (*nodeIsOfType)(const Node*), bool onlyReturnEditableNodes)
{
    if (p.isNull())
        return 0;
        
    Node* root = highestEditableRoot(p);
    for (Node* n = p.node(); n; n = n->parentNode()) {
        // Don't return a non-editable node if the input position was editable, since
        // the callers from editing will no doubt want to perform editing inside the returned node.
        if (root && !isContentEditable(n) && onlyReturnEditableNodes)
            continue;
        if ((*nodeIsOfType)(n))
            return n;
        if (n == root)
            return 0;
    }
    
    return 0;
}

Node* highestEnclosingNodeOfType(const Position& p, bool (*nodeIsOfType)(const Node*))
{
    Node* highest = 0;
    Node* root = highestEditableRoot(p);
    for (Node* n = p.node(); n; n = n->parentNode()) {
        if ((*nodeIsOfType)(n))
            highest = n;
        if (n == root)
            break;
    }
    
    return highest;
}

Node* enclosingTableCell(const Position& p)
{
    return static_cast<Element*>(enclosingNodeOfType(p, isTableCell));
}

Node* enclosingAnchorElement(const Position& p)
{
    if (p.isNull())
        return 0;
    
    Node* node = p.node();
    while (node && !(node->isElementNode() && node->isLink()))
        node = node->parentNode();
    return node;
}

HTMLElement* enclosingList(Node* node)
{
    if (!node)
        return 0;
        
    Node* root = highestEditableRoot(Position(node, 0));
    
    for (Node* n = node->parentNode(); n; n = n->parentNode()) {
        if (n->hasTagName(ulTag) || n->hasTagName(olTag))
            return static_cast<HTMLElement*>(n);
        if (n == root)
            return 0;
    }
    
    return 0;
}

Node* enclosingListChild(Node *node)
{
    if (!node)
        return 0;
    // Check for a list item element, or for a node whose parent is a list element.  Such a node
    // will appear visually as a list item (but without a list marker)
    Node* root = highestEditableRoot(Position(node, 0));
    
    // FIXME: This function is inappropriately named if it starts with node instead of node->parentNode()
    for (Node* n = node; n && n->parentNode(); n = n->parentNode()) {
        if (n->hasTagName(liTag) || isListElement(n->parentNode()))
            return n;
        if (n == root || isTableCell(n))
            return 0;
    }
    
    return 0;
}

static HTMLElement* embeddedSublist(Node* listItem)
{
    // Check the DOM so that we'll find collapsed sublists without renderers.
    for (Node* n = listItem->firstChild(); n; n = n->nextSibling()) {
        if (isListElement(n))
            return static_cast<HTMLElement*>(n);
    }
    
    return 0;
}

static Node* appendedSublist(Node* listItem)
{
    // Check the DOM so that we'll find collapsed sublists without renderers.
    for (Node* n = listItem->nextSibling(); n; n = n->nextSibling()) {
        if (isListElement(n))
            return static_cast<HTMLElement*>(n);
        if (n->renderer() && n->renderer()->isListItem())
            return 0;
    }
    
    return 0;
}

Node* enclosingEmptyListItem(const VisiblePosition& visiblePos)
{
    // Check that position is on a line by itself inside a list item
    Node* listChildNode = enclosingListChild(visiblePos.deepEquivalent().node());
    if (!listChildNode || !isStartOfParagraph(visiblePos) || !isEndOfParagraph(visiblePos))
        return 0;
        
    VisiblePosition firstInListChild(Position(listChildNode, 0));
    VisiblePosition lastInListChild(Position(listChildNode, maxDeepOffset(listChildNode)));
    
    if (firstInListChild != visiblePos || lastInListChild != visiblePos)
        return 0;
    
    if (embeddedSublist(listChildNode) || appendedSublist(listChildNode))
        return 0;
        
    return listChildNode;
}

HTMLElement* outermostEnclosingList(Node* node)
{
    HTMLElement* list = enclosingList(node);
    if (!list)
        return 0;
    while (HTMLElement* nextList = enclosingList(list))
        list = nextList;
    return list;
}

Node* highestAncestor(Node* node)
{
    ASSERT(node);
    Node* parent = node;
    while ((node = node->parentNode()))
        parent = node;
    return parent;
}

// FIXME: do not require renderer, so that this can be used within fragments, or rename to isRenderedTable()
bool isTableElement(Node* n)
{
    if (!n || !n->isElementNode())
        return false;

    RenderObject* renderer = n->renderer();
    return (renderer && (renderer->style()->display() == TABLE || renderer->style()->display() == INLINE_TABLE));
}

bool isTableCell(const Node* node)
{
    RenderObject* r = node->renderer();
    if (!r)
        return node->hasTagName(tdTag) || node->hasTagName(thTag);
    
    return r->isTableCell();
}

PassRefPtr<HTMLElement> createDefaultParagraphElement(Document* document)
{
    return new HTMLDivElement(divTag, document);
}

PassRefPtr<HTMLElement> createBreakElement(Document* document)
{
    return new HTMLBRElement(brTag, document);
}

PassRefPtr<HTMLElement> createOrderedListElement(Document* document)
{
    return new HTMLOListElement(olTag, document);
}

PassRefPtr<HTMLElement> createUnorderedListElement(Document* document)
{
    return new HTMLUListElement(ulTag, document);
}

PassRefPtr<HTMLElement> createListItemElement(Document* document)
{
    return new HTMLLIElement(liTag, document);
}

PassRefPtr<HTMLElement> createHTMLElement(Document* document, const QualifiedName& name)
{
    return HTMLElementFactory::createHTMLElement(name, document, 0, false);
}

PassRefPtr<HTMLElement> createHTMLElement(Document* document, const AtomicString& tagName)
{
    return createHTMLElement(document, QualifiedName(nullAtom, tagName, xhtmlNamespaceURI));
}

bool isTabSpanNode(const Node *node)
{
    return node && node->hasTagName(spanTag) && node->isElementNode() && static_cast<const Element *>(node)->getAttribute(classAttr) == AppleTabSpanClass;
}

bool isTabSpanTextNode(const Node *node)
{
    return node && node->isTextNode() && node->parentNode() && isTabSpanNode(node->parentNode());
}

Node *tabSpanNode(const Node *node)
{
    return isTabSpanTextNode(node) ? node->parentNode() : 0;
}

Position positionBeforeTabSpan(const Position& pos)
{
    Node *node = pos.node();
    if (isTabSpanTextNode(node))
        node = tabSpanNode(node);
    else if (!isTabSpanNode(node))
        return pos;
    
    return positionBeforeNode(node);
}

PassRefPtr<Element> createTabSpanElement(Document* document, PassRefPtr<Node> tabTextNode)
{
    // make the span to hold the tab
    ExceptionCode ec = 0;
    RefPtr<Element> spanElement = document->createElementNS(xhtmlNamespaceURI, "span", ec);
    ASSERT(ec == 0);
    spanElement->setAttribute(classAttr, AppleTabSpanClass);
    spanElement->setAttribute(styleAttr, "white-space:pre");

    // add tab text to that span
    if (!tabTextNode)
        tabTextNode = document->createEditingTextNode("\t");
    spanElement->appendChild(tabTextNode, ec);
    ASSERT(ec == 0);

    return spanElement.release();
}

PassRefPtr<Element> createTabSpanElement(Document* document, const String& tabText)
{
    return createTabSpanElement(document, document->createTextNode(tabText));
}

PassRefPtr<Element> createTabSpanElement(Document* document)
{
    return createTabSpanElement(document, PassRefPtr<Node>());
}

bool isNodeRendered(const Node *node)
{
    if (!node)
        return false;

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return false;

    return renderer->style()->visibility() == VISIBLE;
}

Node *nearestMailBlockquote(const Node *node)
{
    for (Node *n = const_cast<Node *>(node); n; n = n->parentNode()) {
        if (isMailBlockquote(n))
            return n;
    }
    return 0;
}

unsigned numEnclosingMailBlockquotes(const Position& p)
{
    unsigned num = 0;
    for (Node* n = p.node(); n; n = n->parentNode())
        if (isMailBlockquote(n))
            num++;
    
    return num;
}

bool isMailBlockquote(const Node *node)
{
    if (!node || !node->isElementNode() && !node->hasTagName(blockquoteTag))
        return false;
        
    return static_cast<const Element *>(node)->getAttribute("type") == "cite";
}

int caretMinOffset(const Node* n)
{
    RenderObject* r = n->renderer();
    ASSERT(!n->isCharacterDataNode() || !r || r->isText()); // FIXME: This was a runtime check that seemingly couldn't fail; changed it to an assertion for now.
    return r ? r->caretMinOffset() : 0;
}

// If a node can contain candidates for VisiblePositions, return the offset of the last candidate, otherwise 
// return the number of children for container nodes and the length for unrendered text nodes.
int caretMaxOffset(const Node* n)
{
    // For rendered text nodes, return the last position that a caret could occupy.
    if (n->isTextNode() && n->renderer())
        return n->renderer()->caretMaxOffset();
    // For containers return the number of children.  For others do the same as above.
    return maxDeepOffset(n);
}

bool lineBreakExistsAtVisiblePosition(const VisiblePosition& visiblePosition)
{
    return lineBreakExistsAtPosition(visiblePosition.deepEquivalent().downstream());
}

bool lineBreakExistsAtPosition(const Position& position)
{
    if (position.isNull())
        return false;
    
    if (position.node()->hasTagName(brTag) && position.offset() == 0)
        return true;
    
    if (!position.node()->isTextNode() || !position.node()->renderer()->style()->preserveNewline())
        return false;
    
    Text* textNode = static_cast<Text*>(position.node());
    unsigned offset = position.offset();
    return offset < textNode->length() && textNode->data()[offset] == '\n';
}

// Modifies selections that have an end point at the edge of a table
// that contains the other endpoint so that they don't confuse
// code that iterates over selected paragraphs.
Selection selectionForParagraphIteration(const Selection& original)
{
    Selection newSelection(original);
    VisiblePosition startOfSelection(newSelection.visibleStart());
    VisiblePosition endOfSelection(newSelection.visibleEnd());
    
    // If the end of the selection to modify is just after a table, and
    // if the start of the selection is inside that table, then the last paragraph
    // that we'll want modify is the last one inside the table, not the table itself
    // (a table is itself a paragraph).
    if (Node* table = isFirstPositionAfterTable(endOfSelection))
        if (startOfSelection.deepEquivalent().node()->isDescendantOf(table))
            newSelection = Selection(startOfSelection, endOfSelection.previous(true));
    
    // If the start of the selection to modify is just before a table,
    // and if the end of the selection is inside that table, then the first paragraph
    // we'll want to modify is the first one inside the table, not the paragraph
    // containing the table itself.
    if (Node* table = isLastPositionBeforeTable(startOfSelection))
        if (endOfSelection.deepEquivalent().node()->isDescendantOf(table))
            newSelection = Selection(startOfSelection.next(true), endOfSelection);
    
    return newSelection;
}


int indexForVisiblePosition(VisiblePosition& visiblePosition)
{
    if (visiblePosition.isNull())
        return 0;
    Position p(visiblePosition.deepEquivalent());
    RefPtr<Range> range = Range::create(p.node()->document(), Position(p.node()->document(), 0), rangeCompliantEquivalent(p));
    return TextIterator::rangeLength(range.get(), true);
}

PassRefPtr<Range> avoidIntersectionWithNode(const Range* range, Node* node)
{
    if (!range)
        return 0;

    Document* document = range->ownerDocument();

    Node* startContainer = range->startContainer();
    int startOffset = range->startOffset();
    Node* endContainer = range->endContainer();
    int endOffset = range->endOffset();

    if (!startContainer)
        return 0;

    ASSERT(endContainer);

    if (startContainer == node || startContainer->isDescendantOf(node)) {
        ASSERT(node->parentNode());
        startContainer = node->parentNode();
        startOffset = node->nodeIndex();
    }
    if (endContainer == node || endContainer->isDescendantOf(node)) {
        ASSERT(node->parentNode());
        endContainer = node->parentNode();
        endOffset = node->nodeIndex();
    }

    return Range::create(document, startContainer, startOffset, endContainer, endOffset);
}

Selection avoidIntersectionWithNode(const Selection& selection, Node* node)
{
    if (selection.isNone())
        return Selection(selection);
        
    Selection updatedSelection(selection);
    Node* base = selection.base().node();
    Node* extent = selection.extent().node();
    ASSERT(base);
    ASSERT(extent);
    
    if (base == node || base->isDescendantOf(node)) {
        ASSERT(node->parentNode());
        updatedSelection.setBase(Position(node->parentNode(), node->nodeIndex()));
    }
    
    if (extent == node || extent->isDescendantOf(node)) {
        ASSERT(node->parentNode());
        updatedSelection.setExtent(Position(node->parentNode(), node->nodeIndex()));
    }
        
    return updatedSelection;
}

} // namespace WebCore
