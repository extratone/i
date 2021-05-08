/**
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Computer, Inc.
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
 */

#include "config.h"
#include "RenderTheme.h"

#include "CSSValueKeywords.h"
#include "Document.h"
#include "FocusController.h"
#include "FontSelector.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Page.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "SelectionController.h"
#include "Settings.h"

#include <wtf/UnusedParam.h>

// The methods in this file are shared by all themes on every platform.

namespace WebCore {

using namespace HTMLNames;

RenderTheme::RenderTheme()
#if USE(NEW_THEME)
    : m_theme(platformTheme())
#endif
{
}

void RenderTheme::adjustStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e,
                              bool UAHasAppearance, const BorderData& border, const FillLayer& background, const Color& backgroundColor)
{
    // Force inline and table display styles to be inline-block (except for table- which is block)
    ControlPart part = style->appearance();
    if (style->display() == INLINE || style->display() == INLINE_TABLE || style->display() == TABLE_ROW_GROUP ||
        style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_FOOTER_GROUP ||
        style->display() == TABLE_ROW || style->display() == TABLE_COLUMN_GROUP || style->display() == TABLE_COLUMN ||
        style->display() == TABLE_CELL || style->display() == TABLE_CAPTION)
        style->setDisplay(INLINE_BLOCK);
    else if (style->display() == COMPACT || style->display() == RUN_IN || style->display() == LIST_ITEM || style->display() == TABLE)
        style->setDisplay(BLOCK);

    if (UAHasAppearance && theme()->isControlStyled(style, border, background, backgroundColor)) {
        if (part == MenulistPart) {
            style->setAppearance(MenulistButtonPart);
            part = MenulistButtonPart;
        } else
            style->setAppearance(NoControlPart);
    }

    if (!style->hasAppearance())
        return;

    // Never support box-shadow on native controls.
    style->setBoxShadow(0);
    
#if USE(NEW_THEME)
    switch (part) {
        case CheckboxPart:
        case RadioPart:
        case PushButtonPart:
        case SquareButtonPart:
        case DefaultButtonPart:
        case ButtonPart: {
            // Border
            LengthBox borderBox(style->borderTopWidth(), style->borderRightWidth(), style->borderBottomWidth(), style->borderLeftWidth());
            borderBox = m_theme->controlBorder(part, style->font(), borderBox, style->effectiveZoom());
            if (borderBox.top().value() != style->borderTopWidth()) {
                if (borderBox.top().value())
                    style->setBorderTopWidth(borderBox.top().value());
                else
                    style->resetBorderTop();
            }
            if (borderBox.right().value() != style->borderRightWidth()) {
                if (borderBox.right().value())
                    style->setBorderRightWidth(borderBox.right().value());
                else
                    style->resetBorderRight();
            }
            if (borderBox.bottom().value() != style->borderBottomWidth()) {
                style->setBorderBottomWidth(borderBox.bottom().value());
                if (borderBox.bottom().value())
                    style->setBorderBottomWidth(borderBox.bottom().value());
                else
                    style->resetBorderBottom();
            }
            if (borderBox.left().value() != style->borderLeftWidth()) {
                style->setBorderLeftWidth(borderBox.left().value());
                if (borderBox.left().value())
                    style->setBorderLeftWidth(borderBox.left().value());
                else
                    style->resetBorderLeft();
            }

            // Padding
            LengthBox paddingBox = m_theme->controlPadding(part, style->font(), style->paddingBox(), style->effectiveZoom());
            if (paddingBox != style->paddingBox())
                style->setPaddingBox(paddingBox);

            // Whitespace
            if (m_theme->controlRequiresPreWhiteSpace(part))
                style->setWhiteSpace(PRE);
            
            // Width / Height
            // The width and height here are affected by the zoom.
            // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
            LengthSize controlSize = m_theme->controlSize(part, style->font(), LengthSize(style->width(), style->height()), style->effectiveZoom());
            if (controlSize.width() != style->width())
                style->setWidth(controlSize.width());
            if (controlSize.height() != style->height())
                style->setHeight(controlSize.height());
                
            // Min-Width / Min-Height
            LengthSize minControlSize = m_theme->minimumControlSize(part, style->font(), style->effectiveZoom());
            if (minControlSize.width() != style->minWidth())
                style->setMinWidth(minControlSize.width());
            if (minControlSize.height() != style->minHeight())
                style->setMinHeight(minControlSize.height());
                
            // Font
            FontDescription controlFont = m_theme->controlFont(part, style->font(), style->effectiveZoom());
            if (controlFont != style->font().fontDescription()) {
                // Reset our line-height
                style->setLineHeight(RenderStyle::initialLineHeight());
                
                // Now update our font.
                if (style->setFontDescription(controlFont))
                    style->font().update(0);
            }
        }
        default:
            break;
    }
#endif

    // Call the appropriate style adjustment method based off the appearance value.
    switch (style->appearance()) {
#if !USE(NEW_THEME)
        case CheckboxPart:
            return adjustCheckboxStyle(selector, style, e);
        case RadioPart:
            return adjustRadioStyle(selector, style, e);
        case PushButtonPart:
        case SquareButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
            return adjustButtonStyle(selector, style, e);
#endif
        case TextFieldPart:
            return adjustTextFieldStyle(selector, style, e);
        case TextAreaPart:
            return adjustTextAreaStyle(selector, style, e);
        case MenulistPart:
            return adjustMenuListStyle(selector, style, e);
        case MenulistButtonPart:
            return adjustMenuListButtonStyle(selector, style, e);
        case MediaSliderPart:
        case SliderHorizontalPart:
        case SliderVerticalPart:
            return adjustSliderTrackStyle(selector, style, e);
        case SliderThumbHorizontalPart:
        case SliderThumbVerticalPart:
            return adjustSliderThumbStyle(selector, style, e);
        default:
            break;
    }
}

