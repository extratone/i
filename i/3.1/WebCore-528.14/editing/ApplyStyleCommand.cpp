/*
 * Copyright (C) 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
#include "ApplyStyleCommand.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "HTMLElement.h"
#include "HTMLInterchange.h"
#include "HTMLNames.h"
#include "NodeList.h"
#include "Range.h"
#include "RenderObject.h"
#include "Text.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

class StyleChange {
public:
    enum ELegacyHTMLStyles { DoNotUseLegacyHTMLStyles, UseLegacyHTMLStyles };

    explicit StyleChange(CSSStyleDeclaration*, ELegacyHTMLStyles usesLegacyStyles=UseLegacyHTMLStyles);
    StyleChange(CSSStyleDeclaration*, const Position&, ELegacyHTMLStyles usesLegacyStyles=UseLegacyHTMLStyles);

    static ELegacyHTMLStyles styleModeForParseMode(bool);

    String cssStyle() const { return m_cssStyle; }
    bool applyBold() const { return m_applyBold; }
    bool applyItalic() const { return m_applyItalic; }
    bool applySubscript() const { return m_applySubscript; }
    bool applySuperscript() const { return m_applySuperscript; }
    bool applyFontColor() const { return m_applyFontColor.length() > 0; }
    bool applyFontFace() const { return m_applyFontFace.length() > 0; }
    bool applyFontSize() const { return m_applyFontSize.length() > 0; }

    String fontColor() { return m_applyFontColor; }
    String fontFace() { return m_applyFontFace; }
    String fontSize() { return m_applyFontSize; }

    bool usesLegacyStyles() const { return m_usesLegacyStyles; }

private:
    void init(PassRefPtr<CSSStyleDeclaration>, const Position&);
    bool checkForLegacyHTMLStyleChange(const CSSProperty*);
    static bool currentlyHasStyle(const Position&, const CSSProperty*);
    
    String m_cssStyle;
    bool m_applyBold;
    bool m_applyItalic;
    bool m_applySubscript;
    bool m_applySuperscript;
    String m_applyFontColor;
    String m_applyFontFace;
    String m_applyFontSize;
    bool m_usesLegacyStyles;
};



StyleChange::StyleChange(CSSStyleDeclaration* style, ELegacyHTMLStyles usesLegacyStyles)
    : m_applyBold(false)
    , m_applyItalic(false)
    , m_applySubscript(false)
    , m_applySuperscript(false)
    , m_usesLegacyStyles(usesLegacyStyles)
{
    init(style, Position());
}

StyleChange::StyleChange(CSSStyleDeclaration* style, const Position& position, ELegacyHTMLStyles usesLegacyStyles)
    : m_applyBold(false)
    , m_applyItalic(false)
    , m_applySubscript(false)
    , m_applySuperscript(false)
    , m_usesLegacyStyles(usesLegacyStyles)
{
    init(style, position);
}

void StyleChange::init(PassRefPtr<CSSStyleDeclaration> style, const Position &position)
{
    RefPtr<CSSMutableStyleDeclaration> mutableStyle = style->makeMutable();
    
    String styleText("");

    bool addedDirection = false;
    CSSMutableStyleDeclaration::const_iterator end = mutableStyle->end();
    for (CSSMutableStyleDeclaration::const_iterator it = mutableStyle->begin(); it != end; ++it) {
        const CSSProperty *property = &*it;

        // If position is empty or the position passed in already has the 
        // style, just move on.
        if (position.isNotNull() && currentlyHasStyle(position, property))
            continue;
        
        // Changing the whitespace style in a tab span would collapse the tab into a space.
        if (property->id() == CSSPropertyWhiteSpace && (isTabSpanTextNode(position.node()) || isTabSpanNode((position.node()))))
            continue;
        
        // If needed, figure out if this change is a legacy HTML style change.
        if (m_usesLegacyStyles && checkForLegacyHTMLStyleChange(property))
            continue;

        if (property->id() == CSSPropertyDirection) {
            if (addedDirection)
                continue;
            addedDirection = true;
        }

        // Add this property

        if (property->id() == CSSPropertyWebkitTextDecorationsInEffect) {
            // we have to special-case text decorations
            // FIXME: Why?
            CSSProperty alteredProperty(CSSPropertyTextDecoration, property->value(), property->isImportant());
            styleText += alteredProperty.cssText();
        } else
            styleText += property->cssText();

        if (!addedDirection && property->id() == CSSPropertyUnicodeBidi) {
            styleText += "direction: " + style->getPropertyValue(CSSPropertyDirection) + "; ";
            addedDirection = true;
        }
    }

    // Save the result for later
    m_cssStyle = styleText.stripWhiteSpace();
}

StyleChange::ELegacyHTMLStyles StyleChange::styleModeForParseMode(bool isQuirksMode)
{
    return isQuirksMode ? UseLegacyHTMLStyles : DoNotUseLegacyHTMLStyles;
}

// This function is the mapping from CSS styles to styling tags (like font-weight: bold to <b>)
bool StyleChange::checkForLegacyHTMLStyleChange(const CSSProperty* property)
{
    if (!property || !property->value())
        return false;
    
    String valueText(property->value()->cssText());
    switch (property->id()) {
        case CSSPropertyFontWeight:
            if (equalIgnoringCase(valueText, "bold")) {
                m_applyBold = true;
                return true;
            }
            break;
        case CSSPropertyVerticalAlign:
            if (equalIgnoringCase(valueText, "sub")) {
                m_applySubscript = true;
                return true;
            }
            if (equalIgnoringCase(valueText, "super")) {
                m_applySuperscript = true;
                return true;
            }
            break;
        case CSSPropertyFontStyle:
            if (equalIgnoringCase(valueText, "italic") || equalIgnoringCase(valueText, "oblique")) {
                m_applyItalic = true;
                return true;
            }
            break;
        case CSSPropertyColor: {
            RGBA32 rgba = 0;
            CSSParser::parseColor(rgba, valueText);
            Color color(rgba);
            m_applyFontColor = color.name();
            return true;
        }
        case CSSPropertyFontFamily:
            m_applyFontFace = valueText;
            return true;
        case CSSPropertyFontSize:
            if (property->value()->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE) {
                CSSPrimitiveValue *value = static_cast<CSSPrimitiveValue *>(property->value());

                if (value->primitiveType() < CSSPrimitiveValue::CSS_PX || value->primitiveType() > CSSPrimitiveValue::CSS_PC)
                    // Size keyword or relative unit.
                    return false;

                float number = value->getFloatValue(CSSPrimitiveValue::CSS_PX);
                if (number <= 9)
                    m_applyFontSize = "1";
                else if (number <= 10)
                    m_applyFontSize = "2";
                else if (number <= 13)
                    m_applyFontSize = "3";
                else if (number <= 16)
                    m_applyFontSize = "4";
                else if (number <= 18)
                    m_applyFontSize = "5";
                else if (number <= 24)
                    m_applyFontSize = "6";
                else
                    m_applyFontSize = "7";
                // Huge quirk in Microsft Entourage is that they understand CSS font-size, but also write 
                // out legacy 1-7 values in font tags (I guess for mailers that are not CSS-savvy at all, 
                // like Eudora). Yes, they write out *both*. We need to write out both as well. Return false.
                return false; 
            }
            else {
                // Can't make sense of the number. Put no font size.
                return true;
            }
    }
    return false;
}

bool StyleChange::currentlyHasStyle(const Position &pos, const CSSProperty *property)
{
    ASSERT(pos.isNotNull());
    RefPtr<CSSComputedStyleDeclaration> style = pos.computedStyle();
    RefPtr<CSSValue> value = style->getPropertyCSSValue(property->id(), DoNotUpdateLayout);
    if (!value)
        return false;
    return equalIgnoringCase(value->cssText(), property->value()->cssText());
}

static String& styleSpanClassString()
{
    DEFINE_STATIC_LOCAL(String, styleSpanClassString, ((AppleStyleSpanClass)));
    return styleSpanClassString;
}

bool isStyleSpan(const Node *node)
{
    if (!node || !node->isHTMLElement())
        return false;

    const HTMLElement* elem = static_cast<const HTMLElement*>(node);
    return elem->hasLocalName(spanAttr) && elem->getAttribute(classAttr) == styleSpanClassString();
}

static bool isUnstyledStyleSpan(const Node* node)
{
    if (!node || !node->isHTMLElement() || !node->hasTagName(spanTag))
        return false;

    const HTMLElement* elem = static_cast<const HTMLElement*>(node);
    CSSMutableStyleDeclaration* inlineStyleDecl = elem->inlineStyleDecl();
    return (!inlineStyleDecl || inlineStyleDecl->length() == 0) && elem->getAttribute(classAttr) == styleSpanClassString();
}

static bool isSpanWithoutAttributesOrUnstyleStyleSpan(const Node* node)
{
    if (!node || !node->isHTMLElement() || !node->hasTagName(spanTag))
        return false;

    const HTMLElement* elem = static_cast<const HTMLElement*>(node);
    NamedAttrMap* attributes = elem->attributes(true); // readonly
    if (attributes->length() == 0)
        return true;

    return isUnstyledStyleSpan(node);
}

static bool isEmptyFontTag(const Node *node)
{
    if (!node || !node->hasTagName(fontTag))
        return false;

    const Element *elem = static_cast<const Element *>(node);
    NamedAttrMap *map = elem->attributes(true); // true for read-only
    return (!map || map->length() == 1) && elem->getAttribute(classAttr) == styleSpanClassString();
}

static PassRefPtr<Element> createFontElement(Document* document)
{
    RefPtr<Element> fontNode = createHTMLElement(document, fontTag);
    fontNode->setAttribute(classAttr, styleSpanClassString());
    return fontNode.release();
}

PassRefPtr<HTMLElement> createStyleSpanElement(Document* document)
{
    RefPtr<HTMLElement> styleElement = createHTMLElement(document, spanTag);
    styleElement->setAttribute(classAttr, styleSpanClassString());
    return styleElement.release();
}

ApplyStyleCommand::ApplyStyleCommand(Document* document, CSSStyleDeclaration* style, EditAction editingAction, EPropertyLevel propertyLevel)
    : CompositeEditCommand(document)
    , m_style(style->makeMutable())
    , m_editingAction(editingAction)
    , m_propertyLevel(propertyLevel)
    , m_start(endingSelection().start().downstream())
    , m_end(endingSelection().end().upstream())
    , m_useEndingSelection(true)
    , m_styledInlineElement(0)
    , m_removeOnly(false)
{
}

ApplyStyleCommand::ApplyStyleCommand(Document* document, CSSStyleDeclaration* style, const Position& start, const Position& end, EditAction editingAction, EPropertyLevel propertyLevel)
    : CompositeEditCommand(document)
    , m_style(style->makeMutable())
    , m_editingAction(editingAction)
    , m_propertyLevel(propertyLevel)
    , m_start(start)
    , m_end(end)
    , m_useEndingSelection(false)
    , m_styledInlineElement(0)
    , m_removeOnly(false)
{
}

ApplyStyleCommand::ApplyStyleCommand(PassRefPtr<Element> element, bool removeOnly, EditAction editingAction)
    : CompositeEditCommand(element->document())
    , m_style(CSSMutableStyleDeclaration::create())
    , m_editingAction(editingAction)
    , m_propertyLevel(PropertyDefault)
    , m_start(endingSelection().start().downstream())
    , m_end(endingSelection().end().upstream())
    , m_useEndingSelection(true)
    , m_styledInlineElement(element)
    , m_removeOnly(removeOnly)
{
}

void ApplyStyleCommand::updateStartEnd(const Position& newStart, const Position& newEnd)
{
    ASSERT(Range::compareBoundaryPoints(newEnd, newStart) >= 0);

    if (!m_useEndingSelection && (newStart != m_start || newEnd != m_end))
        m_useEndingSelection = true;

    setEndingSelection(Selection(newStart, newEnd, VP_DEFAULT_AFFINITY));
    m_start = newStart;
    m_end = newEnd;
}

Position ApplyStyleCommand::startPosition()
{
    if (m_useEndingSelection)
        return endingSelection().start();
    
    return m_start;
}

Position ApplyStyleCommand::endPosition()
{
    if (m_useEndingSelection)
        return endingSelection().end();
    
    return m_end;
}

void ApplyStyleCommand::doApply()
{
    switch (m_propertyLevel) {
        case PropertyDefault: {
            // apply the block-centric properties of the style
            RefPtr<CSSMutableStyleDeclaration> blockStyle = m_style->copyBlockProperties();
            if (blockStyle->length())
                applyBlockStyle(blockStyle.get());
            // apply any remaining styles to the inline elements
            // NOTE: hopefully, this string comparison is the same as checking for a non-null diff
            if (blockStyle->length() < m_style->length() || m_styledInlineElement) {
                RefPtr<CSSMutableStyleDeclaration> inlineStyle = m_style->copy();
                applyRelativeFontStyleChange(inlineStyle.get());
                blockStyle->diff(inlineStyle.get());
                applyInlineStyle(inlineStyle.get());
            }
            break;
        }
        case ForceBlockProperties:
            // Force all properties to be applied as block styles.
            applyBlockStyle(m_style.get());
            break;
    }
}

EditAction ApplyStyleCommand::editingAction() const
{
    return m_editingAction;
}

void ApplyStyleCommand::applyBlockStyle(CSSMutableStyleDeclaration *style)
{
    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    updateLayout();

    // get positions we want to use for applying style
    Position start = startPosition();
    Position end = endPosition();
    if (Range::compareBoundaryPoints(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }
        
    VisiblePosition visibleStart(start);
    VisiblePosition visibleEnd(end);
    // Save and restore the selection endpoints using their indices in the editable root, since
    // addBlockStyleIfNeeded may moveParagraphs, which can remove these endpoints.
    // Calculate start and end indices from the start of the tree that they're in.
    Node* scope = highestEditableRoot(visibleStart.deepEquivalent());
    Position rangeStart(scope, 0);
    RefPtr<Range> startRange = Range::create(document(), rangeStart, rangeCompliantEquivalent(visibleStart.deepEquivalent()));
    RefPtr<Range> endRange = Range::create(document(), rangeStart, rangeCompliantEquivalent(visibleEnd.deepEquivalent()));
    int startIndex = TextIterator::rangeLength(startRange.get(), true);
    int endIndex = TextIterator::rangeLength(endRange.get(), true);
    
    VisiblePosition paragraphStart(startOfParagraph(visibleStart));
    VisiblePosition nextParagraphStart(endOfParagraph(paragraphStart).next());
    VisiblePosition beyondEnd(endOfParagraph(visibleEnd).next());
    while (paragraphStart.isNotNull() && paragraphStart != beyondEnd) {
        StyleChange styleChange(style, paragraphStart.deepEquivalent(), StyleChange::styleModeForParseMode(document()->inCompatMode()));
        if (styleChange.cssStyle().length() > 0 || m_removeOnly) {
            RefPtr<Node> block = enclosingBlock(paragraphStart.deepEquivalent().node());
            RefPtr<Node> newBlock = moveParagraphContentsToNewBlockIfNecessary(paragraphStart.deepEquivalent());
            if (newBlock)
                block = newBlock;
            ASSERT(block->isHTMLElement());
            if (block->isHTMLElement()) {
                removeCSSStyle(style, static_cast<HTMLElement*>(block.get()));
                if (!m_removeOnly)
                    addBlockStyle(styleChange, static_cast<HTMLElement*>(block.get()));
            }
        }
        paragraphStart = nextParagraphStart;
        nextParagraphStart = endOfParagraph(paragraphStart).next();
    }
    
    startRange = TextIterator::rangeFromLocationAndLength(static_cast<Element*>(scope), startIndex, 0, true);
    endRange = TextIterator::rangeFromLocationAndLength(static_cast<Element*>(scope), endIndex, 0, true);
    if (startRange && endRange)
        updateStartEnd(startRange->startPosition(), endRange->startPosition());
}

#define NoFontDelta (0.0f)
#define MinimumFontSize (0.1f)

void ApplyStyleCommand::applyRelativeFontStyleChange(CSSMutableStyleDeclaration *style)
{
    RefPtr<CSSValue> value = style->getPropertyCSSValue(CSSPropertyFontSize);
    if (value) {
        // Explicit font size overrides any delta.
        style->removeProperty(CSSPropertyWebkitFontSizeDelta);
        return;
    }

    // Get the adjustment amount out of the style.
    value = style->getPropertyCSSValue(CSSPropertyWebkitFontSizeDelta);
    if (!value)
        return;
    float adjustment = NoFontDelta;
    if (value->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE) {
        CSSPrimitiveValue *primitiveValue = static_cast<CSSPrimitiveValue *>(value.get());
        if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_PX) {
            // Only PX handled now. If we handle more types in the future, perhaps
            // a switch statement here would be more appropriate.
            adjustment = primitiveValue->getFloatValue();
        }
    }
    style->removeProperty(CSSPropertyWebkitFontSizeDelta);
    if (adjustment == NoFontDelta)
        return;
    
    Position start = startPosition();
    Position end = endPosition();
    if (Range::compareBoundaryPoints(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }
    
    // Join up any adjacent text nodes.
    if (start.node()->isTextNode()) {
        joinChildTextNodes(start.node()->parentNode(), start, end);
        start = startPosition();
        end = endPosition();
    }
    if (end.node()->isTextNode() && start.node()->parentNode() != end.node()->parentNode()) {
        joinChildTextNodes(end.node()->parentNode(), start, end);
        start = startPosition();
        end = endPosition();
    }

    // Split the start text nodes if needed to apply style.
    bool splitStart = splitTextAtStartIfNeeded(start, end); 
    if (splitStart) {
        start = startPosition();
        end = endPosition();
    }
    bool splitEnd = splitTextAtEndIfNeeded(start, end);
    if (splitEnd) {
        start = startPosition();
        end = endPosition();
    }

    // Calculate loop end point.
    // If the end node is before the start node (can only happen if the end node is
    // an ancestor of the start node), we gather nodes up to the next sibling of the end node
    Node *beyondEnd;
    if (start.node()->isDescendantOf(end.node()))
        beyondEnd = end.node()->traverseNextSibling();
    else
        beyondEnd = end.node()->traverseNextNode();
    
    start = start.upstream(); // Move upstream to ensure we do not add redundant spans.
    Node *startNode = start.node();
    if (startNode->isTextNode() && start.offset() >= caretMaxOffset(startNode)) // Move out of text node if range does not include its characters.
        startNode = startNode->traverseNextNode();

    // Store away font size before making any changes to the document.
    // This ensures that changes to one node won't effect another.
    HashMap<Node*, float> startingFontSizes;
    for (Node *node = startNode; node != beyondEnd; node = node->traverseNextNode())
        startingFontSizes.set(node, computedFontSize(node));

    // These spans were added by us. If empty after font size changes, they can be removed.
    Vector<RefPtr<HTMLElement> > unstyledSpans;
    
    Node* lastStyledNode = 0;
    for (Node* node = startNode; node != beyondEnd; node = node->traverseNextNode()) {
        RefPtr<HTMLElement> element;
        if (node->isHTMLElement()) {
            // Only work on fully selected nodes.
            if (!nodeFullySelected(node, start, end))
                continue;
            element = static_cast<HTMLElement*>(node);
        } else if (node->isTextNode() && node->renderer() && node->parentNode() != lastStyledNode) {
            // Last styled node was not parent node of this text node, but we wish to style this
            // text node. To make this possible, add a style span to surround this text node.
            RefPtr<HTMLElement> span = createStyleSpanElement(document());
            surroundNodeRangeWithElement(node, node, span.get());
            element = span.release();
        }  else {
            // Only handle HTML elements and text nodes.
            continue;
        }
        lastStyledNode = node;

        CSSMutableStyleDeclaration* inlineStyleDecl = element->getInlineStyleDecl();
        float currentFontSize = computedFontSize(node);
        float desiredFontSize = max(MinimumFontSize, startingFontSizes.get(node) + adjustment);
        RefPtr<CSSValue> value = inlineStyleDecl->getPropertyCSSValue(CSSPropertyFontSize);
        if (value) {
            inlineStyleDecl->removeProperty(CSSPropertyFontSize, true);
            currentFontSize = computedFontSize(node);
        }
        if (currentFontSize != desiredFontSize) {
            inlineStyleDecl->setProperty(CSSPropertyFontSize, String::number(desiredFontSize) + "px", false, false);
            setNodeAttribute(element.get(), styleAttr, inlineStyleDecl->cssText());
        }
        if (inlineStyleDecl->length() == 0) {
            removeNodeAttribute(element.get(), styleAttr);
            // FIXME: should this be isSpanWithoutAttributesOrUnstyleStyleSpan?  Need a test.
            if (isUnstyledStyleSpan(element.get()))
                unstyledSpans.append(element.release());
        }
    }

    size_t size = unstyledSpans.size();
    for (size_t i = 0; i < size; ++i)
        removeNodePreservingChildren(unstyledSpans[i].get());
}

#undef NoFontDelta
#undef MinimumFontSize

static Node* dummySpanAncestorForNode(const Node* node)
{
    while (node && !isStyleSpan(node))
        node = node->parent();
    
    return node ? node->parent() : 0;
}

void ApplyStyleCommand::cleanupUnstyledAppleStyleSpans(Node* dummySpanAncestor)
{
    if (!dummySpanAncestor)
        return;

    // Dummy spans are created when text node is split, so that style information
    // can be propagated, which can result in more splitting. If a dummy span gets
    // cloned/split, the new node is always a sibling of it. Therefore, we scan
    // all the children of the dummy's parent
    Node* next;
    for (Node* node = dummySpanAncestor->firstChild(); node; node = next) {
        next = node->nextSibling();
        if (isUnstyledStyleSpan(node))
            removeNodePreservingChildren(node);
        node = next;
    }
}

HTMLElement* ApplyStyleCommand::splitAncestorsWithUnicodeBidi(Node* node, bool before, RefPtr<CSSPrimitiveValue> allowedDirection)
{
    // We are allowed to leave the highest ancestor with unicode-bidi unsplit if it is unicode-bidi: embed and direction: allowedDirection.
    // In that case, we return the unsplit ancestor. Otherwise, we return 0.
    Node* block = enclosingBlock(node);
    if (!block)
        return 0;

    Node* highestAncestorWithUnicodeBidi = 0;
    Node* nextHighestAncestorWithUnicodeBidi = 0;
    RefPtr<CSSPrimitiveValue> highestAncestorUnicodeBidi;
    for (Node* n = node->parent(); n != block; n = n->parent()) {
        RefPtr<CSSValue> unicodeBidi = computedStyle(n)->getPropertyCSSValue(CSSPropertyUnicodeBidi);
        if (unicodeBidi) {
            ASSERT(unicodeBidi->isPrimitiveValue());
            if (static_cast<CSSPrimitiveValue*>(unicodeBidi.get())->getIdent() != CSSValueNormal) {
                highestAncestorUnicodeBidi = static_cast<CSSPrimitiveValue*>(unicodeBidi.get());
                nextHighestAncestorWithUnicodeBidi = highestAncestorWithUnicodeBidi;
                highestAncestorWithUnicodeBidi = n;
            }
        }
    }

    if (!highestAncestorWithUnicodeBidi)
        return 0;

    HTMLElement* unsplitAncestor = 0;

    if (allowedDirection && highestAncestorUnicodeBidi->getIdent() != CSSValueBidiOverride) {
        RefPtr<CSSValue> highestAncestorDirection = computedStyle(highestAncestorWithUnicodeBidi)->getPropertyCSSValue(CSSPropertyDirection);
        ASSERT(highestAncestorDirection->isPrimitiveValue());
        if (static_cast<CSSPrimitiveValue*>(highestAncestorDirection.get())->getIdent() == allowedDirection->getIdent() && highestAncestorWithUnicodeBidi->isHTMLElement()) {
            if (!nextHighestAncestorWithUnicodeBidi)
                return static_cast<HTMLElement*>(highestAncestorWithUnicodeBidi);

            unsplitAncestor = static_cast<HTMLElement*>(highestAncestorWithUnicodeBidi);
            highestAncestorWithUnicodeBidi = nextHighestAncestorWithUnicodeBidi;
        }
    }

    // Split every ancestor through highest ancestor with embedding.
    Node* n = node;
    while (true) {
        Element* parent = static_cast<Element*>(n->parent());
        if (before ? n->previousSibling() : n->nextSibling())
            splitElement(parent, before ? n : n->nextSibling());
        if (parent == highestAncestorWithUnicodeBidi)
            break;
        n = n->parent();
    }
    return unsplitAncestor;
}

void ApplyStyleCommand::removeEmbeddingUpToEnclosingBlock(Node* node, Node* unsplitAncestor)
{
    Node* block = enclosingBlock(node);
    if (!block)
        return;

    Node* n = node->parent();
    while (n != block && n != unsplitAncestor) {
        Node* parent = n->parent();
        if (!n->isStyledElement()) {
            n = parent;
            continue;
        }

        StyledElement* element = static_cast<StyledElement*>(n);
        RefPtr<CSSValue> unicodeBidi = computedStyle(element)->getPropertyCSSValue(CSSPropertyUnicodeBidi);
        if (unicodeBidi) {
            ASSERT(unicodeBidi->isPrimitiveValue());
            if (static_cast<CSSPrimitiveValue*>(unicodeBidi.get())->getIdent() != CSSValueNormal) {
                // FIXME: This code should really consider the mapped attribute 'dir', the inline style declaration,
                // and all matching style rules in order to determine how to best set the unicode-bidi property to 'normal'.
                // For now, it assumes that if the 'dir' attribute is present, then removing it will suffice, and
                // otherwise it sets the property in the inline style declaration.
                if (element->hasAttribute(dirAttr)) {
                    // FIXME: If this is a BDO element, we should probably just remove it if it has no
                    // other attributes, like we (should) do with B and I elements.
                    removeNodeAttribute(element, dirAttr);
                } else {
                    RefPtr<CSSMutableStyleDeclaration> inlineStyle = element->getInlineStyleDecl()->copy();
                    inlineStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueNormal);
                    inlineStyle->removeProperty(CSSPropertyDirection);
                    setNodeAttribute(element, styleAttr, inlineStyle->cssText());
                    // FIXME: should this be isSpanWithoutAttributesOrUnstyleStyleSpan?  Need a test.
                    if (isUnstyledStyleSpan(element))
                        removeNodePreservingChildren(element);
                }
            }
        }
        n = parent;
    }
}

void ApplyStyleCommand::applyInlineStyle(CSSMutableStyleDeclaration *style)
{
    Node* startDummySpanAncestor = 0;
    Node* endDummySpanAncestor = 0;
    
    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    updateLayout();

    // adjust to the positions we want to use for applying style
    Position start = startPosition();
    Position end = endPosition();
    if (Range::compareBoundaryPoints(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }

    // split the start node and containing element if the selection starts inside of it
    bool splitStart = splitTextElementAtStartIfNeeded(start, end); 
    if (splitStart) {
        start = startPosition();
        end = endPosition();
        startDummySpanAncestor = dummySpanAncestorForNode(start.node());
    }

    // split the end node and containing element if the selection ends inside of it
    bool splitEnd = splitTextElementAtEndIfNeeded(start, end);
    if (splitEnd) {
        start = startPosition();
        end = endPosition();
        endDummySpanAncestor = dummySpanAncestorForNode(end.node());
    }

    RefPtr<CSSValue> unicodeBidi = style->getPropertyCSSValue(CSSPropertyUnicodeBidi);
    RefPtr<CSSValue> direction;
    HTMLElement* startUnsplitAncestor = 0;
    HTMLElement* endUnsplitAncestor = 0;
    if (unicodeBidi) {
        RefPtr<CSSPrimitiveValue> allowedDirection;
        ASSERT(unicodeBidi->isPrimitiveValue());
        if (static_cast<CSSPrimitiveValue*>(unicodeBidi.get())->getIdent() == CSSValueEmbed) {
            // Leave alone an ancestor that provides the desired single level embedding, if there is one.
            direction = style->getPropertyCSSValue(CSSPropertyDirection);
            ASSERT(direction->isPrimitiveValue());
            allowedDirection = static_cast<CSSPrimitiveValue*>(direction.get());
        }
        startUnsplitAncestor = splitAncestorsWithUnicodeBidi(start.node(), true, allowedDirection);
        endUnsplitAncestor = splitAncestorsWithUnicodeBidi(end.node(), false, allowedDirection);
        removeEmbeddingUpToEnclosingBlock(start.node(), startUnsplitAncestor);
        removeEmbeddingUpToEnclosingBlock(end.node(), endUnsplitAncestor);
    }

    // Remove style from the selection.
    // Use the upstream position of the start for removing style.
    // This will ensure we remove all traces of the relevant styles from the selection
    // and prevent us from adding redundant ones, as described in:
    // <rdar://problem/3724344> Bolding and unbolding creates extraneous tags
    Position removeStart = start.upstream();
    Position embeddingRemoveStart = removeStart;
    Position embeddingRemoveEnd = end;
    if (unicodeBidi) {
        // Avoid removing the dir attribute and the unicode-bidi and direction properties from the unsplit ancestors.
        if (startUnsplitAncestor && nodeFullySelected(startUnsplitAncestor, removeStart, end))
            embeddingRemoveStart = positionAfterNode(startUnsplitAncestor);
        if (endUnsplitAncestor && nodeFullySelected(endUnsplitAncestor, removeStart, end))
            embeddingRemoveEnd = positionBeforeNode(endUnsplitAncestor).downstream();
    }

    if (embeddingRemoveStart != removeStart || embeddingRemoveEnd != end) {
        RefPtr<CSSMutableStyleDeclaration> embeddingStyle = CSSMutableStyleDeclaration::create();
        embeddingStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed);
        embeddingStyle->setProperty(CSSPropertyDirection, static_cast<CSSPrimitiveValue*>(direction.get())->getIdent());
        if (Range::compareBoundaryPoints(embeddingRemoveStart, embeddingRemoveEnd) <= 0)
            removeInlineStyle(embeddingStyle, embeddingRemoveStart, embeddingRemoveEnd);

        RefPtr<CSSMutableStyleDeclaration> styleWithoutEmbedding = style->copy();
        styleWithoutEmbedding->removeProperty(CSSPropertyUnicodeBidi);
        styleWithoutEmbedding->removeProperty(CSSPropertyDirection);
        removeInlineStyle(styleWithoutEmbedding, removeStart, end);
   } else
        removeInlineStyle(style, removeStart, end);

    start = startPosition();
    end = endPosition();

    if (splitStart) {
        bool mergedStart = mergeStartWithPreviousIfIdentical(start, end);
        if (mergedStart) {
            start = startPosition();
            end = endPosition();
        }
    }

    if (splitEnd) {
        mergeEndWithNextIfIdentical(start, end);
        start = startPosition();
        end = endPosition();
    }

    // update document layout once before running the rest of the function
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    updateLayout();

    Position embeddingApplyStart = start;
    Position embeddingApplyEnd = end;
    if (unicodeBidi) {
        // Avoid applying the unicode-bidi and direction properties beneath ancestors that already have them.
        Node* startEnclosingBlock = enclosingBlock(start.node());
        for (Node* n = start.node(); n != startEnclosingBlock; n = n->parent()) {
            if (n->isHTMLElement()) {
                RefPtr<CSSValue> ancestorUnicodeBidi = computedStyle(n)->getPropertyCSSValue(CSSPropertyUnicodeBidi);
                if (ancestorUnicodeBidi) {
                    ASSERT(ancestorUnicodeBidi->isPrimitiveValue());
                    if (static_cast<CSSPrimitiveValue*>(ancestorUnicodeBidi.get())->getIdent() == CSSValueEmbed) {
                        embeddingApplyStart = positionAfterNode(n);
                        break;
                    }
                }
            }
        }

        Node* endEnclosingBlock = enclosingBlock(end.node());
        for (Node* n = end.node(); n != endEnclosingBlock; n = n->parent()) {
            if (n->isHTMLElement()) {
                RefPtr<CSSValue> ancestorUnicodeBidi = computedStyle(n)->getPropertyCSSValue(CSSPropertyUnicodeBidi);
                if (ancestorUnicodeBidi) {
                    ASSERT(ancestorUnicodeBidi->isPrimitiveValue());
                    if (static_cast<CSSPrimitiveValue*>(ancestorUnicodeBidi.get())->getIdent() == CSSValueEmbed) {
                        embeddingApplyEnd = positionBeforeNode(n);
                        break;
                    }
                }
            }
        }
    }

    if (embeddingApplyStart != start || embeddingApplyEnd != end) {
        if (embeddingApplyStart.isNotNull() && embeddingApplyEnd.isNotNull()) {
            RefPtr<CSSMutableStyleDeclaration> embeddingStyle = CSSMutableStyleDeclaration::create();
            embeddingStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed);
            embeddingStyle->setProperty(CSSPropertyDirection, static_cast<CSSPrimitiveValue*>(direction.get())->getIdent());
            applyInlineStyleToRange(embeddingStyle.get(), embeddingApplyStart, embeddingApplyEnd);
        }

        RefPtr<CSSMutableStyleDeclaration> styleWithoutEmbedding = style->copy();
        styleWithoutEmbedding->removeProperty(CSSPropertyUnicodeBidi);
        styleWithoutEmbedding->removeProperty(CSSPropertyDirection);
        applyInlineStyleToRange(styleWithoutEmbedding.get(), start, end);
   } else
        applyInlineStyleToRange(style, start, end);

    // Remove dummy style spans created by splitting text elements.
    cleanupUnstyledAppleStyleSpans(startDummySpanAncestor);
    if (endDummySpanAncestor != startDummySpanAncestor)
        cleanupUnstyledAppleStyleSpans(endDummySpanAncestor);
}

void ApplyStyleCommand::applyInlineStyleToRange(CSSMutableStyleDeclaration* style, const Position& start, const Position& rangeEnd)
{
    Node* node = start.node();
    Position end = rangeEnd;

    bool rangeIsEmpty = false;

    if (start.offset() >= caretMaxOffset(start.node())) {
        node = node->traverseNextNode();
        Position newStart = Position(node, 0);
        if (!node || Range::compareBoundaryPoints(end, newStart) < 0)
            rangeIsEmpty = true;
    }

    if (!rangeIsEmpty) {
        // pastEndNode is the node after the last fully selected node.
        Node* pastEndNode = end.node();
        if (end.offset() >= caretMaxOffset(end.node()))
            pastEndNode = end.node()->traverseNextSibling();
        // FIXME: Callers should perform this operation on a Range that includes the br
        // if they want style applied to the empty line.
        if (start == end && start.node()->hasTagName(brTag))
            pastEndNode = start.node()->traverseNextNode();
        // Add the style to selected inline runs.
        for (Node* next; node && node != pastEndNode; node = next) {
            
            next = node->traverseNextNode();
            
            if (!node->renderer() || !node->isContentEditable())
                continue;
            
            if (!node->isContentRichlyEditable() && node->isHTMLElement()) {
                // This is a plaintext-only region. Only proceed if it's fully selected.
                // pastEndNode is the node after the last fully selected node, so if it's inside node then
                // node isn't fully selected.
                if (pastEndNode->isDescendantOf(node))
                    break;
                // Add to this element's inline style and skip over its contents.
                HTMLElement* element = static_cast<HTMLElement*>(node);
                RefPtr<CSSMutableStyleDeclaration> inlineStyle = element->getInlineStyleDecl()->copy();
                inlineStyle->merge(style);
                setNodeAttribute(element, styleAttr, inlineStyle->cssText());
                next = node->traverseNextSibling();
                continue;
            }
        
            if (isBlock(node))
                continue;
                
            if (node->childNodeCount()) {
                if (editingIgnoresContent(node)) {
                    next = node->traverseNextSibling();
                    continue;
                }
                continue;
            }
            
            Node* runStart = node;
            // Find the end of the run.
            Node* sibling = node->nextSibling();
            while (sibling && sibling != pastEndNode && (!sibling->isElementNode() || sibling->hasTagName(brTag)) && !isBlock(sibling)) {
                node = sibling;
                sibling = node->nextSibling();
            }
            // Recompute next, since node has changed.
            next = node->traverseNextNode();
            // Apply the style to the run.
            addInlineStyleIfNeeded(style, runStart, node);
        }
    }
}

// This function maps from styling tags to CSS styles.  Used for knowing which
// styling tags should be removed when toggling styles.
bool ApplyStyleCommand::isHTMLStyleNode(CSSMutableStyleDeclaration* style, HTMLElement* elem)
{
    CSSMutableStyleDeclaration::const_iterator end = style->end();
    for (CSSMutableStyleDeclaration::const_iterator it = style->begin(); it != end; ++it) {
        switch ((*it).id()) {
        case CSSPropertyFontWeight:
            if (elem->hasLocalName(bTag))
                return true;
            break;
        case CSSPropertyVerticalAlign:
            if (elem->hasLocalName(subTag) || elem->hasLocalName(supTag))
                return true;
            break;
        case CSSPropertyFontStyle:
            if (elem->hasLocalName(iTag))
                return true;
        }
    }

    return false;
}

void ApplyStyleCommand::removeHTMLStyleNode(HTMLElement *elem)
{
    // This node can be removed.
    // EDIT FIXME: This does not handle the case where the node
    // has attributes. But how often do people add attributes to <B> tags? 
    // Not so often I think.
    ASSERT(elem);
    removeNodePreservingChildren(elem);
}

void ApplyStyleCommand::removeHTMLFontStyle(CSSMutableStyleDeclaration *style, HTMLElement *elem)
{
    ASSERT(style);
    ASSERT(elem);

    if (!elem->hasLocalName(fontTag))
        return;
        
    CSSMutableStyleDeclaration::const_iterator end = style->end();
    for (CSSMutableStyleDeclaration::const_iterator it = style->begin(); it != end; ++it) {
        switch ((*it).id()) {
            case CSSPropertyColor:
                removeNodeAttribute(elem, colorAttr);
                break;
            case CSSPropertyFontFamily:
                removeNodeAttribute(elem, faceAttr);
                break;
            case CSSPropertyFontSize:
                removeNodeAttribute(elem, sizeAttr);
                break;
        }
    }

    if (isEmptyFontTag(elem))
        removeNodePreservingChildren(elem);
}

void ApplyStyleCommand::removeHTMLBidiEmbeddingStyle(CSSMutableStyleDeclaration *style, HTMLElement *elem)
{
    ASSERT(style);
    ASSERT(elem);

    if (!elem->hasAttribute(dirAttr))
        return;

    if (!style->getPropertyCSSValue(CSSPropertyUnicodeBidi) && !style->getPropertyCSSValue(CSSPropertyDirection))
        return;

    removeNodeAttribute(elem, dirAttr);

    // FIXME: should this be isSpanWithoutAttributesOrUnstyleStyleSpan?  Need a test.
    if (isUnstyledStyleSpan(elem))
        removeNodePreservingChildren(elem);
}

void ApplyStyleCommand::removeCSSStyle(CSSMutableStyleDeclaration* style, HTMLElement* elem)
{
    ASSERT(style);
    ASSERT(elem);

    CSSMutableStyleDeclaration* decl = elem->inlineStyleDecl();
    if (!decl)
        return;

    CSSMutableStyleDeclaration::const_iterator end = style->end();
    for (CSSMutableStyleDeclaration::const_iterator it = style->begin(); it != end; ++it) {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>((*it).id());
        RefPtr<CSSValue> value = decl->getPropertyCSSValue(propertyID);
        if (value && (propertyID != CSSPropertyWhiteSpace || !isTabSpanNode(elem))) {
            removeCSSProperty(decl, propertyID);
            if (propertyID == CSSPropertyUnicodeBidi && !decl->getPropertyValue(CSSPropertyDirection).isEmpty())
                removeCSSProperty(decl, CSSPropertyDirection);
        }
    }

    // No need to serialize <foo style=""> if we just removed the last css property
    if (decl->length() == 0)
        removeNodeAttribute(elem, styleAttr);

    if (isSpanWithoutAttributesOrUnstyleStyleSpan(elem))
        removeNodePreservingChildren(elem);
}

static bool hasTextDecorationProperty(Node *node)
{
    if (!node->isElementNode())
        return false;

    RefPtr<CSSValue> value = computedStyle(node)->getPropertyCSSValue(CSSPropertyTextDecoration, DoNotUpdateLayout);
    return value && !equalIgnoringCase(value->cssText(), "none");
}

static Node* highestAncestorWithTextDecoration(Node *node)
{
    Node *result = NULL;

    for (Node *n = node; n; n = n->parentNode()) {
        if (hasTextDecorationProperty(n))
            result = n;
    }

    return result;
}

PassRefPtr<CSSMutableStyleDeclaration> ApplyStyleCommand::extractTextDecorationStyle(Node* node)
{
    ASSERT(node);
    ASSERT(node->isElementNode());
    
    // non-html elements not handled yet
    if (!node->isHTMLElement())
        return 0;

    HTMLElement *element = static_cast<HTMLElement *>(node);
    RefPtr<CSSMutableStyleDeclaration> style = element->inlineStyleDecl();
    if (!style)
        return 0;

    int properties[1] = { CSSPropertyTextDecoration };
    RefPtr<CSSMutableStyleDeclaration> textDecorationStyle = style->copyPropertiesInSet(properties, 1);

    RefPtr<CSSValue> property = style->getPropertyCSSValue(CSSPropertyTextDecoration);
    if (property && !equalIgnoringCase(property->cssText(), "none"))
        removeCSSProperty(style.get(), CSSPropertyTextDecoration);

    return textDecorationStyle.release();
}

PassRefPtr<CSSMutableStyleDeclaration> ApplyStyleCommand::extractAndNegateTextDecorationStyle(Node* node)
{
    ASSERT(node);
    ASSERT(node->isElementNode());
    
    // non-html elements not handled yet
    if (!node->isHTMLElement())
        return 0;

    RefPtr<CSSComputedStyleDeclaration> nodeStyle = computedStyle(node);
    ASSERT(nodeStyle);

    int properties[1] = { CSSPropertyTextDecoration };
    RefPtr<CSSMutableStyleDeclaration> textDecorationStyle = nodeStyle->copyPropertiesInSet(properties, 1);

    RefPtr<CSSValue> property = nodeStyle->getPropertyCSSValue(CSSPropertyTextDecoration);
    if (property && !equalIgnoringCase(property->cssText(), "none")) {
        RefPtr<CSSMutableStyleDeclaration> newStyle = textDecorationStyle->copy();
        newStyle->setProperty(CSSPropertyTextDecoration, "none");
        applyTextDecorationStyle(node, newStyle.get());
    }

    return textDecorationStyle.release();
}

void ApplyStyleCommand::applyTextDecorationStyle(Node *node, CSSMutableStyleDeclaration *style)
{
    ASSERT(node);

    if (!style || !style->cssText().length())
        return;

    if (node->isTextNode()) {
        RefPtr<HTMLElement> styleSpan = createStyleSpanElement(document());
        surroundNodeRangeWithElement(node, node, styleSpan.get());
        node = styleSpan.get();
    }

    if (!node->isElementNode())
        return;

    HTMLElement *element = static_cast<HTMLElement *>(node);
        
    StyleChange styleChange(style, Position(element, 0), StyleChange::styleModeForParseMode(document()->inCompatMode()));
    if (styleChange.cssStyle().length() > 0) {
        String cssText = styleChange.cssStyle();
        CSSMutableStyleDeclaration *decl = element->inlineStyleDecl();
        if (decl)
            cssText += decl->cssText();
        setNodeAttribute(element, styleAttr, cssText);
    }
}

void ApplyStyleCommand::pushDownTextDecorationStyleAroundNode(Node* node, bool force)
{
    Node *highestAncestor = highestAncestorWithTextDecoration(node);
    
    if (highestAncestor) {
        Node *nextCurrent;
        Node *nextChild;
        for (Node *current = highestAncestor; current != node; current = nextCurrent) {
            ASSERT(current);
            
            nextCurrent = NULL;
            
            RefPtr<CSSMutableStyleDeclaration> decoration = force ? extractAndNegateTextDecorationStyle(current) : extractTextDecorationStyle(current);

            for (Node *child = current->firstChild(); child; child = nextChild) {
                nextChild = child->nextSibling();

                if (node == child) {
                    nextCurrent = child;
                } else if (node->isDescendantOf(child)) {
                    applyTextDecorationStyle(child, decoration.get());
                    nextCurrent = child;
                } else {
                    applyTextDecorationStyle(child, decoration.get());
                }
            }
        }
    }
}

void ApplyStyleCommand::pushDownTextDecorationStyleAtBoundaries(const Position &start, const Position &end)
{
    // We need to work in two passes. First we push down any inline
    // styles that set text decoration. Then we look for any remaining
    // styles (caused by stylesheets) and explicitly negate text
    // decoration while pushing down.

    pushDownTextDecorationStyleAroundNode(start.node(), false);
    updateLayout();
    pushDownTextDecorationStyleAroundNode(start.node(), true);

    pushDownTextDecorationStyleAroundNode(end.node(), false);
    updateLayout();
    pushDownTextDecorationStyleAroundNode(end.node(), true);
}

static int maxRangeOffset(Node *n)
{
    if (n->offsetInCharacters())
        return n->maxCharacterOffset();

    if (n->isElementNode())
        return n->childNodeCount();

    return 1;
}

void ApplyStyleCommand::removeInlineStyle(PassRefPtr<CSSMutableStyleDeclaration> style, const Position &start, const Position &end)
{
    ASSERT(start.isNotNull());
    ASSERT(end.isNotNull());
    ASSERT(start.node()->inDocument());
    ASSERT(end.node()->inDocument());
    ASSERT(Range::compareBoundaryPoints(start, end) <= 0);
    
    RefPtr<CSSValue> textDecorationSpecialProperty = style->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect);

    if (textDecorationSpecialProperty) {
        pushDownTextDecorationStyleAtBoundaries(start.downstream(), end.upstream());
        style = style->copy();
        style->setProperty(CSSPropertyTextDecoration, textDecorationSpecialProperty->cssText(), style->getPropertyPriority(CSSPropertyWebkitTextDecorationsInEffect));
    }

    // The s and e variables store the positions used to set the ending selection after style removal
    // takes place. This will help callers to recognize when either the start node or the end node
    // are removed from the document during the work of this function.
    Position s = start;
    Position e = end;

    Node *node = start.node();
    while (node) {
        Node *next = node->traverseNextNode();
        if (node->isHTMLElement() && nodeFullySelected(node, start, end)) {
            HTMLElement *elem = static_cast<HTMLElement *>(node);
            Node *prev = elem->traversePreviousNodePostOrder();
            Node *next = elem->traverseNextNode();
            if (m_styledInlineElement && elem->hasTagName(m_styledInlineElement->tagQName()))
                removeNodePreservingChildren(elem);
            if (isHTMLStyleNode(style.get(), elem))
                removeHTMLStyleNode(elem);
            else {
                removeHTMLFontStyle(style.get(), elem);
                removeHTMLBidiEmbeddingStyle(style.get(), elem);
                removeCSSStyle(style.get(), elem);
            }
            if (!elem->inDocument()) {
                if (s.node() == elem) {
                    // Since elem must have been fully selected, and it is at the start
                    // of the selection, it is clear we can set the new s offset to 0.
                    ASSERT(s.offset() <= caretMinOffset(s.node()));
                    s = Position(next, 0);
                }
                if (e.node() == elem) {
                    // Since elem must have been fully selected, and it is at the end
                    // of the selection, it is clear we can set the new e offset to
                    // the max range offset of prev.
                    ASSERT(e.offset() >= maxRangeOffset(e.node()));
                    e = Position(prev, maxRangeOffset(prev));
                }
            }
        }
        if (node == end.node())
            break;
        node = next;
    }
    
    ASSERT(s.node()->inDocument());
    ASSERT(e.node()->inDocument());
    updateStartEnd(s, e);
}

bool ApplyStyleCommand::nodeFullySelected(Node *node, const Position &start, const Position &end) const
{
    ASSERT(node);
    ASSERT(node->isElementNode());

    Position pos = Position(node, node->childNodeCount()).upstream();
    return Range::compareBoundaryPoints(node, 0, start.node(), start.offset()) >= 0 &&
        Range::compareBoundaryPoints(pos, end) <= 0;
}

bool ApplyStyleCommand::nodeFullyUnselected(Node *node, const Position &start, const Position &end) const
{
    ASSERT(node);
    ASSERT(node->isElementNode());

    Position pos = Position(node, node->childNodeCount()).upstream();
    bool isFullyBeforeStart = Range::compareBoundaryPoints(pos, start) < 0;
    bool isFullyAfterEnd = Range::compareBoundaryPoints(node, 0, end.node(), end.offset()) > 0;

    return isFullyBeforeStart || isFullyAfterEnd;
}


bool ApplyStyleCommand::splitTextAtStartIfNeeded(const Position &start, const Position &end)
{
    if (start.node()->isTextNode() && start.offset() > caretMinOffset(start.node()) && start.offset() < caretMaxOffset(start.node())) {
        int endOffsetAdjustment = start.node() == end.node() ? start.offset() : 0;
        Text *text = static_cast<Text *>(start.node());
        splitTextNode(text, start.offset());
        updateStartEnd(Position(start.node(), 0), Position(end.node(), end.offset() - endOffsetAdjustment));
        return true;
    }
    return false;
}

bool ApplyStyleCommand::splitTextAtEndIfNeeded(const Position &start, const Position &end)
{
    if (end.node()->isTextNode() && end.offset() > caretMinOffset(end.node()) && end.offset() < caretMaxOffset(end.node())) {
        Text *text = static_cast<Text *>(end.node());
        splitTextNode(text, end.offset());
        
        Node *prevNode = text->previousSibling();
        ASSERT(prevNode);
        Node *startNode = start.node() == end.node() ? prevNode : start.node();
        ASSERT(startNode);
        updateStartEnd(Position(startNode, start.offset()), Position(prevNode, caretMaxOffset(prevNode)));
        return true;
    }
    return false;
}

bool ApplyStyleCommand::splitTextElementAtStartIfNeeded(const Position &start, const Position &end)
{
    if (start.node()->isTextNode() && start.offset() > caretMinOffset(start.node()) && start.offset() < caretMaxOffset(start.node())) {
        int endOffsetAdjustment = start.node() == end.node() ? start.offset() : 0;
        Text *text = static_cast<Text *>(start.node());
        splitTextNodeContainingElement(text, start.offset());

        updateStartEnd(Position(start.node()->parentNode(), start.node()->nodeIndex()), Position(end.node(), end.offset() - endOffsetAdjustment));
        return true;
    }
    return false;
}

bool ApplyStyleCommand::splitTextElementAtEndIfNeeded(const Position &start, const Position &end)
{
    if (end.node()->isTextNode() && end.offset() > caretMinOffset(end.node()) && end.offset() < caretMaxOffset(end.node())) {
        Text *text = static_cast<Text *>(end.node());
        splitTextNodeContainingElement(text, end.offset());

        Node *prevNode = text->parent()->previousSibling()->lastChild();
        ASSERT(prevNode);
        Node *startNode = start.node() == end.node() ? prevNode : start.node();
        ASSERT(startNode);
        updateStartEnd(Position(startNode, start.offset()), Position(prevNode->parent(), prevNode->nodeIndex() + 1));
        return true;
    }
    return false;
}

static bool areIdenticalElements(Node *first, Node *second)
{
    // check that tag name and all attribute names and values are identical

    if (!first->isElementNode())
        return false;
    
    if (!second->isElementNode())
        return false;

    Element *firstElement = static_cast<Element *>(first);
    Element *secondElement = static_cast<Element *>(second);
    
    if (!firstElement->tagQName().matches(secondElement->tagQName()))
        return false;

    NamedAttrMap *firstMap = firstElement->attributes();
    NamedAttrMap *secondMap = secondElement->attributes();

    unsigned firstLength = firstMap->length();

    if (firstLength != secondMap->length())
        return false;

    for (unsigned i = 0; i < firstLength; i++) {
        Attribute *attribute = firstMap->attributeItem(i);
        Attribute *secondAttribute = secondMap->getAttributeItem(attribute->name());

        if (!secondAttribute || attribute->value() != secondAttribute->value())
            return false;
    }
    
    return true;
}

bool ApplyStyleCommand::mergeStartWithPreviousIfIdentical(const Position &start, const Position &end)
{
    Node *startNode = start.node();
    int startOffset = start.offset();

    if (isAtomicNode(start.node())) {
        if (start.offset() != 0)
            return false;

        // note: prior siblings could be unrendered elements. it's silly to miss the
        // merge opportunity just for that.
        if (start.node()->previousSibling())
            return false;

        startNode = start.node()->parent();
        startOffset = 0;
    }

    if (!startNode->isElementNode())
        return false;

    if (startOffset != 0)
        return false;

    Node *previousSibling = startNode->previousSibling();

    if (previousSibling && areIdenticalElements(startNode, previousSibling)) {
        Element *previousElement = static_cast<Element *>(previousSibling);
        Element *element = static_cast<Element *>(startNode);
        Node *startChild = element->firstChild();
        ASSERT(startChild);
        mergeIdenticalElements(previousElement, element);

        int startOffsetAdjustment = startChild->nodeIndex();
        int endOffsetAdjustment = startNode == end.node() ? startOffsetAdjustment : 0;
        updateStartEnd(Position(startNode, startOffsetAdjustment), Position(end.node(), end.offset() + endOffsetAdjustment)); 
        return true;
    }

    return false;
}

bool ApplyStyleCommand::mergeEndWithNextIfIdentical(const Position &start, const Position &end)
{
    Node *endNode = end.node();
    int endOffset = end.offset();

    if (isAtomicNode(endNode)) {
        if (endOffset < caretMaxOffset(endNode))
            return false;

        unsigned parentLastOffset = end.node()->parent()->childNodes()->length() - 1;
        if (end.node()->nextSibling())
            return false;

        endNode = end.node()->parent();
        endOffset = parentLastOffset;
    }

    if (!endNode->isElementNode() || endNode->hasTagName(brTag))
        return false;

    Node *nextSibling = endNode->nextSibling();

    if (nextSibling && areIdenticalElements(endNode, nextSibling)) {
        Element *nextElement = static_cast<Element *>(nextSibling);
        Element *element = static_cast<Element *>(endNode);
        Node *nextChild = nextElement->firstChild();

        mergeIdenticalElements(element, nextElement);

        Node *startNode = start.node() == endNode ? nextElement : start.node();
        ASSERT(startNode);

        int endOffset = nextChild ? nextChild->nodeIndex() : nextElement->childNodes()->length();
        updateStartEnd(Position(startNode, start.offset()), Position(nextElement, endOffset));
        return true;
    }

    return false;
}

void ApplyStyleCommand::surroundNodeRangeWithElement(Node* startNode, Node* endNode, PassRefPtr<Element> elementToInsert)
{
    ASSERT(startNode);
    ASSERT(endNode);
    ASSERT(elementToInsert);
    RefPtr<Element> element = elementToInsert;

    insertNodeBefore(element, startNode);
    
    Node* node = startNode;
    while (1) {
        Node* next = node->traverseNextNode();
        if (node->childNodeCount() == 0 && node->renderer() && node->renderer()->isInline()) {
            removeNode(node);
            appendNode(node, element);
        }
        if (node == endNode)
            break;
        node = next;
    }
    // FIXME: We should probably call updateStartEnd if the start or end was in the node
    // range so that the endingSelection() is canonicalized.  See the comments at the end of
    // Selection::validate().
}

void ApplyStyleCommand::addBlockStyle(const StyleChange& styleChange, HTMLElement* block)
{
    // Do not check for legacy styles here. Those styles, like <B> and <I>, only apply for
    // inline content.
    if (!block)
        return;
        
    String cssText = styleChange.cssStyle();
    CSSMutableStyleDeclaration* decl = block->inlineStyleDecl();
    if (decl)
        cssText += decl->cssText();
    setNodeAttribute(block, styleAttr, cssText);
}

static bool fontColorChangesComputedStyle(RenderStyle* computedStyle, StyleChange styleChange)
{
    if (styleChange.applyFontColor()) {
        if (Color(styleChange.fontColor()) != computedStyle->color())
            return true;
    }
    return false;
}

static bool fontSizeChangesComputedStyle(RenderStyle* computedStyle, StyleChange styleChange)
{
    if (styleChange.applyFontSize()) {
        if (styleChange.fontSize().toInt() != computedStyle->fontSize())
            return true;
    }
    return false;
}

static bool fontFaceChangesComputedStyle(RenderStyle* computedStyle, StyleChange styleChange)
{
    if (styleChange.applyFontFace()) {
        if (computedStyle->fontDescription().family().family().string() != styleChange.fontFace())
            return true;
    }
    return false;
}

void ApplyStyleCommand::addInlineStyleIfNeeded(CSSMutableStyleDeclaration *style, Node *startNode, Node *endNode)
{
    if (m_removeOnly)
        return;

    StyleChange styleChange(style, Position(startNode, 0), StyleChange::styleModeForParseMode(document()->inCompatMode()));

    //
    // Font tags need to go outside of CSS so that CSS font sizes override leagcy font sizes.
    //
    if (styleChange.applyFontColor() || styleChange.applyFontFace() || styleChange.applyFontSize()) {
        RefPtr<Element> fontElement = createFontElement(document());
        RenderStyle* computedStyle = startNode->computedStyle();

        // We only want to insert a font element if it will end up changing the style of the
        // text somehow. Otherwise it will be a garbage node that will create problems for us
        // most notably when we apply a blockquote style for a message reply.
        if (fontColorChangesComputedStyle(computedStyle, styleChange)
                || fontFaceChangesComputedStyle(computedStyle, styleChange)
                || fontSizeChangesComputedStyle(computedStyle, styleChange)) {
            if (styleChange.applyFontColor())
                fontElement->setAttribute(colorAttr, styleChange.fontColor());
            if (styleChange.applyFontFace())
                fontElement->setAttribute(faceAttr, styleChange.fontFace());
            if (styleChange.applyFontSize())
                fontElement->setAttribute(sizeAttr, styleChange.fontSize());
            surroundNodeRangeWithElement(startNode, endNode, fontElement.get());
        }
    }

    if (styleChange.cssStyle().length() > 0) {
        RefPtr<Element> styleElement = createStyleSpanElement(document());
        styleElement->setAttribute(styleAttr, styleChange.cssStyle());
        surroundNodeRangeWithElement(startNode, endNode, styleElement.release());
    }

    if (styleChange.applyBold())
        surroundNodeRangeWithElement(startNode, endNode, createHTMLElement(document(), bTag));

    if (styleChange.applyItalic())
        surroundNodeRangeWithElement(startNode, endNode, createHTMLElement(document(), iTag));

    if (styleChange.applySubscript())
        surroundNodeRangeWithElement(startNode, endNode, createHTMLElement(document(), subTag));
    else if (styleChange.applySuperscript())
        surroundNodeRangeWithElement(startNode, endNode, createHTMLElement(document(), supTag));

    if (m_styledInlineElement)
        surroundNodeRangeWithElement(startNode, endNode, m_styledInlineElement->cloneElement());
}

float ApplyStyleCommand::computedFontSize(const Node *node)
{
    if (!node)
        return 0;
    
    Position pos(const_cast<Node *>(node), 0);
    RefPtr<CSSComputedStyleDeclaration> computedStyle = pos.computedStyle();
    if (!computedStyle)
        return 0;

    RefPtr<CSSPrimitiveValue> value = static_pointer_cast<CSSPrimitiveValue>(computedStyle->getPropertyCSSValue(CSSPropertyFontSize));
    if (!value)
        return 0;

    return value->getFloatValue(CSSPrimitiveValue::CSS_PX);
}

void ApplyStyleCommand::joinChildTextNodes(Node *node, const Position &start, const Position &end)
{
    if (!node)
        return;

    Position newStart = start;
    Position newEnd = end;
    
    Node *child = node->firstChild();
    while (child) {
        Node *next = child->nextSibling();
        if (child->isTextNode() && next && next->isTextNode()) {
            Text *childText = static_cast<Text *>(child);
            Text *nextText = static_cast<Text *>(next);
            if (next == start.node())
                newStart = Position(childText, childText->length() + start.offset());
            if (next == end.node())
                newEnd = Position(childText, childText->length() + end.offset());
            String textToMove = nextText->data();
            insertTextIntoNode(childText, childText->length(), textToMove);
            removeNode(next);
            // don't move child node pointer. it may want to merge with more text nodes.
        }
        else {
            child = child->nextSibling();
        }
    }

    updateStartEnd(newStart, newEnd);
}

}
