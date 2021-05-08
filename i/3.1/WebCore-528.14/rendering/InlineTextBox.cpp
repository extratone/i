/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "InlineTextBox.h"

#include "ChromeClient.h"
#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "Page.h"
#include "RenderArena.h"
#include "RenderBlock.h"
#include "RenderTheme.h"
#include "Text.h"
#include "break_lines.h"
#include <wtf/AlwaysInline.h>

using namespace std;

namespace WebCore {

int InlineTextBox::selectionTop()
{
    return root()->selectionTop();
}

int InlineTextBox::selectionHeight()
{
    return root()->selectionHeight();
}

bool InlineTextBox::isSelected(int startPos, int endPos) const
{
    int sPos = max(startPos - m_start, 0);
    int ePos = min(endPos - m_start, (int)m_len);
    return (sPos < ePos);
}

RenderObject::SelectionState InlineTextBox::selectionState()
{
    RenderObject::SelectionState state = object()->selectionState();
    if (state == RenderObject::SelectionStart || state == RenderObject::SelectionEnd || state == RenderObject::SelectionBoth) {
        int startPos, endPos;
        object()->selectionStartEnd(startPos, endPos);
        // The position after a hard line break is considered to be past its end.
        int lastSelectable = start() + len() - (isLineBreak() ? 1 : 0);

        bool start = (state != RenderObject::SelectionEnd && startPos >= m_start && startPos < m_start + m_len);
        bool end = (state != RenderObject::SelectionStart && endPos > m_start && endPos <= lastSelectable);
        if (start && end)
            state = RenderObject::SelectionBoth;
        else if (start)
            state = RenderObject::SelectionStart;
        else if (end)
            state = RenderObject::SelectionEnd;
        else if ((state == RenderObject::SelectionEnd || startPos < m_start) &&
                 (state == RenderObject::SelectionStart || endPos > lastSelectable))
            state = RenderObject::SelectionInside;
        else if (state == RenderObject::SelectionBoth)
            state = RenderObject::SelectionNone;
    }
    return state;
}

IntRect InlineTextBox::selectionRect(int tx, int ty, int startPos, int endPos)
{
    int sPos = max(startPos - m_start, 0);
    int ePos = min(endPos - m_start, (int)m_len);
    
    if (sPos >= ePos)
        return IntRect();

    RenderText* textObj = textObject();
    int selTop = selectionTop();
    int selHeight = selectionHeight();
    const Font& f = textObj->style(m_firstLine)->font();

    IntRect r = enclosingIntRect(f.selectionRectForText(TextRun(textObj->text()->characters() + m_start, m_len, textObj->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride),
                                                        IntPoint(tx + m_x, ty + selTop), selHeight, sPos, ePos));
    if (r.x() > tx + m_x + m_width)
        r.setWidth(0);
    else if (r.right() - 1 > tx + m_x + m_width)
        r.setWidth(tx + m_x + m_width - r.x());
    return r;
}

void InlineTextBox::deleteLine(RenderArena* arena)
{
    toRenderText(m_object)->removeTextBox(this);
    destroy(arena);
}

void InlineTextBox::extractLine()
{
    if (m_extracted)
        return;

    toRenderText(m_object)->extractTextBox(this);
}

void InlineTextBox::attachLine()
{
    if (!m_extracted)
        return;
    
    toRenderText(m_object)->attachTextBox(this);
}

int InlineTextBox::placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool& foundBox)
{
    if (foundBox) {
        m_truncation = cFullTruncation;
        return -1;
    }

    int ellipsisX = ltr ? blockEdge - ellipsisWidth : blockEdge + ellipsisWidth;
    
    // For LTR, if the left edge of the ellipsis is to the left of our text run, then we are the run that will get truncated.
    if (ltr) {
        if (ellipsisX <= m_x) {
            // Too far.  Just set full truncation, but return -1 and let the ellipsis just be placed at the edge of the box.
            m_truncation = cFullTruncation;
            foundBox = true;
            return -1;
        }

        if (ellipsisX < m_x + m_width) {
            if (direction() == RTL)
                return -1; // FIXME: Support LTR truncation when the last run is RTL someday.

            foundBox = true;

            int offset = offsetForPosition(ellipsisX, false);
            if (offset == 0) {
                // No characters should be rendered.  Set ourselves to full truncation and place the ellipsis at the min of our start
                // and the ellipsis edge.
                m_truncation = cFullTruncation;
                return min(ellipsisX, m_x);
            }
            
            // Set the truncation index on the text run.  The ellipsis needs to be placed just after the last visible character.
            m_truncation = offset;
            return m_x + toRenderText(m_object)->width(m_start, offset, textPos(), m_firstLine);
        }
    }
    else {
        // FIXME: Support RTL truncation someday, including both modes (when the leftmost run on the line is either RTL or LTR)
    }
    return -1;
}