bool RenderTheme::paint(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    // If painting is disabled, but we aren't updating control tints, then just bail.
    // If we are updating control tints, just schedule a repaint if the theme supports tinting
    // for that control.
    if (paintInfo.context->updatingControlTints()) {
        if (controlSupportsTints(o))
            o->repaint();
        return false;
    }
    if (paintInfo.context->paintingDisabled())
        return false;

    ControlPart part = o->style()->appearance();

#if USE(NEW_THEME)
    switch (part) {
        case CheckboxPart:
        case RadioPart:
        case PushButtonPart:
        case SquareButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
            m_theme->paint(part, controlStatesForRenderer(o), const_cast<GraphicsContext*>(paintInfo.context), r, o->style()->effectiveZoom(), o->view()->frameView());
            return false;
        default:
            break;
    }
#endif

    // Call the appropriate paint method based off the appearance value.
    switch (part) {
#if !USE(NEW_THEME)
        case CheckboxPart:
            return paintCheckbox(o, paintInfo, r);
        case RadioPart:
            return paintRadio(o, paintInfo, r);
        case PushButtonPart:
        case SquareButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
            return paintButton(o, paintInfo, r);
#endif
        case MenulistPart:
            return paintMenuList(o, paintInfo, r);
        case SliderHorizontalPart:
        case SliderVerticalPart:
            return paintSliderTrack(o, paintInfo, r);
        case SliderThumbHorizontalPart:
        case SliderThumbVerticalPart:
            if (o->parent()->isSlider())
                return paintSliderThumb(o, paintInfo, r);
            // We don't support drawing a slider thumb without a parent slider
            break;
        case MediaFullscreenButtonPart:
            return paintMediaFullscreenButton(o, paintInfo, r);
        case MediaPlayButtonPart:
            return paintMediaPlayButton(o, paintInfo, r);
        case MediaMuteButtonPart:
            return paintMediaMuteButton(o, paintInfo, r);
        case MediaSeekBackButtonPart:
            return paintMediaSeekBackButton(o, paintInfo, r);
        case MediaSeekForwardButtonPart:
            return paintMediaSeekForwardButton(o, paintInfo, r);
        case MediaSliderPart:
            return paintMediaSliderTrack(o, paintInfo, r);
        case MediaSliderThumbPart:
            if (o->parent()->isSlider())
                return paintMediaSliderThumb(o, paintInfo, r);
            break;
        case MediaTimeRemainingPart:
            return paintMediaTimeRemaining(o, paintInfo, r);
        case MediaCurrentTimePart:
            return paintMediaCurrentTime(o, paintInfo, r);
        case MediaTimelineContainerPart:
            return paintMediaTimelineContainer(o, paintInfo, r);
        case MenulistButtonPart:
        case TextFieldPart:
        case TextAreaPart:
        case ListboxPart:
            return true;
        default:
            break;
    }

    return true; // We don't support the appearance, so let the normal background/border paint.
}

bool RenderTheme::paintBorderOnly(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    if (paintInfo.context->paintingDisabled())
        return false;

    UNUSED_PARAM(r);
    return o->style()->appearance() != NoControlPart;
}

