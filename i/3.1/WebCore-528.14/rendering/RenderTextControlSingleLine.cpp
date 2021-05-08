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
#include "RenderTextControlSingleLine.h"

#include "CSSStyleSelector.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InputElement.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "RenderScrollbar.h"
#include "RenderTheme.h"
#include "SearchPopupMenu.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TextControlInnerElements.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderTextControlSingleLine::RenderTextControlSingleLine(Node* node)
    : RenderTextControl(node)
    , m_placeholderVisible(false)
    , m_searchPopupIsVisible(false)
    , m_shouldDrawCapsLockIndicator(false)
    , m_searchEventTimer(this, &RenderTextControlSingleLine::searchEventTimerFired)
    , m_searchPopup(0)
{
}

RenderTextControlSingleLine::~RenderTextControlSingleLine()
{
    if (m_searchPopup) {
        m_searchPopup->disconnectClient();
        m_searchPopup = 0;
    }
 
    if (m_innerBlock)
        m_innerBlock->detach();
}

bool RenderTextControlSingleLine::placeholderShouldBeVisible() const
{
    return inputElement()->placeholderShouldBeVisible();
}

void RenderTextControlSingleLine::updatePlaceholderVisibility()
{
    RenderStyle* parentStyle = m_innerBlock ? m_innerBlock->renderer()->style() : style();

    RefPtr<RenderStyle> textBlockStyle = createInnerTextStyle(parentStyle);
    HTMLElement* innerText = innerTextElement();
    innerText->renderer()->setStyle(textBlockStyle);

    for (Node* n = innerText->firstChild(); n; n = n->traverseNextNode(innerText)) {
        if (RenderObject* renderer = n->renderer())
            renderer->setStyle(textBlockStyle);
    }

    updateFromElement();
}

void RenderTextControlSingleLine::addSearchResult()
{
    ASSERT(node()->isHTMLElement());
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    if (input->maxResults() <= 0)
        return;

    String value = input->value();
    if (value.isEmpty())
        return;

    Settings* settings = document()->settings();
    if (!settings || settings->privateBrowsingEnabled())
        return;

    int size = static_cast<int>(m_recentSearches.size());
    for (int i = size - 1; i >= 0; --i) {
        if (m_recentSearches[i] == value)
            m_recentSearches.remove(i);
    }

    m_recentSearches.insert(0, value);
    while (static_cast<int>(m_recentSearches.size()) > input->maxResults())
        m_recentSearches.removeLast();

    const AtomicString& name = autosaveName();
    if (!m_searchPopup)
        m_searchPopup = SearchPopupMenu::create(this);

    m_searchPopup->saveRecentSearches(name, m_recentSearches);
}

void RenderTextControlSingleLine::stopSearchEventTimer()
{
    ASSERT(node()->isHTMLElement());
    m_searchEventTimer.stop();
}

void RenderTextControlSingleLine::showPopup()
{
    ASSERT(node()->isHTMLElement());
    if (m_searchPopupIsVisible)
        return;

    if (!m_searchPopup)
        m_searchPopup = SearchPopupMenu::create(this);

    if (!m_searchPopup->enabled())
        return;

    m_searchPopupIsVisible = true;

    const AtomicString& name = autosaveName();
    m_searchPopup->loadRecentSearches(name, m_recentSearches);

    // Trim the recent searches list if the maximum size has changed since we last saved.
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    if (static_cast<int>(m_recentSearches.size()) > input->maxResults()) {
        do {
            m_recentSearches.removeLast();
        } while (static_cast<int>(m_recentSearches.size()) > input->maxResults());

        m_searchPopup->saveRecentSearches(name, m_recentSearches);
    }

    m_searchPopup->show(absoluteBoundingBoxRect(true), document()->view(), -1);
}

void RenderTextControlSingleLine::hidePopup()
{
    ASSERT(node()->isHTMLElement());
    if (m_searchPopup)
        m_searchPopup->hide();

    m_searchPopupIsVisible = false;
}