Color correctedTextColor(Color textColor, Color backgroundColor) 
{
    // Adjust the text color if it is too close to the background color,
    // by darkening or lightening it to move it further away.
    
    int d = differenceSquared(textColor, backgroundColor);
    // semi-arbitrarily chose 65025 (255^2) value here after a few tests; 
    if (d > 65025) {
        return textColor;
    }
    
    int distanceFromWhite = differenceSquared(textColor, Color::white);
    int distanceFromBlack = differenceSquared(textColor, Color::black);

    if (distanceFromWhite < distanceFromBlack) {
        return textColor.dark();
    }
    
    return textColor.light();
}

void updateGraphicsContext(GraphicsContext* context, const Color& fillColor, const Color& strokeColor, float strokeThickness)
{
    int mode = context->textDrawingMode();
    if (strokeThickness > 0) {
        int newMode = mode | cTextStroke;
        if (mode != newMode) {
            context->setTextDrawingMode(newMode);
            mode = newMode;
        }
    }
    
    if (mode & cTextFill && fillColor != context->fillColor())
        context->setFillColor(fillColor);

    if (mode & cTextStroke) {
        if (strokeColor != context->strokeColor())
            context->setStrokeColor(strokeColor);
        if (strokeThickness != context->strokeThickness())
            context->setStrokeThickness(strokeThickness);
    }
}

bool InlineTextBox::isLineBreak() const
{
    return object()->isBR() || (object()->style()->preserveNewline() && len() == 1 && (*textObject()->text())[start()] == '\n');
}

bool InlineTextBox::nodeAtPoint(const HitTestRequest&, HitTestResult& result, int x, int y, int tx, int ty)
{
    if (isLineBreak())
        return false;

    IntRect rect(tx + m_x, ty + m_y, m_width, m_height);
    if (m_truncation != cFullTruncation && visibleToHitTesting() && rect.contains(x, y)) {
        object()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
        return true;
    }
    return false;
}

static void paintTextWithShadows(GraphicsContext* context, const Font& font, const TextRun& textRun, int startOffset, int endOffset, const IntPoint& textOrigin, int x, int y, int w, int h, ShadowData* shadow, bool stroked)
{
    do {
        IntSize extraOffset;

        if (shadow) {
            IntSize shadowOffset(shadow->x, shadow->y);
            int shadowBlur = shadow->blur;
            const Color& shadowColor = shadow->color;

            if (shadow->next || stroked) {
                IntRect shadowRect(x, y, w, h);
                shadowRect.inflate(shadowBlur);
                shadowRect.move(shadowOffset);
                context->save();
                context->clip(shadowRect);

                extraOffset = IntSize(0, 2 * h + max(0, shadowOffset.height()) + shadowBlur);
                shadowOffset -= extraOffset;
            }
            context->setShadow(shadowOffset, shadowBlur, shadowColor);
        }

        // Always draw selected text with a white background on iPhone
        context->save();
        Color c = Color(255, 255, 255);
        context->setStrokeColor(c);
        if (startOffset <= endOffset)
            context->drawText(font, textRun, textOrigin + extraOffset, startOffset, endOffset);
        else {
            if (endOffset > 0)
                context->drawText(font, textRun, textOrigin + extraOffset,  0, endOffset);
            if (startOffset < textRun.length())
                context->drawText(font, textRun, textOrigin + extraOffset, startOffset);
        }
        context->restore();

        if (!shadow)
            break;

        if (shadow->next || stroked)
            context->restore();
        else
            context->clearShadow();

        shadow = shadow->next;
    } while (shadow || stroked);
}