bool RenderTheme::paintDecorations(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    if (paintInfo.context->paintingDisabled())
        return false;

    // Call the appropriate paint method based off the appearance value.
    switch (o->style()->appearance()) {
        case MenulistButtonPart:
            return paintMenuListButtonDecorations(o, paintInfo, r);
        case TextFieldPart:
            return paintTextFieldDecorations(o, paintInfo, r);
        case TextAreaPart:
            return paintTextAreaDecorations(o, paintInfo, r);
        case CheckboxPart:
            return paintCheckboxDecorations(o, paintInfo, r);
        case RadioPart:
            return paintRadioDecorations(o, paintInfo, r);
        case PushButtonPart:
            return paintPushButtonDecorations(o, paintInfo, r);
        case SquareButtonPart:
            return paintSquareButtonDecorations(o, paintInfo, r);
        case ButtonPart:
            return paintButtonDecorations(o, paintInfo, r);
        case MenulistPart:
            return paintMenuListDecorations(o, paintInfo, r);
        default:
            break;
    }

    return false;
}

#if ENABLE(VIDEO)
bool RenderTheme::hitTestMediaControlPart(RenderObject* o, const IntPoint& absPoint)
{
    if (!o->isBox())
        return false;

    FloatPoint localPoint = o->absoluteToLocal(absPoint, false, true);  // respect transforms
    return toRenderBox(o)->borderBoxRect().contains(roundedIntPoint(localPoint));
}
#endif

Color RenderTheme::activeSelectionBackgroundColor() const
{
    if (!m_activeSelectionBackgroundColor.isValid())
        m_activeSelectionBackgroundColor = platformActiveSelectionBackgroundColor().blendWithWhite();
    return m_activeSelectionBackgroundColor;
}

Color RenderTheme::inactiveSelectionBackgroundColor() const
{
    if (!m_inactiveSelectionBackgroundColor.isValid())
        m_inactiveSelectionBackgroundColor = platformInactiveSelectionBackgroundColor().blendWithWhite();
    return m_inactiveSelectionBackgroundColor;
}

Color RenderTheme::activeSelectionForegroundColor() const
{
    if (!m_activeSelectionForegroundColor.isValid() && supportsSelectionForegroundColors())
        m_activeSelectionForegroundColor = platformActiveSelectionForegroundColor();
    return m_activeSelectionForegroundColor;
}

Color RenderTheme::inactiveSelectionForegroundColor() const
{
    if (!m_inactiveSelectionForegroundColor.isValid() && supportsSelectionForegroundColors())
        m_inactiveSelectionForegroundColor = platformInactiveSelectionForegroundColor();
    return m_inactiveSelectionForegroundColor;
}

Color RenderTheme::activeListBoxSelectionBackgroundColor() const
{
    if (!m_activeListBoxSelectionBackgroundColor.isValid())
        m_activeListBoxSelectionBackgroundColor = platformActiveListBoxSelectionBackgroundColor();
    return m_activeListBoxSelectionBackgroundColor;
}

Color RenderTheme::inactiveListBoxSelectionBackgroundColor() const
{
    if (!m_inactiveListBoxSelectionBackgroundColor.isValid())
        m_inactiveListBoxSelectionBackgroundColor = platformInactiveListBoxSelectionBackgroundColor();
    return m_inactiveListBoxSelectionBackgroundColor;
}

Color RenderTheme::activeListBoxSelectionForegroundColor() const
{
    if (!m_activeListBoxSelectionForegroundColor.isValid() && supportsListBoxSelectionForegroundColors())
        m_activeListBoxSelectionForegroundColor = platformActiveListBoxSelectionForegroundColor();
    return m_activeListBoxSelectionForegroundColor;
}

Color RenderTheme::inactiveListBoxSelectionForegroundColor() const
{
    if (!m_inactiveListBoxSelectionForegroundColor.isValid() && supportsListBoxSelectionForegroundColors())
        m_inactiveListBoxSelectionForegroundColor = platformInactiveListBoxSelectionForegroundColor();
    return m_inactiveListBoxSelectionForegroundColor;
}

Color RenderTheme::platformActiveSelectionBackgroundColor() const
{
    // Use a blue color by default if the platform theme doesn't define anything.
    return Color(0, 0, 255);
}

Color RenderTheme::platformActiveSelectionForegroundColor() const
{
    // Use a white color by default if the platform theme doesn't define anything.
    return Color::white;
}

Color RenderTheme::platformInactiveSelectionBackgroundColor() const
{
    // Use a grey color by default if the platform theme doesn't define anything.
    // This color matches Firefox's inactive color.
    return Color(176, 176, 176);
}

Color RenderTheme::platformInactiveSelectionForegroundColor() const
{
    // Use a black color by default.
    return Color::black;
}

Color RenderTheme::platformActiveListBoxSelectionBackgroundColor() const
{
    return platformActiveSelectionBackgroundColor();
}