void RenderTextControlSingleLine::subtreeHasChanged()
{
    bool wasEdited = isEdited();
    RenderTextControl::subtreeHasChanged();

    InputElement* input = inputElement();
    input->setValueFromRenderer(input->constrainValue(text()));

    if (m_cancelButton)
        updateCancelButtonVisibility();

    // If the incremental attribute is set, then dispatch the search event
    if (input->searchEventsShouldBeDispatched())
        startSearchEventTimer();

    if (!wasEdited && node()->focused()) {
        if (Frame* frame = document()->frame())
            frame->textFieldDidBeginEditing(static_cast<Element*>(node()));
    }

    if (node()->focused()) {
        if (Frame* frame = document()->frame())
            frame->textDidChangeInTextField(static_cast<Element*>(node()));
    }
}

void RenderTextControlSingleLine::paint(PaintInfo& paintInfo, int tx, int ty)
{
    RenderTextControl::paint(paintInfo, tx, ty);

    if (paintInfo.phase == PaintPhaseBlockBackground && m_shouldDrawCapsLockIndicator) {
        IntRect contentsRect = contentBoxRect();

        // Convert the rect into the coords used for painting the content
        contentsRect.move(tx + x(), ty + y());
        theme()->paintCapsLockIndicator(this, paintInfo, contentsRect);
    }
}

void RenderTextControlSingleLine::layout()
{
    int oldHeight = height();
    calcHeight();

    int oldWidth = width();
    calcWidth();

    bool relayoutChildren = oldHeight != height() || oldWidth != width();

    RenderBox* innerTextRenderer = innerTextElement()->renderBox();
    RenderBox* innerBlockRenderer = m_innerBlock ? m_innerBlock->renderBox() : 0;

    // Set the text block height
    int desiredHeight = textBlockHeight();
    int currentHeight = innerTextRenderer->height();

    if (m_innerBlock || currentHeight > height()) {
        if (desiredHeight != currentHeight)
            relayoutChildren = true;
        innerTextRenderer->style()->setHeight(Length(desiredHeight, Fixed));
    }

    if (m_innerBlock) {
        ASSERT(innerBlockRenderer);
        if (desiredHeight != innerBlockRenderer->height())
            relayoutChildren = true;
        innerBlockRenderer->style()->setHeight(Length(desiredHeight, Fixed));
    }

    // Set the text block width
    int desiredWidth = textBlockWidth();
    if (desiredWidth != innerTextRenderer->width())
        relayoutChildren = true;
    innerTextRenderer->style()->setWidth(Length(desiredWidth, Fixed));

    if (m_innerBlock) {
        int innerBlockWidth = width() - paddingLeft() - paddingRight() - borderLeft() - borderRight();
        if (innerBlockWidth != innerBlockRenderer->width())
            relayoutChildren = true;
        innerBlockRenderer->style()->setWidth(Length(innerBlockWidth, Fixed));
    }

    RenderBlock::layoutBlock(relayoutChildren);

    // For text fields, center the inner text vertically
    // Don't do this for search fields, since we don't honor height for them
    if (!m_innerBlock) {
        currentHeight = innerTextRenderer->height();
        if (currentHeight < height())
            innerTextRenderer->setLocation(innerTextRenderer->x(), (height() - currentHeight) / 2);
    }
}