void InlineTextBox::paint(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (isLineBreak() || !object()->shouldPaintWithinRoot(paintInfo) || object()->style()->visibility() != VISIBLE ||
        m_truncation == cFullTruncation || paintInfo.phase == PaintPhaseOutline)
        return;
    
    ASSERT(paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines);

    int xPos = tx + m_x - parent()->maxHorizontalVisualOverflow();
    int w = width() + 2 * parent()->maxHorizontalVisualOverflow();
    if (xPos >= paintInfo.rect.right() || xPos + w <= paintInfo.rect.x())
        return;

    bool isPrinting = textObject()->document()->printing();
    
    // Determine whether or not we're selected.
    bool haveSelection = !isPrinting && paintInfo.phase != PaintPhaseTextClip && selectionState() != RenderObject::SelectionNone;
    if (!haveSelection && paintInfo.phase == PaintPhaseSelection)
        // When only painting the selection, don't bother to paint if there is none.
        return;

    GraphicsContext* context = paintInfo.context;

    // Determine whether or not we have composition underlines to draw.
    bool containsComposition = object()->document()->frame()->editor()->compositionNode() == object()->node();
    bool useCustomUnderlines = containsComposition && object()->document()->frame()->editor()->compositionUsesCustomUnderlines();

    // Set our font.
    RenderStyle* styleToUse = object()->style(m_firstLine);
    int d = styleToUse->textDecorationsInEffect();
    const Font& font = styleToUse->font();

    // 1. Paint backgrounds behind text if needed. Examples of such backgrounds include selection
    // and composition underlines.
    if (paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseTextClip && !isPrinting) {
#if PLATFORM(MAC)
        // Custom highlighters go behind everything else.
        if (styleToUse->highlight() != nullAtom && !context->paintingDisabled())
            paintCustomHighlight(tx, ty, styleToUse->highlight());
#endif

        if (containsComposition && !useCustomUnderlines)
            paintCompositionBackground(context, tx, ty, styleToUse, font,
                object()->document()->frame()->editor()->compositionStart(),
                object()->document()->frame()->editor()->compositionEnd());

        // On iPhone, we suppress the drawing of selected marked text in favor of our special
        // drawing done in WebKit.
        if (haveSelection && !containsComposition)
            paintSelection(context, tx, ty, styleToUse, font);
    }

    // 2. Now paint the foreground, including text and decorations like underline/overline (in quirks mode only).
    if (m_len <= 0)
        return;

    Color textFillColor;
    Color textStrokeColor;
    float textStrokeWidth = styleToUse->textStrokeWidth();
    ShadowData* textShadow = paintInfo.forceBlackText ? 0 : styleToUse->textShadow();

    if (paintInfo.forceBlackText) {
        textFillColor = Color::black;
        textStrokeColor = Color::black;
    } else {
        textFillColor = styleToUse->textFillColor();
        if (!textFillColor.isValid())
            textFillColor = styleToUse->color();

        // Make the text fill color legible against a white background
        if (styleToUse->forceBackgroundsToWhite())
            textFillColor = correctedTextColor(textFillColor, Color::white);

        textStrokeColor = styleToUse->textStrokeColor();
        if (!textStrokeColor.isValid())
            textStrokeColor = styleToUse->color();

        // Make the text stroke color legible against a white background
        if (styleToUse->forceBackgroundsToWhite())
            textStrokeColor = correctedTextColor(textStrokeColor, Color::white);
    }

    bool paintSelectedTextOnly = (paintInfo.phase == PaintPhaseSelection);
    bool paintSelectedTextSeparately = false;

    // If there is marked text, we need to draw the selected/marked text in a separate pass,
    // in order to support our custom drawing.
    paintSelectedTextSeparately = containsComposition;
    Color selectionFillColor = textFillColor;
    Color selectionStrokeColor = textStrokeColor;
    float selectionStrokeWidth = textStrokeWidth;
    ShadowData* selectionShadow = textShadow;
    if (haveSelection) {
        // Check foreground color first.
        Color foreground = paintInfo.forceBlackText ? Color::black : object()->selectionForegroundColor();
        if (foreground.isValid() && foreground != selectionFillColor) {
            if (!paintSelectedTextOnly)
                paintSelectedTextSeparately = true;
            selectionFillColor = foreground;
        }

        if (RenderStyle* pseudoStyle = object()->getCachedPseudoStyle(RenderStyle::SELECTION)) {
            ShadowData* shadow = paintInfo.forceBlackText ? 0 : pseudoStyle->textShadow();
            if (shadow != selectionShadow) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionShadow = shadow;
            }

            float strokeWidth = pseudoStyle->textStrokeWidth();
            if (strokeWidth != selectionStrokeWidth) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionStrokeWidth = strokeWidth;
            }

            Color stroke = paintInfo.forceBlackText ? Color::black : pseudoStyle->textStrokeColor();
            if (!stroke.isValid())
                stroke = pseudoStyle->color();
            if (stroke != selectionStrokeColor) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionStrokeColor = stroke;
            }
        }
    }

    IntPoint textOrigin(m_x + tx, m_y + ty + m_baseline);
    TextRun textRun(textObject()->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || styleToUse->visuallyOrdered());

    int sPos = 0;
    int ePos = 0;
    if (paintSelectedTextOnly || paintSelectedTextSeparately)
        selectionStartEnd(sPos, ePos);

    if (!paintSelectedTextOnly) {
        // For stroked painting, we have to change the text drawing mode.  It's probably dangerous to leave that mutated as a side
        // effect, so only when we know we're stroking, do a save/restore.
        if (textStrokeWidth > 0)
            context->save();

        updateGraphicsContext(context, textFillColor, textStrokeColor, textStrokeWidth);
        if (!paintSelectedTextSeparately || ePos <= sPos) {
            // FIXME: Truncate right-to-left text correctly.
            paintTextWithShadows(context, font, textRun, 0, m_truncation == cNoTruncation ? m_len : m_truncation, textOrigin, m_x + tx, m_y + ty, width(), height(), textShadow, textStrokeWidth > 0);
        } else
            paintTextWithShadows(context, font, textRun, ePos, sPos, textOrigin, m_x + tx, m_y + ty, width(), height(), textShadow, textStrokeWidth > 0);

        if (textStrokeWidth > 0)
            context->restore();
    }

    if ((paintSelectedTextOnly || paintSelectedTextSeparately) && sPos < ePos) {
        // paint only the text that is selected
        if (selectionStrokeWidth > 0)
            context->save();

        updateGraphicsContext(context, selectionFillColor, selectionStrokeColor, selectionStrokeWidth);
        paintTextWithShadows(context, font, textRun, sPos, ePos, textOrigin, m_x + tx, m_y + ty, width(), height(), selectionShadow, selectionStrokeWidth > 0);

        if (selectionStrokeWidth > 0)
            context->restore();
    }

    // Paint decorations
    if (d != TDNONE && paintInfo.phase != PaintPhaseSelection && styleToUse->htmlHacks()) {
        context->setStrokeColor(styleToUse->color());
        paintDecoration(context, tx, ty, d, textShadow);
    }

    if (paintInfo.phase == PaintPhaseForeground) {
        paintDocumentMarkers(context, tx, ty, styleToUse, font, false);

        if (useCustomUnderlines) {
            const Vector<CompositionUnderline>& underlines = object()->document()->frame()->editor()->customCompositionUnderlines();
            size_t numUnderlines = underlines.size();

            for (size_t index = 0; index < numUnderlines; ++index) {
                const CompositionUnderline& underline = underlines[index];

                if (underline.endOffset <= start())
                    // underline is completely before this run.  This might be an underline that sits
                    // before the first run we draw, or underlines that were within runs we skipped 
                    // due to truncation.
                    continue;
                
                if (underline.startOffset <= end()) {
                    // underline intersects this run.  Paint it.
                    paintCompositionUnderline(context, tx, ty, underline);
                    if (underline.endOffset > end() + 1)
                        // underline also runs into the next run. Bail now, no more marker advancement.
                        break;
                } else
                    // underline is completely after this run, bail.  A later run will paint it.
                    break;
            }
        }
    }
}