Color RenderTheme::platformActiveListBoxSelectionForegroundColor() const
{
    return platformActiveSelectionForegroundColor();
}

Color RenderTheme::platformInactiveListBoxSelectionBackgroundColor() const
{
    return platformInactiveSelectionBackgroundColor();
}

Color RenderTheme::platformInactiveListBoxSelectionForegroundColor() const
{
    return platformInactiveSelectionForegroundColor();
}

int RenderTheme::baselinePosition(const RenderObject* o) const
{
    if (!o->isBox())
        return 0;

    const RenderBox* box = toRenderBox(o);

#if USE(NEW_THEME)
    return box->height() + box->marginTop() + m_theme->baselinePositionAdjustment(o->style()->appearance()) * o->style()->effectiveZoom();
#else
    return box->height() + box->marginTop();
#endif
}

bool RenderTheme::isControlContainer(ControlPart appearance) const
{
    // There are more leaves than this, but we'll patch this function as we add support for
    // more controls.
    return appearance != CheckboxPart && appearance != RadioPart;
}

bool RenderTheme::isControlStyled(const RenderStyle* style, const BorderData& border, const FillLayer& background,
                                  const Color& backgroundColor) const
{
    switch (style->appearance()) {
        case PushButtonPart:
        case SquareButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
        case ListboxPart:
        case MenulistPart:
        // FIXME: Uncomment this when making search fields style-able.
        // case SearchFieldPart:
        case TextFieldPart:
        case TextAreaPart:
            // Test the style to see if the UA border and background match.
            return (style->border() != border ||
                    *style->backgroundLayers() != background ||
                    style->backgroundColor() != backgroundColor);
        default:
            return false;
    }
}

void RenderTheme::adjustRepaintRect(const RenderObject* o, IntRect& r)
{
#if USE(NEW_THEME)
    m_theme->inflateControlPaintRect(o->style()->appearance(), controlStatesForRenderer(o), r, o->style()->effectiveZoom());
#else
    UNUSED_PARAM(o);
    UNUSED_PARAM(r);
#endif
}

bool RenderTheme::supportsFocusRing(const RenderStyle* style) const
{
    return (style->hasAppearance() && style->appearance() != TextFieldPart && style->appearance() != TextAreaPart && style->appearance() != MenulistButtonPart && style->appearance() != ListboxPart);
}

bool RenderTheme::stateChanged(RenderObject* o, ControlState state) const
{
    // Default implementation assumes the controls dont respond to changes in :hover state
    if (state == HoverState && !supportsHover(o->style()))
        return false;

    // Assume pressed state is only responded to if the control is enabled.
    if (state == PressedState && !isEnabled(o))
        return false;

    // Repaint the control.
    o->repaint();
    return true;
}

ControlStates RenderTheme::controlStatesForRenderer(const RenderObject* o) const
{
    ControlStates result = 0;
    if (isHovered(o))
        result |= HoverState;
    if (isPressed(o))
        result |= PressedState;
    if (isFocused(o) && o->style()->outlineStyleIsAuto())
        result |= FocusState;
    if (isEnabled(o))
        result |= EnabledState;
    if (isChecked(o))
        result |= CheckedState;
    if (isReadOnlyControl(o))
        result |= ReadOnlyState;
    if (isDefault(o))
        result |= DefaultState;
    if (!isActive(o))
        result |= WindowInactiveState;
    if (isIndeterminate(o))
        result |= IndeterminateState;
    return result;
}

bool RenderTheme::isActive(const RenderObject* o) const
{
    Node* node = o->element();
    if (!node)
        return false;

    Frame* frame = node->document()->frame();
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    return page->focusController()->isActive();
}

bool RenderTheme::isChecked(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->isChecked();
}

bool RenderTheme::isIndeterminate(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->isIndeterminate();
}

bool RenderTheme::isEnabled(const RenderObject* o) const
{
    if (!o->element())
        return true;
    return o->element()->isEnabled();
}

bool RenderTheme::isFocused(const RenderObject* o) const
{
    Node* node = o->element();
    if (!node)
        return false;
    Document* document = node->document();
    Frame* frame = document->frame();
    return node == document->focusedNode() && frame && frame->selection()->isFocusedAndActive();
}

bool RenderTheme::isPressed(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->active();
}

bool RenderTheme::isReadOnlyControl(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->isReadOnlyControl();
}

bool RenderTheme::isHovered(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->hovered();
}

bool RenderTheme::isDefault(const RenderObject* o) const
{
    if (!o->document())
        return false;

    Settings* settings = o->document()->settings();
    if (!settings || !settings->inApplicationChromeMode())
        return false;
    
    return o->style()->appearance() == DefaultButtonPart;
}

