/*
 * Copyright (C) 2006, 2007, Apple Inc. All rights reserved.
 */

#include "config.h"
#include "Editor.h"

#include "Clipboard.h"
#include "ClipboardIOS.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPrimitiveValueMappings.h"
#include "Font.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "NodeTraversal.h"
#include "RenderBlock.h"
#include "StylePropertySet.h"
#include "Text.h"
#include "TypingCommand.h"
#include "WAKAppKitStubs.h"
#include "htmlediting.h"

#if PLATFORM(IOS)

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy, Frame* frame)
{
    return ClipboardIOS::create(Clipboard::CopyAndPaste, policy, frame);
}    

void Editor::showFontPanel()
{
}

void Editor::showStylesPanel()
{
}

void Editor::showColorPanel()
{
}

void Editor::setTextAlignmentForChangedBaseWritingDirection(WritingDirection direction)
{
    // Note that the passed-in argument is the direction that has been changed to by
    // some code or user interaction outside the scope of this function. The former
    // direction is not known, nor is it required for the kind of text alignment
    // changes done by this function.
    //
    // Rules:
    // When text has no explicit alignment, set to alignment to match the writing direction.
    // If the text has left or right alignment, flip left->right and right->left. 
    // Otherwise, do nothing.

    RefPtr<EditingStyle> selectionStyle = EditingStyle::styleAtSelectionStart(frame()->selection()->selection());
    if (!selectionStyle || !selectionStyle->style())
         return;

    RefPtr<CSSPrimitiveValue> value = static_pointer_cast<CSSPrimitiveValue>(selectionStyle->style()->getPropertyCSSValue(CSSPropertyTextAlign));
    if (!value)
        return;
        
    const char *newValue = NULL;
    ETextAlign textAlign = *value;
    switch (textAlign) {
        case TASTART:
        case TAEND:
        {
            switch (direction) {
                case NaturalWritingDirection:
                    // no-op
                    break;
                case LeftToRightWritingDirection:
                    newValue = "left";
                    break;
                case RightToLeftWritingDirection:
                    newValue = "right";
                    break;
            }
            break;
        }
        case LEFT:
        case WEBKIT_LEFT:
            newValue = "right";
            break;
        case RIGHT:
        case WEBKIT_RIGHT:
            newValue = "left";
            break;
        case CENTER:
        case WEBKIT_CENTER:
        case JUSTIFY:
            // no-op
            break;
    }

    if (!newValue)
        return;

    Element* focusedElement = frame()->document()->focusedElement();
    if (focusedElement && (focusedElement->hasTagName(textareaTag) || (focusedElement->hasTagName(inputTag) &&
        (static_cast<HTMLInputElement*>(focusedElement)->isTextField() ||
         static_cast<HTMLInputElement*>(focusedElement)->isSearchField())))) {
        if (direction == NaturalWritingDirection)
            return;
        static_cast<HTMLElement*>(focusedElement)->setAttribute(alignAttr, newValue);
        frame()->document()->updateStyleIfNeeded();
        return;
    }

    RefPtr<MutableStylePropertySet> style = MutableStylePropertySet::create();
    style->setProperty(CSSPropertyTextAlign, newValue);
    applyParagraphStyle(style.get());
}

bool Editor::insertParagraphSeparatorInQuotedContent()
{
    // FIXME: Why is this missing calls to canEdit, canEditRichly, etc...
    TypingCommand::insertParagraphSeparatorInQuotedContent(m_frame->document());
    revealSelectionAfterEditingOperation();
    return true;
}