void InlineTextBox::selectionStartEnd(int& sPos, int& ePos)
{
    int startPos, endPos;
    if (object()->selectionState() == RenderObject::SelectionInside) {
        startPos = 0;
        endPos = textObject()->textLength();
    } else {
        textObject()->selectionStartEnd(startPos, endPos);
        if (object()->selectionState() == RenderObject::SelectionStart)
            endPos = textObject()->textLength();
        else if (object()->selectionState() == RenderObject::SelectionEnd)
            startPos = 0;
    }

    sPos = max(startPos - m_start, 0);
    ePos = min(endPos - m_start, (int)m_len);
}

void InlineTextBox::paintSelection(GraphicsContext* context, int tx, int ty, RenderStyle* style, const Font& font)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(tx);
    UNUSED_PARAM(ty);
    UNUSED_PARAM(style);
    UNUSED_PARAM(font);
    // Never draw selection on iPhone, ever EVER!
    return;
}

void InlineTextBox::paintCompositionBackground(GraphicsContext* context, int tx, int ty, RenderStyle* style, const Font& font, int startPos, int endPos)
{
    int offset = m_start;
    int sPos = max(startPos - offset, 0);
    int ePos = min(endPos - offset, (int)m_len);

    if (sPos >= ePos)
        return;

    context->save();

    Color c = style->compositionFillColor();
    Color stroke = style->compositionFrameColor();
    
    updateGraphicsContext(context, c, c, 0); // Don't draw text at all!

    int y = selectionTop();
    int h = selectionHeight();
    FloatRect effectRect = font.selectionRectForText(TextRun(textObject()->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || style->visuallyOrdered()), IntPoint(m_x + tx, y + ty), h, sPos, ePos);
    effectRect.inflate(-0.5f);
    context->setStrokeColor(stroke);
    context->strokeRect(effectRect, 1.0f);
    effectRect.inflate(-0.5f);
    context->fillRect(effectRect, c);
    context->restore();
}