bool RenderTextControlSingleLine::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int xPos, int yPos, int tx, int ty, HitTestAction hitTestAction)
{
    // If we're within the text control, we want to act as if we've hit the inner text block element, in case the point
    // was on the control but not on the inner element (see Radar 4617841).

    // In a search field, we want to act as if we've hit the results block if we're to the left of the inner text block,
    // and act as if we've hit the close block if we're to the right of the inner text block.

    if (!RenderTextControl::nodeAtPoint(request, result, xPos, yPos, tx, ty, hitTestAction))
        return false;

    // If we hit a node inside the inner text element, say that we hit that element,
    // and if we hit our node (e.g. we're over the border or padding), also say that we hit the
    // inner text element so that it gains focus.
    if (result.innerNode()->isDescendantOf(innerTextElement()) || result.innerNode() == element())
        hitInnerTextElement(result, xPos, yPos, tx, ty);

    // If we're not a search field, or we already found the results or cancel buttons, we're done.
    if (!m_innerBlock || result.innerNode() == m_resultsButton || result.innerNode() == m_cancelButton)
        return true;

    Node* innerNode = 0;
    RenderBox* innerBlockRenderer = m_innerBlock->renderBox();
    RenderBox* innerTextRenderer = innerTextElement()->renderBox();

    IntPoint localPoint = result.localPoint();
    localPoint.move(-innerBlockRenderer->x(), -innerBlockRenderer->y());

    int textLeft = tx + x() + innerBlockRenderer->x() + innerTextRenderer->x();
    if (m_resultsButton && m_resultsButton->renderer() && xPos < textLeft)
        innerNode = m_resultsButton.get();

    if (!innerNode) {
        int textRight = textLeft + innerTextRenderer->width();
        if (m_cancelButton && m_cancelButton->renderer() && xPos > textRight)
            innerNode = m_cancelButton.get();
    }

    if (innerNode) {
        result.setInnerNode(innerNode);
        localPoint.move(-innerNode->renderBox()->x(), -innerNode->renderBox()->y());
    }

    result.setLocalPoint(localPoint);
    return true;
}

void RenderTextControlSingleLine::forwardEvent(Event* event)
{
    RenderBox* innerTextRenderer = innerTextElement()->renderBox();

    if (event->type() == eventNames().blurEvent) {
        if (innerTextRenderer) {
            if (RenderLayer* innerLayer = innerTextRenderer->layer())
                innerLayer->scrollToOffset(style()->direction() == RTL ? innerLayer->scrollWidth() : 0, 0);
        }

        capsLockStateMayHaveChanged();
    } else if (event->type() == eventNames().focusEvent)
        capsLockStateMayHaveChanged();

    if (!event->isMouseEvent()) {
        RenderTextControl::forwardEvent(event);
        return;
    }

    FloatPoint localPoint = innerTextRenderer->absoluteToLocal(FloatPoint(static_cast<MouseEvent*>(event)->pageX(), static_cast<MouseEvent*>(event)->pageY()), false, true);
    if (m_resultsButton && localPoint.x() < innerTextRenderer->borderBoxRect().x())
        m_resultsButton->defaultEventHandler(event);
    else if (m_cancelButton && localPoint.x() > innerTextRenderer->borderBoxRect().right())
        m_cancelButton->defaultEventHandler(event);
    else
        RenderTextControl::forwardEvent(event);
}

void RenderTextControlSingleLine::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderTextControl::styleDidChange(diff, oldStyle);

    if (RenderObject* innerBlockRenderer = m_innerBlock ? m_innerBlock->renderer() : 0) {
        // We may have set the width and the height in the old style in layout().
        // Reset them now to avoid getting a spurious layout hint.
        innerBlockRenderer->style()->setHeight(Length());
        innerBlockRenderer->style()->setWidth(Length());
        innerBlockRenderer->setStyle(createInnerBlockStyle(style()));
    }

    if (RenderObject* resultsRenderer = m_resultsButton ? m_resultsButton->renderer() : 0)
        resultsRenderer->setStyle(createResultsButtonStyle(style()));

    if (RenderObject* cancelRenderer = m_cancelButton ? m_cancelButton->renderer() : 0)
        cancelRenderer->setStyle(createCancelButtonStyle(style()));
}

