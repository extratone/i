/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Holger Hans Peter Freyther

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "Font.h"
#include "FontDescription.h"
#include "FontFallbackList.h"
#include "FontSelector.h"

#include "GraphicsContext.h"
#include <QTextLayout>
#include <QPainter>
#include <QFontMetrics>
#include <QFontInfo>
#include <qalgorithms.h>
#include <qdebug.h>

#include <limits.h>

#if QT_VERSION >= 0x040400
namespace WebCore {

static QString qstring(const TextRun& run)
{
    QString string((QChar *)run.characters(), run.length());
    QChar *uc = string.data();
    for (int i = 0; i < string.length(); ++i) {
        if (Font::treatAsSpace(uc[i].unicode()))
            uc[i] = 0x20;
        else if (Font::treatAsZeroWidthSpace(uc[i].unicode()))
            uc[i] = 0x200c;
    }
    return string;
}


static QTextLine setupLayout(QTextLayout* layout, const TextRun& style)
{
    int flags = style.rtl() ? Qt::TextForceRightToLeft : Qt::TextForceLeftToRight;
    if (style.padding())
        flags |= Qt::TextJustificationForced;
    layout->setFlags(flags);
    layout->beginLayout();
    QTextLine line = layout->createLine();
    line.setLineWidth(INT_MAX/256);
    if (style.padding())
        line.setLineWidth(line.naturalTextWidth() + style.padding());
    layout->endLayout();
    return line;
}

void Font::drawComplexText(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    if (to < 0)
        to = run.length();

    QPainter *p = ctx->platformContext();
    Color color = ctx->fillColor();
    p->setPen(QColor(color));

    QString string = qstring(run);

    // text shadow
    IntSize shadowSize;
    int shadowBlur;
    Color shadowColor;
    bool hasShadow = ctx->textDrawingMode() == cTextFill && ctx->getShadow(shadowSize, shadowBlur, shadowColor);

    if (from > 0 || to < run.length()) {
        QTextLayout layout(string, font());
        QTextLine line = setupLayout(&layout, run);
        float x1 = line.cursorToX(from);
        float x2 = line.cursorToX(to);
        if (x2 < x1)
            qSwap(x1, x2);

        QFontMetrics fm(font());
        int ascent = fm.ascent();
        QRectF clip(point.x() + x1, point.y() - ascent, x2 - x1, fm.height());

        if (hasShadow) {
            // TODO: when blur support is added, the clip will need to account
            // for the blur radius
            qreal dx1 = 0, dx2 = 0, dy1 = 0, dy2 = 0;
            if (shadowSize.width() > 0)
                dx2 = shadowSize.width();
            else
                dx1 = -shadowSize.width();
            if (shadowSize.height() > 0)
                dy2 = shadowSize.height();
            else
                dy1 = -shadowSize.height();
            // expand the clip rect to include the text shadow as well
            clip.adjust(dx1, dx2, dy1, dy2);
        }
        p->save();
        p->setClipRect(clip.toRect());
        QPointF pt(point.x(), point.y() - ascent);
        if (hasShadow) {
            p->save();
            p->setPen(QColor(shadowColor));
            p->translate(shadowSize.width(), shadowSize.height());
            line.draw(p, pt);
            p->restore();
        }
        line.draw(p, pt);
        p->restore();
        return;
    }

    p->setFont(font());

    QPointF pt(point.x(), point.y());
    int flags = run.rtl() ? Qt::TextForceRightToLeft : Qt::TextForceLeftToRight;
    if (hasShadow) {
        // TODO: text shadow blur support
        p->save();
        p->setPen(QColor(shadowColor));
        p->translate(shadowSize.width(), shadowSize.height());
        p->drawText(pt, string, flags, run.padding());
        p->restore();
    }
    p->drawText(pt, string, flags, run.padding());
}

float Font::floatWidthForComplexText(const TextRun& run) const
{
    if (!run.length())
        return 0;
    QString string = qstring(run);
    QTextLayout layout(string, font());
    QTextLine line = setupLayout(&layout, run);
    int w = int(line.naturalTextWidth());
    // WebKit expects us to ignore word spacing on the first character (as opposed to what Qt does)
    if (treatAsSpace(run[0]))
        w -= m_wordSpacing;

    return w + run.padding();
}

int Font::offsetForPositionForComplexText(const TextRun& run, int position, bool includePartialGlyphs) const
{
    QString string = qstring(run);
    QTextLayout layout(string, font());
    QTextLine line = setupLayout(&layout, run);
    return line.xToCursor(position);
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const IntPoint& pt, int h, int from, int to) const
{
    QString string = qstring(run);
    QTextLayout layout(string, font());
    QTextLine line = setupLayout(&layout, run);

    float x1 = line.cursorToX(from);
    float x2 = line.cursorToX(to);
    if (x2 < x1)
        qSwap(x1, x2);

    return FloatRect(pt.x() + x1, pt.y(), x2 - x1, h);
}

QFont Font::font() const
{
    return primaryFont()->getQtFont();
}

}

#endif