#if PLATFORM(MAC)

void InlineTextBox::paintCustomHighlight(int tx, int ty, const AtomicString& type)
{
    Frame* frame = object()->document()->frame();
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;

    RootInlineBox* r = root();
    FloatRect rootRect(tx + r->xPos(), ty + selectionTop(), r->width(), selectionHeight());
    FloatRect textRect(tx + xPos(), rootRect.y(), width(), rootRect.height());

    page->chrome()->client()->paintCustomHighlight(object()->node(), type, textRect, rootRect, true, false);
}

#endif

void InlineTextBox::paintDecoration(GraphicsContext* context, int tx, int ty, int deco, ShadowData* shadow)
{
    tx += m_x;
    ty += m_y;

    if (m_truncation == cFullTruncation)
        return;
    
    int width = (m_truncation == cNoTruncation) ? m_width
        : toRenderText(m_object)->width(m_start, m_truncation, textPos(), m_firstLine);
    
    // Get the text decoration colors.
    Color underline, overline, linethrough;
    object()->getTextDecorationColors(deco, underline, overline, linethrough, true);
    
    // Use a special function for underlines to get the positioning exactly right.
    bool isPrinting = textObject()->document()->printing();
    context->setStrokeThickness(1.0f); // FIXME: We should improve this rule and not always just assume 1.

    bool linesAreOpaque = !isPrinting && (!(deco & UNDERLINE) || underline.alpha() == 255) && (!(deco & OVERLINE) || overline.alpha() == 255) && (!(deco & LINE_THROUGH) || linethrough.alpha() == 255);

    bool setClip = false;
    int extraOffset = 0;
    if (!linesAreOpaque && shadow && shadow->next) {
        context->save();
        IntRect clipRect(tx, ty, width, m_baseline + 2);
        for (ShadowData* s = shadow; s; s = s->next) {
            IntRect shadowRect(tx, ty, width, m_baseline + 2);
            shadowRect.inflate(s->blur);
            shadowRect.move(s->x, s->y);
            clipRect.unite(shadowRect);
            extraOffset = max(extraOffset, max(0, s->y) + s->blur);
        }
        context->save();
        context->clip(clipRect);
        extraOffset += m_baseline + 2;
        ty += extraOffset;
        setClip = true;
    }

    bool setShadow = false;
    do {
        if (shadow) {
            if (!shadow->next) {
                // The last set of lines paints normally inside the clip.
                ty -= extraOffset;
                extraOffset = 0;
            }
            context->setShadow(IntSize(shadow->x, shadow->y - extraOffset), shadow->blur, shadow->color);
            setShadow = true;
            shadow = shadow->next;
        }

        if (deco & UNDERLINE) {
            context->setStrokeColor(underline);
            // On iPhone we have special math to position the underline. Don't muck with the position before hand.
            context->drawLineForText(IntPoint(tx, ty + m_baseline), width, isPrinting);
        }
        if (deco & OVERLINE) {
            context->setStrokeColor(overline);
            context->drawLineForText(IntPoint(tx, ty), width, isPrinting);
        }
        if (deco & LINE_THROUGH) {
            context->setStrokeColor(linethrough);
            context->drawLineForText(IntPoint(tx, ty + 2 * m_baseline / 3), width, isPrinting);
        }
    } while (shadow);

    if (setClip)
        context->restore();
    else if (setShadow)
        context->clearShadow();
}