void RenderTextControlSingleLine::capsLockStateMayHaveChanged()
{
    if (!node() || !document())
        return;

    // Only draw the caps lock indicator if these things are true:
    // 1) The field is a password field
    // 2) The frame is active
    // 3) The element is focused
    // 4) The caps lock is on
    bool shouldDrawCapsLockIndicator = false;

    if (Frame* frame = document()->frame())
        shouldDrawCapsLockIndicator = inputElement()->isPasswordField()
                                      && frame->selection()->isFocusedAndActive()
                                      && document()->focusedNode() == node()
                                      && PlatformKeyboardEvent::currentCapsLockState();

    if (shouldDrawCapsLockIndicator != m_shouldDrawCapsLockIndicator) {
        m_shouldDrawCapsLockIndicator = shouldDrawCapsLockIndicator;
        repaint();
    }
}

int RenderTextControlSingleLine::textBlockWidth() const
{
    int width = RenderTextControl::textBlockWidth();

    if (RenderBox* resultsRenderer = m_resultsButton ? m_resultsButton->renderBox() : 0) {
        resultsRenderer->calcWidth();
        width -= resultsRenderer->width() + resultsRenderer->marginLeft() + resultsRenderer->marginRight();
    }

    if (RenderBox* cancelRenderer = m_cancelButton ? m_cancelButton->renderBox() : 0) {
        cancelRenderer->calcWidth();
        width -= cancelRenderer->width() + cancelRenderer->marginLeft() + cancelRenderer->marginRight();
    }

    return width;
}

int RenderTextControlSingleLine::preferredContentWidth(float charWidth) const
{
    int factor = inputElement()->size();
    if (factor <= 0)
        factor = 20;

    int result = static_cast<int>(ceilf(charWidth * factor));

    if (RenderBox* resultsRenderer = m_resultsButton ? m_resultsButton->renderBox() : 0)
        result += resultsRenderer->borderLeft() + resultsRenderer->borderRight() +
                  resultsRenderer->paddingLeft() + resultsRenderer->paddingRight();

    if (RenderBox* cancelRenderer = m_cancelButton ? m_cancelButton->renderBox() : 0)
        result += cancelRenderer->borderLeft() + cancelRenderer->borderRight() +
                  cancelRenderer->paddingLeft() + cancelRenderer->paddingRight();

    return result;
}

void RenderTextControlSingleLine::adjustControlHeightBasedOnLineHeight(int lineHeight)
{
    if (RenderBox* resultsRenderer = m_resultsButton ? m_resultsButton->renderBox() : 0) {
        static_cast<RenderBlock*>(resultsRenderer)->calcHeight();
        setHeight(max(height(),
                  resultsRenderer->borderTop() + resultsRenderer->borderBottom() +
                  resultsRenderer->paddingTop() + resultsRenderer->paddingBottom() +
                  resultsRenderer->marginTop() + resultsRenderer->marginBottom()));
        lineHeight = max(lineHeight, resultsRenderer->height());
    }

    if (RenderBox* cancelRenderer = m_cancelButton ? m_cancelButton->renderBox() : 0) {
        static_cast<RenderBlock*>(cancelRenderer)->calcHeight();
        setHeight(max(height(),
                  cancelRenderer->borderTop() + cancelRenderer->borderBottom() +
                  cancelRenderer->paddingTop() + cancelRenderer->paddingBottom() +
                  cancelRenderer->marginTop() + cancelRenderer->marginBottom()));
        lineHeight = max(lineHeight, cancelRenderer->height());
    }

    setHeight(height() + lineHeight);
}

void RenderTextControlSingleLine::createSubtreeIfNeeded()
{
    if (!inputElement()->isSearchField()) {
        RenderTextControl::createSubtreeIfNeeded(m_innerBlock.get());
        return;
    }

    if (!m_innerBlock) {
        // Create the inner block element
        m_innerBlock = new TextControlInnerElement(document(), node());
        m_innerBlock->attachInnerElement(node(), createInnerBlockStyle(style()), renderArena());
    }

    if (!m_resultsButton) {
        // Create the search results button element
        m_resultsButton = new SearchFieldResultsButtonElement(document());
        m_resultsButton->attachInnerElement(m_innerBlock.get(), createResultsButtonStyle(m_innerBlock->renderer()->style()), renderArena());
    }

    // Create innerText element before adding the cancel button
    RenderTextControl::createSubtreeIfNeeded(m_innerBlock.get());

    if (!m_cancelButton) {
        // Create the cancel button element
        m_cancelButton = new SearchFieldCancelButtonElement(document());
        m_cancelButton->attachInnerElement(m_innerBlock.get(), createCancelButtonStyle(m_innerBlock->renderer()->style()), renderArena());
    }
}