// FIXME: Copied from EditorMac. This should be shared between the two so that
// the implementation does not differ.
static RenderStyle* styleForSelectionStart(Frame* frame, Node *&nodeToRemove)
{
    nodeToRemove = 0;

    if (frame->selection()->isNone())
        return 0;

    Position position = frame->selection()->selection().visibleStart().deepEquivalent();
    if (!position.isCandidate() || position.isNull())
        return 0;

    RefPtr<EditingStyle> typingStyle = frame->selection()->typingStyle();
    if (!typingStyle || !typingStyle->style())
        return position.deprecatedNode()->renderer()->style();

    RefPtr<Element> styleElement = frame->document()->createElement(spanTag, false);

    String styleText = typingStyle->style()->asText() + " display: inline";
    styleElement->setAttribute(styleAttr, styleText.impl());

    ExceptionCode ec = 0;
    styleElement->appendChild(frame->document()->createEditingTextNode(""), ec);
    ASSERT(!ec);

    position.deprecatedNode()->parentNode()->appendChild(styleElement, ec);
    ASSERT(!ec);

    nodeToRemove = styleElement.get();
    return styleElement->renderer() ? styleElement->renderer()->style() : 0;
}

const SimpleFontData* Editor::fontForSelection(bool& hasMultipleFonts) const
{
    hasMultipleFonts = false;

    if (!m_frame->selection()->isRange()) {
        Node* nodeToRemove;
        RenderStyle* style = styleForSelectionStart(m_frame, nodeToRemove); // sets nodeToRemove

        const SimpleFontData* result = 0;
        if (style)
            result = style->font().primaryFont();

        if (nodeToRemove) {
            ExceptionCode ec;
            nodeToRemove->remove(ec);
            ASSERT(!ec);
        }

        return result;
    }

    const SimpleFontData* font = 0;
    RefPtr<Range> range = m_frame->selection()->toNormalizedRange();
    if (Node* startNode = adjustedSelectionStartForStyleComputation(m_frame->selection()->selection()).deprecatedNode()) {
        Node* pastEnd = range->pastLastNode();
        // In the loop below, n should eventually match pastEnd and not become nil, but we've seen at least one
        // unreproducible case where this didn't happen, so check for null also.
        for (Node* node = startNode; node && node != pastEnd; node = NodeTraversal::next(node)) {
            RenderObject* renderer = node->renderer();
            if (!renderer)
                continue;
            // FIXME: Are there any node types that have renderers, but that we should be skipping?
            const SimpleFontData* primaryFont = renderer->style()->font().primaryFont();
            if (!font)
                font = primaryFont;
            else if (font != primaryFont) {
                hasMultipleFonts = true;
                break;
            }
        }
    }

    return font;
}

NSDictionary* Editor::fontAttributesForSelectionStart() const
{
    Node* nodeToRemove;
    RenderStyle* style = styleForSelectionStart(m_frame, nodeToRemove);
    if (!style)
        return nil;

    NSMutableDictionary* result = [NSMutableDictionary dictionary];
    return result;
}

void Editor::removeUnchangeableStyles()
{
    // This function removes styles that the user cannot modify by applying their default values.
    
    RefPtr<EditingStyle> editingStyle = EditingStyle::create(m_frame->document()->body());
    RefPtr<MutableStylePropertySet> defaultStyle = editingStyle.get()->style()->mutableCopy();
    
    // Text widgets implement background color via the UIView property. Their body element will not have one.
    defaultStyle->setProperty(CSSPropertyBackgroundColor, "rgba(255, 255, 255, 0.0)");
    
    // Remove properties that the user can modify, like font-weight. 
    // Also remove font-family, per HI spec.
    // FIXME: it'd be nice if knowledge about which styles were unchangeable was not hard-coded here.
    defaultStyle->removeProperty(CSSPropertyFontWeight);
    defaultStyle->removeProperty(CSSPropertyFontStyle);
    defaultStyle->removeProperty(CSSPropertyFontVariant);
    // FIXME: we should handle also pasted quoted text, strikethrough, etc. <rdar://problem/9255115>
    defaultStyle->removeProperty(CSSPropertyTextDecoration);
    defaultStyle->removeProperty(CSSPropertyWebkitTextDecorationsInEffect); // implements underline

    // FIXME add EditActionMatchStlye <rdar://problem/9156507> Undo rich text's paste & match style should say "Undo Match Style"
    applyStyleToSelection(defaultStyle.get(), EditActionChangeAttributes);
}

} // namespace WebCore

#endif // PLATFORM(IOS)