#if !USE(NEW_THEME)

void RenderTheme::adjustCheckboxStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    // A summary of the rules for checkbox designed to match WinIE:
    // width/height - honored (WinIE actually scales its control for small widths, but lets it overflow for small heights.)
    // font-size - not honored (control has no text), but we use it to decide which control size to use.
    setCheckboxSize(style);

    // padding - not honored by WinIE, needs to be removed.
    style->resetPadding();

    // border - honored by WinIE, but looks terrible (just paints in the control box and turns off the Windows XP theme)
    // for now, we will not honor it.
    style->resetBorder();

    style->setBoxShadow(0);
}

void RenderTheme::adjustRadioStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    // A summary of the rules for checkbox designed to match WinIE:
    // width/height - honored (WinIE actually scales its control for small widths, but lets it overflow for small heights.)
    // font-size - not honored (control has no text), but we use it to decide which control size to use.
    setRadioSize(style);

    // padding - not honored by WinIE, needs to be removed.
    style->resetPadding();

    // border - honored by WinIE, but looks terrible (just paints in the control box and turns off the Windows XP theme)
    // for now, we will not honor it.
    style->resetBorder();

    style->setBoxShadow(0);
}

void RenderTheme::adjustButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    // Most platforms will completely honor all CSS, and so we have no need to adjust the style
    // at all by default.  We will still allow the theme a crack at setting up a desired vertical size.
    setButtonSize(style);
}

#endif

void RenderTheme::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::adjustTextAreaStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::adjustButtonInnerStyle(RenderStyle*) const
{
}

void RenderTheme::adjustSliderTrackStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSliderThumbStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSliderThumbSize(RenderObject*) const
{
}

void RenderTheme::adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSearchFieldDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSearchFieldResultsButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const
{
}

void RenderTheme::platformColorsDidChange()
{
    m_activeSelectionForegroundColor = Color();
    m_inactiveSelectionForegroundColor = Color();
    m_activeSelectionBackgroundColor = Color();
    m_inactiveSelectionBackgroundColor = Color();

    m_activeListBoxSelectionForegroundColor = Color();
    m_inactiveListBoxSelectionForegroundColor = Color();
    m_activeListBoxSelectionBackgroundColor = Color();
    m_inactiveListBoxSelectionForegroundColor = Color();
}

Color RenderTheme::systemColor(int cssValueId) const
{
    switch (cssValueId) {
        case CSSValueActiveborder:
            return 0xFFFFFFFF;
        case CSSValueActivecaption:
            return 0xFFCCCCCC;
        case CSSValueAppworkspace:
            return 0xFFFFFFFF;
        case CSSValueBackground:
            return 0xFF6363CE;
        case CSSValueButtonface:
            return 0xFFC0C0C0;
        case CSSValueButtonhighlight:
            return 0xFFDDDDDD;
        case CSSValueButtonshadow:
            return 0xFF888888;
        case CSSValueButtontext:
            return 0xFF000000;
        case CSSValueCaptiontext:
            return 0xFF000000;
        case CSSValueGraytext:
            return 0xFF808080;
        case CSSValueHighlight:
            return 0xFFB5D5FF;
        case CSSValueHighlighttext:
            return 0xFF000000;
        case CSSValueInactiveborder:
            return 0xFFFFFFFF;
        case CSSValueInactivecaption:
            return 0xFFFFFFFF;
        case CSSValueInactivecaptiontext:
            return 0xFF7F7F7F;
        case CSSValueInfobackground:
            return 0xFFFBFCC5;
        case CSSValueInfotext:
            return 0xFF000000;
        case CSSValueMenu:
            return 0xFFC0C0C0;
        case CSSValueMenutext:
            return 0xFF000000;
        case CSSValueScrollbar:
            return 0xFFFFFFFF;
        case CSSValueText:
            return 0xFF000000;
        case CSSValueThreeddarkshadow:
            return 0xFF666666;
        case CSSValueThreedface:
            return 0xFFC0C0C0;
        case CSSValueThreedhighlight:
            return 0xFFDDDDDD;
        case CSSValueThreedlightshadow:
            return 0xFFC0C0C0;
        case CSSValueThreedshadow:
            return 0xFF888888;
        case CSSValueWindow:
            return 0xFFFFFFFF;
        case CSSValueWindowframe:
            return 0xFFCCCCCC;
        case CSSValueWindowtext:
            return 0xFF000000;
    }
    return Color();
}

Color RenderTheme::platformTextSearchHighlightColor() const
{
    return Color(255, 255, 0);
}

} // namespace WebCore