void RenderTextControlSingleLine::updateFromElement()
{
    createSubtreeIfNeeded();
    RenderTextControl::updateFromElement();

    bool placeholderVisibilityShouldChange = m_placeholderVisible != placeholderShouldBeVisible();
    m_placeholderVisible = placeholderShouldBeVisible();

    if (m_cancelButton)
        updateCancelButtonVisibility();

    if (m_placeholderVisible) {
        ExceptionCode ec = 0;
        innerTextElement()->setInnerText(inputElement()->placeholderValue(), ec);
        ASSERT(!ec);
    } else if (!formControlElement()->valueMatchesRenderer() || placeholderVisibilityShouldChange)
        setInnerTextValue(inputElement()->value());

    if (m_searchPopupIsVisible)
        m_searchPopup->updateFromElement();
}

void RenderTextControlSingleLine::cacheSelection(int start, int end)
{
    inputElement()->cacheSelection(start, end);
}

PassRefPtr<RenderStyle> RenderTextControlSingleLine::createInnerTextStyle(const RenderStyle* startStyle) const
{
    RefPtr<RenderStyle> textBlockStyle;
    if (placeholderShouldBeVisible()) {
        RenderStyle* pseudoStyle = getCachedPseudoStyle(RenderStyle::INPUT_PLACEHOLDER);
        textBlockStyle = RenderStyle::clone(pseudoStyle);
    } else {
        textBlockStyle = RenderStyle::create();   
        textBlockStyle->inheritFrom(startStyle);
    }

    adjustInnerTextStyle(startStyle, textBlockStyle.get());

    textBlockStyle->setWhiteSpace(PRE);
    textBlockStyle->setWordWrap(NormalWordWrap);
    textBlockStyle->setOverflowX(OHIDDEN);
    textBlockStyle->setOverflowY(OHIDDEN);

    // Do not allow line-height to be smaller than our default.
    if (textBlockStyle->font().lineSpacing() > lineHeight(true, true))
        textBlockStyle->setLineHeight(Length(-100.0f, Percent));

    textBlockStyle->setDisplay(m_innerBlock ? INLINE_BLOCK : BLOCK);

    // We're adding one extra pixel of padding to match WinIE.
    textBlockStyle->setPaddingLeft(Length(1, Fixed));
    textBlockStyle->setPaddingRight(Length(1, Fixed));

    // When the placeholder is going to be displayed, temporarily override the text security to be "none".
    // After this, updateFromElement will immediately update the text displayed.
    // When the placeholder is no longer visible, updatePlaceholderVisiblity will reset the style, 
    // and the text security mode will be set back to the computed value correctly.
    if (placeholderShouldBeVisible())
        textBlockStyle->setTextSecurity(TSNONE);

    return textBlockStyle.release();
}

PassRefPtr<RenderStyle> RenderTextControlSingleLine::createInnerBlockStyle(const RenderStyle* startStyle) const
{
    ASSERT(node()->isHTMLElement());

    RefPtr<RenderStyle> innerBlockStyle = RenderStyle::create();
    innerBlockStyle->inheritFrom(startStyle);

    innerBlockStyle->setDisplay(BLOCK);
    innerBlockStyle->setDirection(LTR);

    // We don't want the shadow dom to be editable, so we set this block to read-only in case the input itself is editable.
    innerBlockStyle->setUserModify(READ_ONLY);

    return innerBlockStyle.release();
}