void InlineTextBox::paintSpellingOrGrammarMarker(GraphicsContext* pt, int tx, int ty, DocumentMarker marker, RenderStyle* style, const Font& font, bool grammar)
{
    // Never print spelling/grammar markers (5327887)
    if (textObject()->document()->printing())
        return;

    if (m_truncation == cFullTruncation)
        return;

    int start = 0;                  // start of line to draw, relative to tx
    int width = m_width;            // how much line to draw

    // Determine whether we need to measure text
    bool markerSpansWholeBox = true;
    if (m_start <= (int)marker.startOffset)
        markerSpansWholeBox = false;
    if ((end() + 1) != marker.endOffset)      // end points at the last char, not past it
        markerSpansWholeBox = false;
    if (m_truncation != cNoTruncation)
        markerSpansWholeBox = false;

    if (!markerSpansWholeBox || grammar) {
        int startPosition = max<int>(marker.startOffset - m_start, 0);
        int endPosition = min<int>(marker.endOffset - m_start, m_len);
        
        if (m_truncation != cNoTruncation)
            endPosition = min<int>(endPosition, m_truncation);

        // Calculate start & width
        IntPoint startPoint(tx + m_x, ty + selectionTop());
        TextRun run(textObject()->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || style->visuallyOrdered());
        int h = selectionHeight();
        
        IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, startPoint, h, startPosition, endPosition));
        start = markerRect.x() - startPoint.x();
        width = markerRect.width();
        
        // Store rendered rects for bad grammar markers, so we can hit-test against it elsewhere in order to
        // display a toolTip. We don't do this for misspelling markers.
        if (grammar)
            object()->document()->setRenderedRectForMarker(object()->node(), marker, markerRect);
    }
    
    // IMPORTANT: The misspelling underline is not considered when calculating the text bounds, so we have to
    // make sure to fit within those bounds.  This means the top pixel(s) of the underline will overlap the
    // bottom pixel(s) of the glyphs in smaller font sizes.  The alternatives are to increase the line spacing (bad!!)
    // or decrease the underline thickness.  The overlap is actually the most useful, and matches what AppKit does.
    // So, we generally place the underline at the bottom of the text, but in larger fonts that's not so good so
    // we pin to two pixels under the baseline.
    int lineThickness = cMisspellingLineThickness;
    int descent = m_height - m_baseline;
    int underlineOffset;
    if (descent <= (2 + lineThickness)) {
        // place the underline at the very bottom of the text in small/medium fonts
        underlineOffset = m_height - lineThickness;
    } else {
        // in larger fonts, tho, place the underline up near the baseline to prevent big gap
        underlineOffset = m_baseline + 2;
    }
    pt->drawLineForMisspellingOrBadGrammar(IntPoint(tx + m_x + start, ty + m_y + underlineOffset), width, grammar);
}

