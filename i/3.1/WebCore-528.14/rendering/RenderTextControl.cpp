/**
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)  
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderTextControl.h"

#include "CharacterNames.h"
#include "Editor.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLBRElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "RenderText.h"
#include "ScrollbarTheme.h"
#include "SelectionController.h"
#include "TextControlInnerElements.h"
#include "Text.h"
#include "TextIterator.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// Value chosen by observation.  This can be tweaked.
static const int minColorContrastValue = 1300;

static Color disabledTextColor(const Color& textColor, const Color& backgroundColor)
{
    // The explicit check for black is an optimization for the 99% case (black on white).
    // This also means that black on black will turn into grey on black when disabled.
    Color disabledColor;
    if (textColor.rgb() == Color::black || differenceSquared(textColor, Color::white) > differenceSquared(backgroundColor, Color::white))
        disabledColor = textColor.light();
    else
        disabledColor = textColor.dark();
    
    // If there's not very much contrast between the disabled color and the background color,
    // just leave the text color alone.  We don't want to change a good contrast color scheme so that it has really bad contrast.
    // If the the contrast was already poor, then it doesn't do any good to change it to a different poor contrast color scheme.
    if (differenceSquared(disabledColor, backgroundColor) < minColorContrastValue)
        return textColor;
    
    return disabledColor;
}

RenderTextControl::RenderTextControl(Node* node)
    : RenderBlock(node)
    , m_edited(false)
    , m_userEdited(false)
{
}

RenderTextControl::~RenderTextControl()
{
    // The children renderers have already been destroyed by destroyLeftoverChildren
    if (m_innerText)
        m_innerText->detach();
}

void RenderTextControl::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (m_innerText) {
        RenderBlock* textBlockRenderer = static_cast<RenderBlock*>(m_innerText->renderer());
        RefPtr<RenderStyle> textBlockStyle = createInnerTextStyle(style());
        // We may have set the width and the height in the old style in layout().
        // Reset them now to avoid getting a spurious layout hint.
        textBlockRenderer->style()->setHeight(Length());
        textBlockRenderer->style()->setWidth(Length());
        textBlockRenderer->setStyle(textBlockStyle);
        for (Node* n = m_innerText->firstChild(); n; n = n->traverseNextNode(m_innerText.get())) {
            if (n->renderer())
                n->renderer()->setStyle(textBlockStyle);
        }
    }

    setHasOverflowClip(false);
    setReplaced(isInline());
}

void RenderTextControl::adjustInnerTextStyle(const RenderStyle* startStyle, RenderStyle* textBlockStyle) const
{
    // The inner block, if present, always has its direction set to LTR,
    // so we need to inherit the direction from the element.
    textBlockStyle->setDirection(style()->direction());
    textBlockStyle->setUserModify((node()->isReadOnlyControl() || !node()->isEnabled()) ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);

    if (!node()->isEnabled())
        textBlockStyle->setColor(disabledTextColor(textBlockStyle->color(), startStyle->backgroundColor()));
}

void RenderTextControl::createSubtreeIfNeeded(TextControlInnerElement* innerBlock)
{
    if (!m_innerText) {
        // Create the text block element
        // For non-search fields, there is no intermediate innerBlock as the shadow node.
        // m_innerText will be the shadow node in that case.        
        RenderStyle* parentStyle = innerBlock ? innerBlock->renderer()->style() : style();
        m_innerText = new TextControlInnerTextElement(document(), innerBlock ? 0 : node());
        m_innerText->attachInnerElement(innerBlock ? innerBlock : node(), createInnerTextStyle(parentStyle), renderArena());
    }
}

int RenderTextControl::textBlockHeight() const
{
    return height() - paddingTop() - paddingBottom() - borderTop() - borderBottom();
}

int RenderTextControl::textBlockWidth() const
{
    return width() - paddingLeft() - paddingRight() - borderLeft() - borderRight()
           - m_innerText->renderBox()->paddingLeft() - m_innerText->renderBox()->paddingRight();
}

void RenderTextControl::updateFromElement()
{
    m_innerText->renderer()->style()->setUserModify((node()->isReadOnlyControl() || !node()->isEnabled()) ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY); 
}

void RenderTextControl::setInnerTextValue(const String& innerTextValue)
{
    String value;

    if (innerTextValue.isNull())
        value = "";
    else {
        value = innerTextValue; 
        value = document()->displayStringModifiedByEncoding(value);
    }

    if (value != text() || !m_innerText->hasChildNodes()) {
        if (value != text()) {
            if (Frame* frame = document()->frame())
                frame->editor()->clearUndoRedoOperations();
        }

        ExceptionCode ec = 0;
        m_innerText->setInnerText(value, ec);
        ASSERT(!ec);

        if (value.endsWith("\n") || value.endsWith("\r")) {
            m_innerText->appendChild(new HTMLBRElement(brTag, document()), ec);
            ASSERT(!ec);
        }

        m_edited = false;
        m_userEdited = false;
    }

    formControlElement()->setValueMatchesRenderer();
}

void RenderTextControl::setUserEdited(bool isUserEdited)
{
    m_userEdited = isUserEdited;
    document()->setIgnoreAutofocus(isUserEdited);
}

int RenderTextControl::selectionStart()
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;
    return indexForVisiblePosition(frame->selection()->start());
}

int RenderTextControl::selectionEnd()
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;
    return indexForVisiblePosition(frame->selection()->end());
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
    // We don't want to select all the text in iPhone, instead use the standard textfield behavior of going to the end of the line.
    setSelectionRange(text().length(), text().length());
}

void RenderTextControl::setSelectionRange(int start, int end)
{
    end = max(end, 0);
    start = min(max(start, 0), end);

    document()->updateLayout();

    if (style()->visibility() == HIDDEN || !m_innerText || !m_innerText->renderer() || !m_innerText->renderBox()->height()) {
        cacheSelection(start, end);
        return;
    }
    VisiblePosition startPosition = visiblePositionForIndex(start);
    VisiblePosition endPosition;
    if (start == end)
        endPosition = startPosition;
    else
        endPosition = visiblePositionForIndex(end);


    Selection newSelection = Selection(startPosition, endPosition);

    if (Frame* frame = document()->frame())
        frame->selection()->setSelection(newSelection);

    // FIXME: Granularity is stored separately on the frame, but also in the selection controller.
    // The granularity in the selection controller should be used, and then this line of code would not be needed.
    if (Frame* frame = document()->frame())
        frame->setSelectionGranularity(CharacterGranularity);
}

Selection RenderTextControl::selection(int start, int end) const
{
    return Selection(VisiblePosition(m_innerText.get(), start, VP_DEFAULT_AFFINITY),
                     VisiblePosition(m_innerText.get(), end, VP_DEFAULT_AFFINITY));
}

VisiblePosition RenderTextControl::visiblePositionForIndex(int index)
{
    if (index <= 0)
        return VisiblePosition(m_innerText.get(), 0, DOWNSTREAM);
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(document());
    range->selectNodeContents(m_innerText.get(), ec);
    ASSERT(!ec);
    CharacterIterator it(range.get());
    it.advance(index - 1);
    Node* endContainer = it.range()->endContainer(ec);
    ASSERT(!ec);
    int endOffset = it.range()->endOffset(ec);
    ASSERT(!ec);
    return VisiblePosition(endContainer, endOffset, UPSTREAM);
}

int RenderTextControl::indexForVisiblePosition(const VisiblePosition& pos)
{
    Position indexPosition = pos.deepEquivalent();
    if (!indexPosition.node() || indexPosition.node()->rootEditableElement() != m_innerText)
        return 0;
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(document());
    range->setStart(m_innerText.get(), 0, ec);
    ASSERT(!ec);
    range->setEnd(indexPosition.node(), indexPosition.offset(), ec);
    ASSERT(!ec);
    return TextIterator::rangeLength(range.get());
}

void RenderTextControl::subtreeHasChanged()
{
    m_edited = true;
    m_userEdited = true;
}

String RenderTextControl::finishText(Vector<UChar>& result) const
{
    // Remove one trailing newline; there's always one that's collapsed out by rendering.
    size_t size = result.size();
    if (size && result[size - 1] == '\n')
        result.shrink(--size);

    // Convert backslash to currency symbol.
    document()->displayBufferModifiedByEncoding(result.data(), result.size());
    
    return String::adopt(result);
}

String RenderTextControl::text()
{
    if (!m_innerText)
        return "";
 
    Frame* frame = document()->frame();
    Text* compositionNode = frame ? frame->editor()->compositionNode() : 0;

    Vector<UChar> result;

    for (Node* n = m_innerText.get(); n; n = n->traverseNextNode(m_innerText.get())) {
        if (n->hasTagName(brTag))
            result.append(&newlineCharacter, 1);
        else if (n->isTextNode()) {
            Text* text = static_cast<Text*>(n);
            String data = text->data();
            unsigned length = data.length();
            if (text != compositionNode)
                result.append(data.characters(), length);
            else {
                unsigned compositionStart = min(frame->editor()->compositionStart(), length);
                unsigned compositionEnd = min(max(compositionStart, frame->editor()->compositionEnd()), length);
                result.append(data.characters(), compositionStart);
                result.append(data.characters() + compositionEnd, length - compositionEnd);
            }
        }
    }

    return finishText(result);
}

static void getNextSoftBreak(RootInlineBox*& line, Node*& breakNode, unsigned& breakOffset)
{
    RootInlineBox* next;
    for (; line; line = next) {
        next = line->nextRootBox();
        if (next && !line->endsWithBreak()) {
            ASSERT(line->lineBreakObj());
            breakNode = line->lineBreakObj()->node();
            breakOffset = line->lineBreakPos();
            line = next;
            return;
        }
    }
    breakNode = 0;
}

String RenderTextControl::textWithHardLineBreaks()
{
    if (!m_innerText)
        return "";
    Node* firstChild = m_innerText->firstChild();
    if (!firstChild)
        return "";

    document()->updateLayout();

    RenderObject* renderer = firstChild->renderer();
    if (!renderer)
        return "";

    InlineBox* box = renderer->isText() ? toRenderText(renderer)->firstTextBox() : toRenderBox(renderer)->inlineBoxWrapper();
    if (!box)
        return "";

    Frame* frame = document()->frame();
    Text* compositionNode = frame ? frame->editor()->compositionNode() : 0;

    Node* breakNode;
    unsigned breakOffset;
    RootInlineBox* line = box->root();
    getNextSoftBreak(line, breakNode, breakOffset);

    Vector<UChar> result;

    for (Node* n = firstChild; n; n = n->traverseNextNode(m_innerText.get())) {
        if (n->hasTagName(brTag))
            result.append(&newlineCharacter, 1);
        else if (n->isTextNode()) {
            Text* text = static_cast<Text*>(n);
            String data = text->data();
            unsigned length = data.length();
            unsigned compositionStart = (text == compositionNode)
                ? min(frame->editor()->compositionStart(), length) : 0;
            unsigned compositionEnd = (text == compositionNode)
                ? min(max(compositionStart, frame->editor()->compositionEnd()), length) : 0;
            unsigned position = 0;
            while (breakNode == n && breakOffset < compositionStart) {
                result.append(data.characters() + position, breakOffset - position);
                position = breakOffset;
                result.append(&newlineCharacter, 1);
                getNextSoftBreak(line, breakNode, breakOffset);
            }
            result.append(data.characters() + position, compositionStart - position);
            position = compositionEnd;
            while (breakNode == n && breakOffset <= length) {
                if (breakOffset > position) {
                    result.append(data.characters() + position, breakOffset - position);
                    position = breakOffset;
                    result.append(&newlineCharacter, 1);
                }
                getNextSoftBreak(line, breakNode, breakOffset);
            }
            result.append(data.characters() + position, length - position);
        }
        while (breakNode == n)
            getNextSoftBreak(line, breakNode, breakOffset);
    }

    return finishText(result);
}

int RenderTextControl::scrollbarThickness() const
{
    // FIXME: We should get the size of the scrollbar from the RenderTheme instead.
    return ScrollbarTheme::nativeTheme()->scrollbarThickness();
}

void RenderTextControl::calcHeight()
{
    setHeight(m_innerText->renderBox()->borderTop() + m_innerText->renderBox()->borderBottom() +
              m_innerText->renderBox()->paddingTop() + m_innerText->renderBox()->paddingBottom() +
              m_innerText->renderBox()->marginTop() + m_innerText->renderBox()->marginBottom());

    adjustControlHeightBasedOnLineHeight(m_innerText->renderer()->lineHeight(true, true));
    setHeight(height() + paddingTop() + paddingBottom() + borderTop() + borderBottom());

    // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
    if (m_innerText->renderer()->style()->overflowX() == OSCROLL ||  (m_innerText->renderer()->style()->overflowX() == OAUTO && m_innerText->renderer()->style()->wordWrap() == NormalWordWrap))
        setHeight(height() + scrollbarThickness());

    RenderBlock::calcHeight();
}

void RenderTextControl::hitInnerTextElement(HitTestResult& result, int xPos, int yPos, int tx, int ty)
{
    result.setInnerNode(m_innerText.get());
    result.setLocalPoint(IntPoint(xPos - tx - x() - m_innerText->renderBox()->x(),
                                  yPos - ty - y() - m_innerText->renderBox()->y()));
}

void RenderTextControl::forwardEvent(Event* event)
{
    if (event->type() == eventNames().blurEvent || event->type() == eventNames().focusEvent)
        return;
    m_innerText->defaultEventHandler(event);
}

IntRect RenderTextControl::controlClipRect(int tx, int ty) const
{
    IntRect clipRect = contentBoxRect();
    clipRect.move(tx, ty);
    return clipRect;
}

void RenderTextControl::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    m_minPrefWidth = 0;
    m_maxPrefWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPrefWidth = m_maxPrefWidth = calcContentBoxWidth(style()->width().value());
    else {
        // Figure out how big a text control needs to be for a given number of characters
        // (using "0" as the nominal character).
        const UChar ch = '0';
        float charWidth = style()->font().floatWidth(TextRun(&ch, 1, false, 0, 0, false, false, false));
        m_maxPrefWidth = preferredContentWidth(charWidth) + m_innerText->renderBox()->paddingLeft() + m_innerText->renderBox()->paddingRight();
    }

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPrefWidth = max(m_maxPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minPrefWidth = max(m_minPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPrefWidth = min(m_maxPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minPrefWidth = min(m_minPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight();

    m_minPrefWidth += toAdd;
    m_maxPrefWidth += toAdd;

    setPrefWidthsDirty(false);
}

void RenderTextControl::selectionChanged(bool userTriggered)
{
    cacheSelection(selectionStart(), selectionEnd());

    if (Frame* frame = document()->frame()) {
        if (frame->selection()->isRange() && userTriggered)
            static_cast<EventTargetNode*>(node())->dispatchEventForType(eventNames().selectEvent, true, false);
    }
}

void RenderTextControl::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    graphicsContext->addFocusRingRect(IntRect(tx, ty, width(), height()));
}

void RenderTextControl::autoscroll()
{
    RenderLayer* layer = m_innerText->renderBox()->layer();
    if (layer)
        layer->autoscroll();
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

bool RenderTextControl::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    RenderLayer* layer = m_innerText->renderBox()->layer();
    if (layer && layer->scroll(direction, granularity, multiplier))
        return true;
    return RenderBlock::scroll(direction, granularity, multiplier);
}

bool RenderTextControl::canScroll() const
{
    return m_innerText && m_innerText->renderer() && m_innerText->renderer()->hasOverflowClip();
}

int RenderTextControl::innerLineHeight() const
{
    if (m_innerText && m_innerText->renderer() && m_innerText->renderer()->style())
        return m_innerText->renderer()->lineHeight(false);
    return RenderBlock::lineHeight(false);
}

HTMLElement* RenderTextControl::innerTextElement() const
{
    return m_innerText.get();
}

FormControlElement* RenderTextControl::formControlElement() const
{
    return toFormControlElement(static_cast<Element*>(node()));
}

} // namespace WebCore