PassRefPtr<RenderStyle> RenderTextControlSingleLine::createResultsButtonStyle(const RenderStyle* startStyle) const
{
    ASSERT(node()->isHTMLElement());
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());

    RefPtr<RenderStyle> resultsBlockStyle;
    if (input->maxResults() < 0)
        resultsBlockStyle = getCachedPseudoStyle(RenderStyle::SEARCH_DECORATION);
    else if (!input->maxResults())
        resultsBlockStyle = getCachedPseudoStyle(RenderStyle::SEARCH_RESULTS_DECORATION);
    else
        resultsBlockStyle = getCachedPseudoStyle(RenderStyle::SEARCH_RESULTS_BUTTON);

    if (!resultsBlockStyle)
        resultsBlockStyle = RenderStyle::create();

    if (startStyle)
        resultsBlockStyle->inheritFrom(startStyle);

    return resultsBlockStyle.release();
}

PassRefPtr<RenderStyle> RenderTextControlSingleLine::createCancelButtonStyle(const RenderStyle* startStyle) const
{
    ASSERT(node()->isHTMLElement());
    RefPtr<RenderStyle> cancelBlockStyle;
    
    if (RefPtr<RenderStyle> pseudoStyle = getCachedPseudoStyle(RenderStyle::SEARCH_CANCEL_BUTTON))
        // We may be sharing style with another search field, but we must not share the cancel button style.
        cancelBlockStyle = RenderStyle::clone(pseudoStyle.get());
    else
        cancelBlockStyle = RenderStyle::create();

    if (startStyle)
        cancelBlockStyle->inheritFrom(startStyle);

    cancelBlockStyle->setVisibility(visibilityForCancelButton());
    return cancelBlockStyle.release();
}

void RenderTextControlSingleLine::updateCancelButtonVisibility() const
{
    if (!m_cancelButton->renderer())
        return;

    const RenderStyle* curStyle = m_cancelButton->renderer()->style();
    EVisibility buttonVisibility = visibilityForCancelButton();
    if (curStyle->visibility() == buttonVisibility)
        return;

    RefPtr<RenderStyle> cancelButtonStyle = RenderStyle::clone(curStyle);
    cancelButtonStyle->setVisibility(buttonVisibility);
    m_cancelButton->renderer()->setStyle(cancelButtonStyle);
}

EVisibility RenderTextControlSingleLine::visibilityForCancelButton() const
{
    ASSERT(node()->isHTMLElement());
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    return input->value().isEmpty() ? HIDDEN : VISIBLE;
}

const AtomicString& RenderTextControlSingleLine::autosaveName() const
{
    return static_cast<Element*>(node())->getAttribute(autosaveAttr);
}

void RenderTextControlSingleLine::startSearchEventTimer()
{
    ASSERT(node()->isHTMLElement());
    unsigned length = text().length();

    // If there's no text, fire the event right away.
    if (!length) {
        stopSearchEventTimer();
        static_cast<HTMLInputElement*>(node())->onSearch();
        return;
    }

    // After typing the first key, we wait 0.5 seconds.
    // After the second key, 0.4 seconds, then 0.3, then 0.2 from then on.
    m_searchEventTimer.startOneShot(max(0.2, 0.6 - 0.1 * length));
}

void RenderTextControlSingleLine::searchEventTimerFired(Timer<RenderTextControlSingleLine>*)
{
    ASSERT(node()->isHTMLElement());
    static_cast<HTMLInputElement*>(node())->onSearch();
}