void InlineTextBox::paintTextMatchMarker(GraphicsContext* pt, int tx, int ty, DocumentMarker marker, RenderStyle* style, const Font& font)
{
   // Use same y positioning and height as for selection, so that when the selection and this highlight are on
   // the same word there are no pieces sticking out.
    int y = selectionTop();
    int h = selectionHeight();
    
    int sPos = max(marker.startOffset - m_start, (unsigned)0);
    int ePos = min(marker.endOffset - m_start, (unsigned)m_len);    
    TextRun run(textObject()->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || style->visuallyOrdered());
    IntPoint startPoint = IntPoint(m_x + tx, y + ty);
    
    // Always compute and store the rect associated with this marker
    IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, startPoint, h, sPos, ePos));
    object()->document()->setRenderedRectForMarker(object()->node(), marker, markerRect);
     
    // Optionally highlight the text
    if (object()->document()->frame()->markedTextMatchesAreHighlighted()) {
        Color color = theme()->platformTextSearchHighlightColor();
        pt->save();
        updateGraphicsContext(pt, color, color, 0);  // Don't draw text at all!
        pt->clip(IntRect(tx + m_x, ty + y, m_width, h));
        pt->drawHighlightForText(font, run, startPoint, h, color, sPos, ePos);
        pt->restore();
    }
}

void InlineTextBox::paintDocumentMarkers(GraphicsContext* pt, int tx, int ty, RenderStyle* style, const Font& font, bool background)
{
    Vector<DocumentMarker> markers = object()->document()->markersForNode(object()->node());
    Vector<DocumentMarker>::iterator markerIt = markers.begin();

    // Give any document markers that touch this run a chance to draw before the text has been drawn.
    // Note end() points at the last char, not one past it like endOffset and ranges do.
    for ( ; markerIt != markers.end(); markerIt++) {
        DocumentMarker marker = *markerIt;
        
        // Paint either the background markers or the foreground markers, but not both
        switch (marker.type) {
            case DocumentMarker::Grammar:
            case DocumentMarker::Spelling:
                if (background)
                    continue;
                break;
                
            case DocumentMarker::TextMatch:
                if (!background)
                    continue;
                break;
            
            default:
                ASSERT_NOT_REACHED();
        }

        if (marker.endOffset <= start())
            // marker is completely before this run.  This might be a marker that sits before the
            // first run we draw, or markers that were within runs we skipped due to truncation.
            continue;
        
        if (marker.startOffset > end())
            // marker is completely after this run, bail.  A later run will paint it.
            break;
        
        // marker intersects this run.  Paint it.
        switch (marker.type) {
            case DocumentMarker::Spelling:
                paintSpellingOrGrammarMarker(pt, tx, ty, marker, style, font, false);
                break;
            case DocumentMarker::Grammar:
                paintSpellingOrGrammarMarker(pt, tx, ty, marker, style, font, true);
                break;
            case DocumentMarker::TextMatch:
                paintTextMatchMarker(pt, tx, ty, marker, style, font);
                break;
            default:
                ASSERT_NOT_REACHED();
        }

    }
}


