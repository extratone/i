/**
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderTextControl.h"

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLBRElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFieldInnerElement.h"
#include "RenderTheme.h"
#include "SelectionController.h"
#include "TextIterator.h"
#import "Font.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

RenderTextControl::RenderTextControl(Node* node, bool multiLine)
    : RenderBlock(node)
    , m_dirty(false)
    , m_multiLine(multiLine)
    , m_placeholderVisible(false)
    , m_searchPopupIsVisible(false)
{
}

RenderTextControl::~RenderTextControl()
{
    if (m_multiLine && node())
        static_cast<HTMLTextAreaElement*>(node())->rendererWillBeDestroyed();
    // The children renderers have already been destroyed by destroyLeftoverChildren
    if (m_innerBlock)
        m_innerBlock->detach();
    else if (m_innerText)
        m_innerText->detach();
}
    
void RenderTextControl::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    if (m_innerBlock)
        m_innerBlock->renderer()->setStyle(createInnerBlockStyle(style));
    
    if (m_innerText) {
        RenderBlock* textBlockRenderer = static_cast<RenderBlock*>(m_innerText->renderer());
        RenderStyle* textBlockStyle = createInnerTextStyle(style);
        textBlockRenderer->setStyle(textBlockStyle);
        for (Node* n = m_innerText->firstChild(); n; n = n->traverseNextNode(m_innerText.get())) {
            if (n->renderer())
                n->renderer()->setStyle(textBlockStyle);
        }
    }
    setHasOverflowClip(false);
    setReplaced(isInline());
}

static Color disabledTextColor(const Color& textColor, const Color& backgroundColor)
{
    if (differenceSquared(textColor, Color::white) > differenceSquared(backgroundColor, Color::white))
        return textColor.light();
    return textColor.dark();
}

RenderStyle* RenderTextControl::createInnerBlockStyle(RenderStyle* startStyle)
{
    RenderStyle* innerBlockStyle = new (renderArena()) RenderStyle();
    
    innerBlockStyle->inheritFrom(startStyle);
    innerBlockStyle->setDisplay(BLOCK);
    // We don't want the shadow dom to be editable, so we set this block to read-only in case the input itself is editable.
    innerBlockStyle->setUserModify(READ_ONLY);
    
    return innerBlockStyle;
}

RenderStyle* RenderTextControl::createInnerTextStyle(RenderStyle* startStyle)
{
    RenderStyle* textBlockStyle = new (renderArena()) RenderStyle();
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    
    textBlockStyle->inheritFrom(startStyle);
    textBlockStyle->setUserModify(element->isReadOnlyControl() || element->disabled() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);
    if (m_innerBlock)
        textBlockStyle->setDisplay(INLINE_BLOCK);
    else
        textBlockStyle->setDisplay(BLOCK);
    
    if (m_multiLine) {
        // Forward overflow properties.
        textBlockStyle->setOverflowX(startStyle->overflowX() == OVISIBLE ? OAUTO : startStyle->overflowX());
        textBlockStyle->setOverflowY(startStyle->overflowY() == OVISIBLE ? OAUTO : startStyle->overflowY());
        
        // Set word wrap property based on wrap attribute
        if (static_cast<HTMLTextAreaElement*>(element)->wrap() == HTMLTextAreaElement::ta_NoWrap) {
            textBlockStyle->setWhiteSpace(PRE);
            textBlockStyle->setWordWrap(NormalWordWrap);
        } else {
            textBlockStyle->setWhiteSpace(PRE_WRAP);
            textBlockStyle->setWordWrap(BreakWordWrap);
        }
    } else {
        textBlockStyle->setWhiteSpace(PRE);
        textBlockStyle->setWordWrap(NormalWordWrap);
        textBlockStyle->setOverflowX(OHIDDEN);
        textBlockStyle->setOverflowY(OHIDDEN);
        
        // Do not allow line-height to be smaller than our default.
        if (textBlockStyle->font().lineSpacing() > lineHeight(true, true))
            textBlockStyle->setLineHeight(Length(-100.0f, Percent));
    }
    
    if (!m_multiLine) {
        // We're adding one extra pixel of padding to match WinIE.
        textBlockStyle->setPaddingLeft(Length(1, Fixed));
        textBlockStyle->setPaddingRight(Length(1, Fixed));
    } else {
        // We're adding three extra pixels of padding to line textareas up with text fields.
        textBlockStyle->setPaddingLeft(Length(3, Fixed));
        textBlockStyle->setPaddingRight(Length(3, Fixed));
    }
    
    if (!element->isEnabled())
        textBlockStyle->setColor(disabledTextColor(startStyle->color(), startStyle->backgroundColor()));
    
    return textBlockStyle;
}

RenderStyle* RenderTextControl::createDivStyle(RenderStyle* startStyle)
{
    RenderStyle* divStyle = new (renderArena()) RenderStyle();
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    
    divStyle->inheritFrom(startStyle);
    divStyle->setDisplay(BLOCK);
    divStyle->setUserModify(element->isReadOnlyControl() || element->disabled() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);

    if (m_multiLine) {
        // Forward overflow properties.
        divStyle->setOverflowX(startStyle->overflowX() == OVISIBLE ? OAUTO : startStyle->overflowX());
        divStyle->setOverflowY(startStyle->overflowY() == OVISIBLE ? OAUTO : startStyle->overflowY());

        // Set word wrap property based on wrap attribute
        if (static_cast<HTMLTextAreaElement*>(element)->wrap() == HTMLTextAreaElement::ta_NoWrap) {
            divStyle->setWhiteSpace(PRE);
            divStyle->setWordWrap(NormalWordWrap);
        } else {
            divStyle->setWhiteSpace(PRE_WRAP);
            divStyle->setWordWrap(BreakWordWrap);
        }

    } else {
        divStyle->setWhiteSpace(PRE);
        divStyle->setWordWrap(NormalWordWrap);
        divStyle->setOverflowX(OHIDDEN);
        divStyle->setOverflowY(OHIDDEN);
    }

    if (!m_multiLine) {
        // We're adding one extra pixel of padding to match WinIE.
        divStyle->setPaddingLeft(Length(1, Fixed));
        divStyle->setPaddingRight(Length(1, Fixed));
    } else {
        // We're adding three extra pixels of padding to line textareas up with text fields.
        divStyle->setPaddingLeft(Length(3, Fixed));
        divStyle->setPaddingRight(Length(3, Fixed));
    }

    if (!element->isEnabled()) {
        Color textColor = startStyle->color();
        Color disabledTextColor;
        if (differenceSquared(textColor, Color::white) > differenceSquared(startStyle->backgroundColor(), Color::white))
            disabledTextColor = textColor.light();
        else
            disabledTextColor = textColor.dark();
        divStyle->setColor(disabledTextColor);
    }

    return divStyle;
}


void RenderTextControl::updatePlaceholder()
{
    String placeholder;
    if (!m_multiLine) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
        if (input->value().isEmpty() && document()->focusNode() != node())    
            placeholder = input->getAttribute(placeholderAttr);
    }
    
    if (!placeholder.isEmpty() || m_placeholderVisible) {
        ExceptionCode ec = 0;
        m_innerText->setInnerText(placeholder, ec);
        m_placeholderVisible = !placeholder.isEmpty();
    }
    
    Color color;
    if (!placeholder.isEmpty())
        color = Color::darkGray;
    else if (node()->isEnabled())
        color = style()->color();
    else
        color = disabledTextColor(style()->color(), style()->backgroundColor());
    
    RenderObject* renderer = m_innerText->renderer();
    // fix for 5198880
    // FIXME: 5199347. This nil check doesn't exist on TOT... we should find out why
    if (!renderer)
        return;
    RenderStyle* style = renderer->style();
    if (style->color() != color) {
        style->setColor(color);
        renderer->repaint();
    }
}

void RenderTextControl::createSubtreeIfNeeded()
{
    // When adding these elements, create the renderer & style first before adding to the DOM.
    // Otherwise, the render tree will create some anonymous blocks that will mess up our layout.
    bool isSearchField = !m_multiLine && static_cast<HTMLInputElement*>(node())->isSearchField();
    if (isSearchField && !m_innerBlock) {
        // Create the inner block element and give it a parent, renderer, and style
        m_innerBlock = new HTMLTextFieldInnerElement(document(), node());
        RenderBlock* innerBlockRenderer = new (renderArena()) RenderBlock(m_innerBlock.get());
        m_innerBlock->setRenderer(innerBlockRenderer);
        m_innerBlock->setAttached();
        m_innerBlock->setInDocument(true);
        innerBlockRenderer->setStyle(createInnerBlockStyle(style()));
        
        // Add inner block renderer to Render tree
        RenderBlock::addChild(innerBlockRenderer);
    }
    if (!m_innerText) {
        // Create the text block element and give it a parent, renderer, and style
        // For non-search fields, there is no intermediate m_innerBlock as the shadow node.
        // m_innerText will be the shadow node in that case.
        m_innerText = new HTMLTextFieldInnerTextElement(document(), m_innerBlock ? 0 : node());
        RenderBlock* textBlockRenderer = new (renderArena()) RenderBlock(m_innerText.get());
        m_innerText->setRenderer(textBlockRenderer);
        m_innerText->setAttached();
        m_innerText->setInDocument(true);
        
        RenderStyle* parentStyle = style();
        if (m_innerBlock)
            parentStyle = m_innerBlock->renderer()->style();
        RenderStyle* textBlockStyle = createInnerTextStyle(parentStyle);
        textBlockRenderer->setStyle(textBlockStyle);
        
        // Add text block renderer to Render tree
        if (m_innerBlock) {
            m_innerBlock->renderer()->addChild(textBlockRenderer);
            ExceptionCode ec = 0;
            // Add text block to the DOM
            m_innerBlock->appendChild(m_innerText, ec);
        } else
            RenderBlock::addChild(textBlockRenderer);
    }
}

void RenderTextControl::updateFromElement()
{
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    
    createSubtreeIfNeeded();
    

    updatePlaceholder();
    
    m_innerText->renderer()->style()->setUserModify(element->isReadOnlyControl() || element->disabled() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);
    
    if ((!element->valueMatchesRenderer() || m_multiLine) && !m_placeholderVisible) {
        String value;
        if (m_multiLine)
            value = static_cast<HTMLTextAreaElement*>(element)->value();
        else
            value = static_cast<HTMLInputElement*>(element)->value();
        if (value.isNull())
            value = "";
        else
            value = value.replace('\\', backslashAsCurrencySymbol());
        if (value != text() || !m_innerText->hasChildNodes()) {
            ExceptionCode ec = 0;
            m_innerText->setInnerText(value, ec);
            if (value.endsWith("\n") || value.endsWith("\r"))
                m_innerText->appendChild(new HTMLBRElement(document()), ec);
            if (Frame* frame = document()->frame())
                frame->clearUndoRedoOperations();
            m_dirty = false;
        }
        element->setValueMatchesRenderer();
    }
    
}

int RenderTextControl::selectionStart()
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;
    return indexForVisiblePosition(document()->frame()->selection().start());
}

int RenderTextControl::selectionEnd()
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;
    return indexForVisiblePosition(document()->frame()->selection().end());
}

void RenderTextControl::setSelectionStart(int start)
{
    setSelectionRange(start, max(start, selectionEnd()));
}
 
void RenderTextControl::setSelectionEnd(int end)
{
    setSelectionRange(min(end, selectionStart()), end);
}
    
void RenderTextControl::select()
{
    setSelectionRange(text().length(), text().length());
}

void RenderTextControl::setSelectionRange(int start, int end)
{
    end = max(end, 0);
    start = min(max(start, 0), end);
    
    document()->updateLayout();

    if (style()->visibility() == HIDDEN) {
        if (m_multiLine)
            static_cast<HTMLTextAreaElement*>(node())->cacheSelection(start, end);
        else
            static_cast<HTMLInputElement*>(node())->cacheSelection(start, end);
        return;
    }
    VisiblePosition startPosition = visiblePositionForIndex(start);
    VisiblePosition endPosition;
    if (start == end)
        endPosition = startPosition;
    else
        endPosition = visiblePositionForIndex(end);
    Selection newSelection = Selection(startPosition, endPosition);
    document()->frame()->selection().setSelection(newSelection);

    // FIXME: Granularity is stored separately on the frame, but also in the selection controller.
    // The granularity in the selection controller should be used, and then this line of code would not be needed.
    document()->frame()->setSelectionGranularity(CharacterGranularity);
    
    document()->frame()->selectionLayoutChanged();
}

VisiblePosition RenderTextControl::visiblePositionForIndex(int index)
{    
    if (index <= 0)
        return VisiblePosition(m_innerText.get(), 0, DOWNSTREAM);
    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
    range->selectNodeContents(m_innerText.get(), ec);
    CharacterIterator it(range.get());
    it.advance(index - 1);
    return VisiblePosition(it.range()->endContainer(ec), it.range()->endOffset(ec), UPSTREAM);
}

int RenderTextControl::indexForVisiblePosition(const VisiblePosition& pos)
{
    Position indexPosition = pos.deepEquivalent();
    if (!indexPosition.node() || indexPosition.node()->rootEditableElement() != m_innerText)
        return 0;
    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
    range->setStart(m_innerText.get(), 0, ec);
    range->setEnd(indexPosition.node(), indexPosition.offset(), ec);
    return TextIterator::rangeLength(range.get());
}


void RenderTextControl::subtreeHasChanged()
{
    bool wasDirty = m_dirty;
    m_dirty = true;
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    if (m_multiLine) {
        element->setValueMatchesRenderer(false);
        if (Frame* frame = document()->frame())
            frame->textDidChangeInTextArea(element);
    } else {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(element);
        input->setValueFromRenderer(input->constrainValue(text()));
        if (!wasDirty) {
            if (Frame* frame = document()->frame())
                frame->textFieldDidBeginEditing(input);
        }
        if (Frame* frame = document()->frame())
            frame->textDidChangeInTextField(input);
    }
}

String RenderTextControl::text()
{
    if (m_innerText)
        return m_innerText->textContent().replace('\\', backslashAsCurrencySymbol());
    return String();
}

String RenderTextControl::textWithHardLineBreaks()
{
    String s("");
    
    if (!m_innerText || !m_innerText->firstChild())
        return s;
    
    document()->updateLayout();
    
    RenderObject* renderer = m_innerText->firstChild()->renderer();
    if (!renderer)
        return s;
    
    InlineBox* box = renderer->inlineBox(0, DOWNSTREAM);
    if (!box)
        return s;
    
    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
    range->selectNodeContents(m_innerText.get(), ec);
    for (RootInlineBox* line = box->root(); line; line = line->nextRootBox()) {
        // If we're at a soft wrap, then insert the hard line break here
        if (!line->endsWithBreak() && line->nextRootBox()) {
            // Update range so it ends before this wrap
            ASSERT(line->lineBreakObj());
            range->setEnd(line->lineBreakObj()->node(), line->lineBreakPos(), ec);
            
            s.append(range->toString(true, ec));
            s.append("\n");
            
            // Update range so it starts after this wrap
            range->setEnd(m_innerText.get(), maxDeepOffset(m_innerText.get()), ec);
            range->setStart(line->lineBreakObj()->node(), line->lineBreakPos(), ec);
        }
    }
    s.append(range->toString(true, ec));
    ASSERT(!ec);
    
    return s.replace('\\', backslashAsCurrencySymbol());
}

void RenderTextControl::calcHeight()
{
    int rows = 1;
    if (m_multiLine)
        rows = static_cast<HTMLTextAreaElement*>(node())->rows();
    
    int line = m_innerText->renderer()->lineHeight(true, true);
    int toAdd = paddingTop() + paddingBottom() + borderTop() + borderBottom();
    
    int innerToAdd = m_innerText->renderer()->borderTop() + m_innerText->renderer()->borderBottom() +
        m_innerText->renderer()->paddingTop() + m_innerText->renderer()->paddingBottom() +
        m_innerText->renderer()->marginTop() + m_innerText->renderer()->marginBottom();
    toAdd += innerToAdd;
    
    // FIXME: We should get the size of the scrollbar from the RenderTheme instead of hard coding it here.
    int scrollbarSize = 0;
    // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
    if (m_innerText->renderer()->style()->overflowX() == OSCROLL ||  (m_innerText->renderer()->style()->overflowX() == OAUTO && m_innerText->renderer()->style()->wordWrap() == NormalWordWrap))
        scrollbarSize = 15;
    
    m_height = line * rows + toAdd + scrollbarSize;
    
    RenderBlock::calcHeight();
}

short RenderTextControl::baselinePosition(bool b, bool isRootLineBox) const
{
    if (m_multiLine)
        return height() + marginTop() + marginBottom();
    return RenderBlock::baselinePosition(b, isRootLineBox);
}

bool RenderTextControl::nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    // If we're within the text control, we want to act as if we've hit the inner text block element, in case the point
    // was on the control but not on the inner element (see Radar 4617841).
    
    // In a search field, we want to act as if we've hit the results block if we're to the left of the inner text block,
    // and act as if we've hit the close block if we're to the right of the inner text block.
    
    if (RenderBlock::nodeAtPoint(info, x, y, tx, ty, hitTestAction)) {
        info.setInnerNode(m_innerText.get());
        return true;
    }  
    return false;
}

void RenderTextControl::layout()
{    
    int oldHeight = m_height;
    calcHeight();
    bool relayoutChildren = oldHeight != m_height;
    
    // Set the text block's height
    int textBlockHeight = m_height - paddingTop() - paddingBottom() - borderTop() - borderBottom();
    m_innerText->renderer()->style()->setHeight(Length(textBlockHeight, Fixed));
    if (m_innerBlock)
        m_innerBlock->renderer()->style()->setHeight(Length(textBlockHeight, Fixed));
    
    int oldWidth = m_width;
    calcWidth();
    if (oldWidth != m_width)
        relayoutChildren = true;
    
    int searchExtrasWidth = 0;
    // Set the text block's width
    int textBlockWidth = m_width - paddingLeft() - paddingRight() - borderLeft() - borderRight() -
        m_innerText->renderer()->paddingLeft() - m_innerText->renderer()->paddingRight() - searchExtrasWidth;
    m_innerText->renderer()->style()->setWidth(Length(textBlockWidth, Fixed));
    if (m_innerBlock)
        m_innerBlock->renderer()->style()->setWidth(Length(m_width - paddingLeft() - paddingRight() - borderLeft() - borderRight(), Fixed));
    
    RenderBlock::layoutBlock(relayoutChildren);    
}

void RenderTextControl::calcMinMaxWidth()
{
    m_minWidth = 0;
    m_maxWidth = 0;
    
    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minWidth = m_maxWidth = calcContentBoxWidth(style()->width().value());
    else {
        // Figure out how big a text control needs to be for a given number of characters
        // (using "0" as the nominal character).
        const UChar ch = '0';
        float charWidth = style()->font().floatWidth(TextRun(&ch, 1), TextStyle(0, 0, 0, false, false, false));
        int factor;
        int scrollbarSize = 0;
        if (m_multiLine) {
            factor = static_cast<HTMLTextAreaElement*>(node())->cols();
            // FIXME: We should get the size of the scrollbar from the RenderTheme instead of hard coding it here.
            if (m_innerText->renderer()->style()->overflowY() != OHIDDEN)
                scrollbarSize = 15;
        } else {
            factor = static_cast<HTMLInputElement*>(node())->size();
            if (factor <= 0)
                factor = 20;
        }
        m_maxWidth = static_cast<int>(ceilf(charWidth * factor)) + scrollbarSize;
    }
    
    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxWidth = max(m_maxWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minWidth = max(m_minWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minWidth = 0;
    else
        m_minWidth = m_maxWidth;
    
    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxWidth = min(m_maxWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minWidth = min(m_minWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }
    
    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight() +
        m_innerText->renderer()->paddingLeft() + m_innerText->renderer()->paddingRight();
    m_minWidth += toAdd;
    m_maxWidth += toAdd;
    
    setMinMaxKnown();
}

void RenderTextControl::forwardEvent(Event* evt)
{
    if (evt->type() == blurEvent) {
        RenderObject* innerRenderer = m_innerText->renderer();
        if (innerRenderer) {
            RenderLayer* innerLayer = innerRenderer->layer();
            if (innerLayer && !m_multiLine)
                innerLayer->scrollToOffset(style()->direction() == RTL ? innerLayer->scrollWidth() : 0, 0);
        }
        updatePlaceholder();
    } else if (evt->type() == focusEvent)
        updatePlaceholder();
    else {
            m_innerText->defaultEventHandler(evt);
    }
}

void RenderTextControl::selectionChanged(bool userTriggered)
{
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    if (m_multiLine)
        static_cast<HTMLTextAreaElement*>(element)->cacheSelection(selectionStart(), selectionEnd());
    else
        static_cast<HTMLInputElement*>(element)->cacheSelection(selectionStart(), selectionEnd());
    if (Frame* frame = document()->frame())
        if (frame->selection().isRange() && userTriggered)
            element->dispatchHTMLEvent(selectEvent, true, false);
}

int RenderTextControl::scrollWidth() const
{
    if (m_innerText)
        return m_innerText->scrollWidth();
    return RenderBlock::scrollWidth();
}

int RenderTextControl::scrollHeight() const
{
    if (m_innerText)
        return m_innerText->scrollHeight();
    return RenderBlock::scrollHeight();
}

int RenderTextControl::scrollLeft() const
{
    if (m_innerText)
        return m_innerText->scrollLeft();
    return RenderBlock::scrollLeft();
}

int RenderTextControl::scrollTop() const
{
    if (m_innerText)
        return m_innerText->scrollTop();
    return RenderBlock::scrollTop();
}

void RenderTextControl::setScrollLeft(int newLeft)
{
    if (m_innerText)
        m_innerText->setScrollLeft(newLeft);
}

void RenderTextControl::setScrollTop(int newTop)
{
    if (m_innerText)
        m_innerText->setScrollTop(newTop);
}

bool RenderTextControl::canScroll() const
{
    return m_innerText && m_innerText->renderer() && m_innerText->renderer()->hasOverflowClip();
}

short RenderTextControl::innerLineHeight() const
{
    if (m_innerText && m_innerText->renderer() && m_innerText->renderer()->style())
        return m_innerText->renderer()->lineHeight(false);
    return RenderBlock::lineHeight(false);
}

}