// PopupMenuClient methods
void RenderTextControlSingleLine::valueChanged(unsigned listIndex, bool fireEvents)
{
    ASSERT(node()->isHTMLElement());
    ASSERT(static_cast<int>(listIndex) < listSize());
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    if (static_cast<int>(listIndex) == (listSize() - 1)) {
        if (fireEvents) {
            m_recentSearches.clear();
            const AtomicString& name = autosaveName();
            if (!name.isEmpty()) {
                if (!m_searchPopup)
                    m_searchPopup = SearchPopupMenu::create(this);
                m_searchPopup->saveRecentSearches(name, m_recentSearches);
            }
        }
    } else {
        input->setValue(itemText(listIndex));
        if (fireEvents)
            input->onSearch();
        input->select();
    }
}

String RenderTextControlSingleLine::itemText(unsigned listIndex) const
{
    if (itemIsSeparator(listIndex))
        return String();
    return m_recentSearches[listIndex - 1];
}

bool RenderTextControlSingleLine::itemIsEnabled(unsigned listIndex) const
{
     if (!listIndex || itemIsSeparator(listIndex))
        return false;
    return true;
}

PopupMenuStyle RenderTextControlSingleLine::itemStyle(unsigned) const
{
    return menuStyle();
}

PopupMenuStyle RenderTextControlSingleLine::menuStyle() const
{
    return PopupMenuStyle(style()->color(), style()->backgroundColor(), style()->font(), style()->visibility() == VISIBLE);
}

int RenderTextControlSingleLine::clientInsetLeft() const
{
    // Inset the menu by the radius of the cap on the left so that
    // it only runs along the straight part of the bezel.
    return height() / 2;
}

int RenderTextControlSingleLine::clientInsetRight() const
{
    // Inset the menu by the radius of the cap on the right so that
    // it only runs along the straight part of the bezel (unless it needs
    // to be wider).
    return height() / 2;
}

int RenderTextControlSingleLine::clientPaddingLeft() const
{
    int padding = paddingLeft();

    if (RenderBox* resultsRenderer = m_resultsButton ? m_resultsButton->renderBox() : 0)
        padding += resultsRenderer->width();

    return padding;
}

int RenderTextControlSingleLine::clientPaddingRight() const
{
    int padding = paddingRight();

    if (RenderBox* cancelRenderer = m_cancelButton ? m_cancelButton->renderBox() : 0)
        padding += cancelRenderer->width();

    return padding;
}

int RenderTextControlSingleLine::listSize() const
{
    // If there are no recent searches, then our menu will have 1 "No recent searches" item.
    if (!m_recentSearches.size())
        return 1;
    // Otherwise, leave room in the menu for a header, a separator, and the "Clear recent searches" item.
    return m_recentSearches.size() + 3;
}

int RenderTextControlSingleLine::selectedIndex() const
{
    return -1;
}

bool RenderTextControlSingleLine::itemIsSeparator(unsigned listIndex) const
{
   // The separator will be the second to last item in our list.
   return static_cast<int>(listIndex) == (listSize() - 2);
}

bool RenderTextControlSingleLine::itemIsLabel(unsigned listIndex) const
{
    return listIndex == 0;
}

bool RenderTextControlSingleLine::itemIsSelected(unsigned) const
{
    return false;
}

void RenderTextControlSingleLine::setTextFromItem(unsigned listIndex)
{
    ASSERT(node()->isHTMLElement());
    static_cast<HTMLInputElement*>(node())->setValue(itemText(listIndex));
}

FontSelector* RenderTextControlSingleLine::fontSelector() const
{
    return document()->styleSelector()->fontSelector();
}

HostWindow* RenderTextControlSingleLine::hostWindow() const
{
    return document()->view()->hostWindow();
}

PassRefPtr<Scrollbar> RenderTextControlSingleLine::createScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
{
    RefPtr<Scrollbar> widget;
    bool hasCustomScrollbarStyle = style()->hasPseudoStyle(RenderStyle::SCROLLBAR);
    if (hasCustomScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(client, orientation, this);
    else
        widget = Scrollbar::createNativeScrollbar(client, orientation, controlSize);
    return widget.release();
}

InputElement* RenderTextControlSingleLine::inputElement() const
{
    return toInputElement(static_cast<Element*>(node()));
}

}