void InlineTextBox::paintCompositionUnderline(GraphicsContext* ctx, int tx, int ty, const CompositionUnderline& underline)
{
    tx += m_x;
    ty += m_y;

    if (m_truncation == cFullTruncation)
        return;
    
    int start = 0;                 // start of line to draw, relative to tx
    int width = m_width;           // how much line to draw
    bool useWholeWidth = true;
    unsigned paintStart = m_start;
    unsigned paintEnd = end() + 1; // end points at the last char, not past it
    if (paintStart <= underline.startOffset) {
        paintStart = underline.startOffset;
        useWholeWidth = false;
        start = toRenderText(m_object)->width(m_start, paintStart - m_start, textPos(), m_firstLine);
    }
    if (paintEnd != underline.endOffset) {      // end points at the last char, not past it
        paintEnd = min(paintEnd, (unsigned)underline.endOffset);
        useWholeWidth = false;
    }
    if (m_truncation != cNoTruncation) {
        paintEnd = min(paintEnd, (unsigned)m_start + m_truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = toRenderText(m_object)->width(paintStart, paintEnd - paintStart, textPos() + start, m_firstLine);
    }

    // Thick marked text underlines are 2px thick as long as there is room for the 2px line under the baseline.
    // All other marked text underlines are 1px thick.
    // If there's not enough space the underline will touch or overlap characters.
    int lineThickness = 1;
    if (underline.thick && m_height - m_baseline >= 2)
        lineThickness = 2;

    // We need to have some space between underlines of subsequent clauses, because some input methods do not use different underline styles for those.
    // We make each line shorter, which has a harmless side effect of shortening the first and last clauses, too.
    start += 1;
    width -= 2;

    ctx->setStrokeColor(underline.color);
    ctx->setStrokeThickness(lineThickness);
    ctx->drawLineForText(IntPoint(tx + start, ty + m_height - lineThickness), width, textObject()->document()->printing());
}

int InlineTextBox::caretMinOffset() const
{
    return m_start;
}

int InlineTextBox::caretMaxOffset() const
{
    return m_start + m_len;
}

unsigned InlineTextBox::caretMaxRenderedOffset() const
{
    return m_start + m_len;
}

int InlineTextBox::textPos() const
{
    if (xPos() == 0)
        return 0;
        
    RenderBlock *blockElement = object()->containingBlock();
    return direction() == RTL ? xPos() - blockElement->borderRight() - blockElement->paddingRight()
                      : xPos() - blockElement->borderLeft() - blockElement->paddingLeft();
}

int InlineTextBox::offsetForPosition(int _x, bool includePartialGlyphs) const
{
    if (isLineBreak())
        return 0;

    RenderText* text = toRenderText(m_object);
    RenderStyle *style = text->style(m_firstLine);
    const Font* f = &style->font();
    return f->offsetForPosition(TextRun(textObject()->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || style->visuallyOrdered()),
                                _x - m_x, includePartialGlyphs);
}

int InlineTextBox::positionForOffset(int offset) const
{
    ASSERT(offset >= m_start);
    ASSERT(offset <= m_start + m_len);

    if (isLineBreak())
        return m_x;

    RenderText* text = toRenderText(m_object);
    const Font& f = text->style(m_firstLine)->font();
    int from = direction() == RTL ? offset - m_start : 0;
    int to = direction() == RTL ? m_len : offset - m_start;
    // FIXME: Do we need to add rightBearing here?
    return enclosingIntRect(f.selectionRectForText(TextRun(text->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride),
                                                   IntPoint(m_x, 0), 0, from, to)).right();
}

bool InlineTextBox::containsCaretOffset(int offset) const
{
    // Offsets before the box are never "in".
    if (offset < m_start)
        return false;

    int pastEnd = m_start + m_len;

    // Offsets inside the box (not at either edge) are always "in".
    if (offset < pastEnd)
        return true;

    // Offsets outside the box are always "out".
    if (offset > pastEnd)
        return false;

    // Offsets at the end are "out" for line breaks (they are on the next line).
    if (isLineBreak())
        return false;

    // Offsets at the end are "in" for normal boxes (but the caller has to check affinity).
    return true;
}

} // namespace WebCore
