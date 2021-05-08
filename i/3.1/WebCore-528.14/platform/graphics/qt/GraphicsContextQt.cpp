/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2008 Dirk Schulze <vbs85@gmx.de>
 *
 * All rights reserved.
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

#ifdef Q_WS_WIN
#include <windows.h>
#endif

#include "TransformationMatrix.h"
#include "Color.h"
#include "FloatConversion.h"
#include "Font.h"
#include "GraphicsContext.h"
#include "GraphicsContextPrivate.h"
#include "ImageBuffer.h"
#include "Path.h"
#include "Pattern.h"
#include "Pen.h"
#include "NotImplemented.h"

#include <QDebug>
#include <QGradient>
#include <QPainter>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QStack>
#include <QVector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WebCore {

static inline QPainter::CompositionMode toQtCompositionMode(CompositeOperator op)
{
    switch (op) {
        case CompositeClear:
            return QPainter::CompositionMode_Clear;
        case CompositeCopy:
            return QPainter::CompositionMode_Source;
        case CompositeSourceOver:
            return QPainter::CompositionMode_SourceOver;
        case CompositeSourceIn:
            return QPainter::CompositionMode_SourceIn;
        case CompositeSourceOut:
            return QPainter::CompositionMode_SourceOut;
        case CompositeSourceAtop:
            return QPainter::CompositionMode_SourceAtop;
        case CompositeDestinationOver:
            return QPainter::CompositionMode_DestinationOver;
        case CompositeDestinationIn:
            return QPainter::CompositionMode_DestinationIn;
        case CompositeDestinationOut:
            return QPainter::CompositionMode_DestinationOut;
        case CompositeDestinationAtop:
            return QPainter::CompositionMode_DestinationAtop;
        case CompositeXOR:
            return QPainter::CompositionMode_Xor;
        case CompositePlusDarker:
            return QPainter::CompositionMode_SourceOver;
        case CompositeHighlight:
            return QPainter::CompositionMode_SourceOver;
        case CompositePlusLighter:
            return QPainter::CompositionMode_SourceOver;
    }

    return QPainter::CompositionMode_SourceOver;
}

static inline Qt::PenCapStyle toQtLineCap(LineCap lc)
{
    switch (lc) {
        case ButtCap:
            return Qt::FlatCap;
        case RoundCap:
            return Qt::RoundCap;
        case SquareCap:
            return Qt::SquareCap;
    }

    return Qt::FlatCap;
}

static inline Qt::PenJoinStyle toQtLineJoin(LineJoin lj)
{
    switch (lj) {
        case MiterJoin:
            return Qt::SvgMiterJoin;
        case RoundJoin:
            return Qt::RoundJoin;
        case BevelJoin:
            return Qt::BevelJoin;
    }

    return Qt::MiterJoin;
}

static Qt::PenStyle toQPenStyle(StrokeStyle style)
{
    switch (style) {
    case NoStroke:
        return Qt::NoPen;
        break;
    case SolidStroke:
        return Qt::SolidLine;
        break;
    case DottedStroke:
        return Qt::DotLine;
        break;
    case DashedStroke:
        return Qt::DashLine;
        break;
    }
    qWarning("couldn't recognize the pen style");
    return Qt::NoPen;
}

static inline QGradient applySpreadMethod(QGradient gradient, GradientSpreadMethod spreadMethod)
{
    switch (spreadMethod) {
        case SpreadMethodPad:
            gradient.setSpread(QGradient::PadSpread);
           break;
        case SpreadMethodReflect:
            gradient.setSpread(QGradient::ReflectSpread);
            break;
        case SpreadMethodRepeat:
            gradient.setSpread(QGradient::RepeatSpread);
            break;
    }
    return gradient;
}

struct TransparencyLayer
{
    TransparencyLayer(const QPainter* p, const QRect &rect)
        : pixmap(rect.width(), rect.height())
    {
        offset = rect.topLeft();
        pixmap.fill(Qt::transparent);
        painter.begin(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, p->testRenderHint(QPainter::Antialiasing));
        painter.translate(-offset);
        painter.setPen(p->pen());
        painter.setBrush(p->brush());
        painter.setTransform(p->transform(), true);
        painter.setOpacity(p->opacity());
        painter.setFont(p->font());
        if (painter.paintEngine()->hasFeature(QPaintEngine::PorterDuff))
            painter.setCompositionMode(p->compositionMode());
        painter.setClipPath(p->clipPath());
    }

    TransparencyLayer()
    {
    }

    QPixmap pixmap;
    QPoint offset;
    QPainter painter;
    qreal opacity;
private:
    TransparencyLayer(const TransparencyLayer &) {}
    TransparencyLayer & operator=(const TransparencyLayer &) { return *this; }
};

class GraphicsContextPlatformPrivate
{
public:
    GraphicsContextPlatformPrivate(QPainter* painter);
    ~GraphicsContextPlatformPrivate();

    inline QPainter* p()
    {
        if (layers.isEmpty()) {
            if (redirect)
                return redirect;

            return painter;
        } else
            return &layers.top()->painter;
    }

    bool antiAliasingForRectsAndLines;

    QStack<TransparencyLayer *> layers;
    QPainter* redirect;

    QBrush solidColor;

    // Only used by SVG for now.
    QPainterPath currentPath;

private:
    QPainter* painter;
};


GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(QPainter* p)
{
    painter = p;
    redirect = 0;

    solidColor = QBrush(Qt::black);

    if (painter) {
        // use the default the QPainter was constructed with
        antiAliasingForRectsAndLines = painter->testRenderHint(QPainter::Antialiasing);
        // FIXME: Maybe only enable in SVG mode?
        painter->setRenderHint(QPainter::Antialiasing, true);
    } else {
        antiAliasingForRectsAndLines = false;
    }
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
}

GraphicsContext::GraphicsContext(PlatformGraphicsContext* context)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(context))
{
    setPaintingDisabled(!context);
    if (context) {
        // Make sure the context starts in sync with our state.
        setPlatformFillColor(fillColor());
        setPlatformStrokeColor(strokeColor());
    }
}

GraphicsContext::~GraphicsContext()
{
    while(!m_data->layers.isEmpty())
        endTransparencyLayer();

    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    return m_data->p();
}

TransformationMatrix GraphicsContext::getCTM() const
{
    return platformContext()->combinedMatrix();
}

void GraphicsContext::savePlatformState()
{
    m_data->p()->save();
}

void GraphicsContext::restorePlatformState()
{
    m_data->p()->restore();

    if (!m_data->currentPath.isEmpty() && m_common->state.pathTransform.isInvertible()) {
        QMatrix matrix = m_common->state.pathTransform;
        m_data->currentPath = m_data->currentPath * matrix;
    }
}

/* FIXME: DISABLED WHILE MERGING BACK FROM UNITY
void GraphicsContext::drawTextShadow(const TextRun& run, const IntPoint& point, const FontStyle& style)
{
    if (paintingDisabled())
        return;

    if (m_data->shadow.isNull())
        return;

    TextShadow* shadow = &m_data->shadow;

    if (shadow->blur <= 0) {
        Pen p = pen();
        setPen(shadow->color);
        font().drawText(this, run, style, IntPoint(point.x() + shadow->x, point.y() + shadow->y));
        setPen(p);
    } else {
        const int thickness = shadow->blur;
        // FIXME: OPTIMIZE: limit the area to only the actually painted area + 2*thickness
        const int w = m_data->p()->device()->width();
        const int h = m_data->p()->device()->height();
        const QRgb color = qRgb(255, 255, 255);
        const QRgb bgColor = qRgb(0, 0, 0);
        QImage image(QSize(w, h), QImage::Format_ARGB32);
        image.fill(bgColor);
        QPainter p;

        Pen curPen = pen();
        p.begin(&image);
        setPen(color);
        m_data->redirect = &p;
        font().drawText(this, run, style, IntPoint(point.x() + shadow->x, point.y() + shadow->y));
        m_data->redirect = 0;
        p.end();
        setPen(curPen);

        int md = thickness * thickness; // max-dist^2

        // blur map/precalculated shadow-decay
        float* bmap = (float*) alloca(sizeof(float) * (md + 1));
        for (int n = 0; n <= md; n++) {
            float f;
            f = n / (float) (md + 1);
            f = 1.0 - f * f;
            bmap[n] = f;
        }

        float factor = 0.0; // maximal potential opacity-sum
        for (int n = -thickness; n <= thickness; n++) {
            for (int m = -thickness; m <= thickness; m++) {
                int d = n * n + m * m;
                if (d <= md)
                    factor += bmap[d];
            }
        }

        // alpha map
        float* amap = (float*) alloca(sizeof(float) * (h * w));
        memset(amap, 0, h * w * (sizeof(float)));

        for (int j = thickness; j<h-thickness; j++) {
            for (int i = thickness; i<w-thickness; i++) {
                QRgb col = image.pixel(i,j);
                if (col == bgColor)
                    continue;

                float g = qAlpha(col);
                g = g / 255;

                for (int n = -thickness; n <= thickness; n++) {
                    for (int m = -thickness; m <= thickness; m++) {
                        int d = n * n + m * m;
                        if (d > md)
                            continue;

                        float f = bmap[d];
                        amap[(i + m) + (j + n) * w] += (g * f);
                    }
                }
            }
        }

        QImage res(QSize(w,h),QImage::Format_ARGB32);
        int r = shadow->color.red();
        int g = shadow->color.green();
        int b = shadow->color.blue();
        int a1 = shadow->color.alpha();

        // arbitratry factor adjustment to make shadows more solid.
        factor = 1.333 / factor;

        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                int a = (int) (amap[i + j * w] * factor * a1);
                if (a > 255)
                    a = 255;

                res.setPixel(i,j, qRgba(r, g, b, a));
            }
        }

        m_data->p()->drawImage(0, 0, res, 0, 0, -1, -1, Qt::DiffuseAlphaDither | Qt::ColorOnly | Qt::PreferDither);
    }
}
*/

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    const bool antiAlias = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, m_data->antiAliasingForRectsAndLines);

    p->drawRect(rect);

    p->setRenderHint(QPainter::Antialiasing, antiAlias);
}

// FIXME: Now that this is refactored, it should be shared by all contexts.
static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth,
                                        const StrokeStyle& penStyle)
{
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == DottedStroke || penStyle == DashedStroke) {
        if (p1.x() == p2.x()) {
            p1.setY(p1.y() + strokeWidth);
            p2.setY(p2.y() - strokeWidth);
        } else {
            p1.setX(p1.x() + strokeWidth);
            p2.setX(p2.x() - strokeWidth);
        }
    }

    if (((int) strokeWidth) % 2) {
        if (p1.x() == p2.x()) {
            // We're a vertical line.  Adjust our x.
            p1.setX(p1.x() + 0.5);
            p2.setX(p2.x() + 0.5);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.setY(p1.y() + 0.5);
            p2.setY(p2.y() + 0.5);
        }
    }
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;

    QPainter *p = m_data->p();
    const bool antiAlias = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, m_data->antiAliasingForRectsAndLines);
    adjustLineToPixelBoundaries(p1, p2, strokeThickness(), strokeStyle());

    IntSize shadowSize;
    int shadowBlur;
    Color shadowColor;
    if (textDrawingMode() == cTextFill && getShadow(shadowSize, shadowBlur, shadowColor)) {
        p->save();
        p->translate(shadowSize.width(), shadowSize.height());
        p->setPen(QColor(shadowColor));
        p->drawLine(p1, p2);
        p->restore();
    }

    p->drawLine(p1, p2);

    p->setRenderHint(QPainter::Antialiasing, antiAlias);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->p()->drawEllipse(rect);
}

void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{
    if (paintingDisabled() || strokeStyle() == NoStroke || strokeThickness() <= 0.0f || !strokeColor().alpha())
        return;

    QPainter *p = m_data->p();
    const bool antiAlias = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, m_data->antiAliasingForRectsAndLines);

    p->drawArc(rect, startAngle * 16, angleSpan * 16);

    p->setRenderHint(QPainter::Antialiasing, antiAlias);
}

void GraphicsContext::drawConvexPolygon(size_t npoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (npoints <= 1)
        return;

    QPolygonF polygon(npoints);

    for (size_t i = 0; i < npoints; i++)
        polygon[i] = points[i];

    QPainter *p = m_data->p();
    p->save();
    p->setRenderHint(QPainter::Antialiasing, shouldAntialias);
    p->drawConvexPolygon(polygon);
    p->restore();
}

QPen GraphicsContext::pen()
{
    if (paintingDisabled())
        return QPen();

    QPainter *p = m_data->p();
    return p->pen();
}

void GraphicsContext::fillPath()
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPainterPath path = m_data->currentPath;

    switch (m_common->state.fillColorSpace) {
    case SolidColorSpace:
        if (fillColor().alpha())
            p->fillPath(path, p->brush());
        break;
    case PatternColorSpace: {
        TransformationMatrix affine;
        p->fillPath(path, QBrush(m_common->state.fillPattern->createPlatformPattern(affine)));
        break;
    }
    case GradientColorSpace:
        QGradient* gradient = m_common->state.fillGradient->platformGradient();
        *gradient = applySpreadMethod(*gradient, spreadMethod());  
        p->fillPath(path, QBrush(*gradient));
        break;
    }
    m_data->currentPath = QPainterPath();
}

void GraphicsContext::strokePath()
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPen pen = p->pen();
    QPainterPath path = m_data->currentPath;

    switch (m_common->state.strokeColorSpace) {
    case SolidColorSpace:
        if (strokeColor().alpha())
            p->strokePath(path, pen);
        break;
    case PatternColorSpace: {
        TransformationMatrix affine;
        pen.setBrush(QBrush(m_common->state.strokePattern->createPlatformPattern(affine)));
        p->setPen(pen);
        p->strokePath(path, pen);
        break;
    }
    case GradientColorSpace: {
        QGradient* gradient = m_common->state.strokeGradient->platformGradient();
        *gradient = applySpreadMethod(*gradient, spreadMethod()); 
        pen.setBrush(QBrush(*gradient));
        p->setPen(pen);
        p->strokePath(path, pen);
        break;
    }
    }
    m_data->currentPath = QPainterPath();
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();

    switch (m_common->state.fillColorSpace) {
    case SolidColorSpace:
        if (fillColor().alpha())
            p->fillRect(rect, p->brush());
        break;
    case PatternColorSpace: {
        TransformationMatrix affine;
        p->fillRect(rect, QBrush(m_common->state.fillPattern->createPlatformPattern(affine)));
        break;
    }
    case GradientColorSpace:
        p->fillRect(rect, QBrush(*(m_common->state.fillGradient.get()->platformGradient())));
        break;
    }
    m_data->currentPath = QPainterPath();
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& c)
{
    if (paintingDisabled())
        return;

    m_data->solidColor.setColor(QColor(c));
    m_data->p()->fillRect(rect, m_data->solidColor);
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color)
{
    if (paintingDisabled() || !color.alpha())
        return;

    Path path = Path::createRoundedRectangle(rect, topLeft, topRight, bottomLeft, bottomRight);
    m_data->p()->fillPath(*path.platformPath(), QColor(color));
}

void GraphicsContext::beginPath()
{
    m_data->currentPath = QPainterPath();
}

void GraphicsContext::addPath(const Path& path)
{
    QPainterPath newPath = m_data->currentPath;
    newPath.addPath(*(path.platformPath()));
    m_data->currentPath = newPath;
}

bool GraphicsContext::inTransparencyLayer() const
{
    return !m_data->layers.isEmpty();
}

PlatformPath* GraphicsContext::currentPath()
{
    return &m_data->currentPath;
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    if (p->clipRegion().isEmpty())
        p->setClipRect(rect);
    else p->setClipRect(rect, Qt::IntersectClip);
}

void GraphicsContext::clipPath(WindRule clipRule)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPainterPath newPath = m_data->currentPath;
    newPath.setFillRule(clipRule == RULE_EVENODD ? Qt::OddEvenFill : Qt::WindingFill);
    p->setClipPath(newPath);
}

/**
 * Focus ring handling is not handled here. Qt style in 
 * RenderTheme handles drawing focus on widgets which 
 * need it.
 */
Color focusRingColor() { return Color(0, 0, 0); }
void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    const Vector<IntRect>& rects = focusRingRects();
    unsigned rectCount = rects.size();

    if (rects.size() == 0)
        return;

    QPainter *p = m_data->p();
    const bool antiAlias = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, m_data->antiAliasingForRectsAndLines);

    const QPen oldPen = p->pen();
    const QBrush oldBrush = p->brush();

    QPen nPen = p->pen();
    nPen.setColor(color);
    p->setBrush(Qt::NoBrush);
    nPen.setStyle(Qt::DotLine);
    p->setPen(nPen);
#if 0
    // FIXME How do we do a bounding outline with Qt?
    QPainterPath path;
    for (int i = 0; i < rectCount; ++i)
        path.addRect(QRectF(rects[i]));
    QPainterPathStroker stroker;
    QPainterPath newPath = stroker.createStroke(path);
    p->strokePath(newPath, nPen);
#else
    for (int i = 0; i < rectCount; ++i)
        p->drawRect(QRectF(rects[i]));
#endif
    p->setPen(oldPen);
    p->setBrush(oldBrush);

    p->setRenderHint(QPainter::Antialiasing, antiAlias);
}

void GraphicsContext::drawLineForText(const IntPoint& origin, int width, bool printing)
{
    if (paintingDisabled())
        return;

    IntPoint endPoint = origin + IntSize(width, 0);
    drawLine(origin, endPoint);
}

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint&,
                                                         int width, bool grammar)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& frect)
{
    QRectF rect(frect);
    rect = m_data->p()->deviceMatrix().mapRect(rect);

    QRect result = rect.toRect(); //round it
    return FloatRect(QRectF(result));
}

void GraphicsContext::setPlatformShadow(const IntSize& pos, int blur, const Color &color)
{
    // Qt doesn't support shadows natively, they are drawn manually in the draw*
    // functions
}

void GraphicsContext::clearPlatformShadow()
{
    // Qt doesn't support shadows natively, they are drawn manually in the draw*
    // functions
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    int x, y, w, h;
    x = y = 0;
    QPainter *p = m_data->p();
    const QPaintDevice *device = p->device();
    w = device->width();
    h = device->height();

    QRectF clip = p->clipPath().boundingRect();
    QRectF deviceClip = p->transform().mapRect(clip);
    x = int(qBound(qreal(0), deviceClip.x(), (qreal)w));
    y = int(qBound(qreal(0), deviceClip.y(), (qreal)h));
    w = int(qBound(qreal(0), deviceClip.width(), (qreal)w) + 2);
    h = int(qBound(qreal(0), deviceClip.height(), (qreal)h) + 2);

    TransparencyLayer * layer = new TransparencyLayer(m_data->p(), QRect(x, y, w, h));

    layer->opacity = opacity;
    m_data->layers.push(layer);
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    TransparencyLayer *layer = m_data->layers.pop();
    layer->painter.end();

    QPainter *p = m_data->p();
    p->save();
    p->resetTransform();
    p->setOpacity(layer->opacity);
    p->drawPixmap(layer->offset, layer->pixmap);
    p->restore();

    delete layer;
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPainter::CompositionMode currentCompositionMode = p->compositionMode();
    if (p->paintEngine()->hasFeature(QPaintEngine::PorterDuff))
        p->setCompositionMode(QPainter::CompositionMode_Source);
    p->eraseRect(rect);
    if (p->paintEngine()->hasFeature(QPaintEngine::PorterDuff))
        p->setCompositionMode(currentCompositionMode);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float width)
{
    if (paintingDisabled())
        return;

    QPainterPath path;
    path.addRect(rect);
    setStrokeThickness(width);
    m_data->currentPath = path;

    strokePath();
}

void GraphicsContext::setLineCap(LineCap lc)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPen nPen = p->pen();
    nPen.setCapStyle(toQtLineCap(lc));
    p->setPen(nPen);
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    QPainter* p = m_data->p();
    QPen pen = p->pen();
    unsigned dashLength = dashes.size();
    if (dashLength) {
        QVector<qreal> pattern;
        unsigned count = dashLength;
        if (dashLength % 2)
            count *= 2;

        float penWidth = narrowPrecisionToFloat(double(pen.widthF()));
        for (unsigned i = 0; i < count; i++)
            pattern.append(dashes[i % dashLength] / penWidth);

        pen.setDashPattern(pattern);
        pen.setDashOffset(dashOffset);
    }
    p->setPen(pen);
}

void GraphicsContext::setLineJoin(LineJoin lj)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPen nPen = p->pen();
    nPen.setJoinStyle(toQtLineJoin(lj));
    p->setPen(nPen);
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QPen nPen = p->pen();
    nPen.setMiterLimit(limit);
    p->setPen(nPen);
}

void GraphicsContext::setAlpha(float opacity)
{
    if (paintingDisabled())
        return;
    QPainter *p = m_data->p();
    p->setOpacity(opacity);
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;

    if (m_data->p()->paintEngine()->hasFeature(QPaintEngine::PorterDuff))
        m_data->p()->setCompositionMode(toQtCompositionMode(op));
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    m_data->p()->setClipPath(*path.platformPath(), Qt::IntersectClip);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QRectF clipBounds = p->clipPath().boundingRect();
    QPainterPath clippedOut = *path.platformPath();
    QPainterPath newClip;
    newClip.setFillRule(Qt::OddEvenFill);
    newClip.addRect(clipBounds);
    newClip.addPath(clippedOut);

    p->setClipPath(newClip, Qt::IntersectClip);
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;

    m_data->p()->translate(x, y);

    if (!m_data->currentPath.isEmpty()) {
        QMatrix matrix;
        m_data->currentPath = m_data->currentPath * matrix.translate(-x, -y);
        m_common->state.pathTransform.translate(x, y);
    }
}

IntPoint GraphicsContext::origin()
{
    if (paintingDisabled())
        return IntPoint();
    const QTransform &transform = m_data->p()->transform();
    return IntPoint(qRound(transform.dx()), qRound(transform.dy()));
}

void GraphicsContext::rotate(float radians)
{
    if (paintingDisabled())
        return;

    m_data->p()->rotate(180/M_PI*radians);

    if (!m_data->currentPath.isEmpty()) {
        QMatrix matrix;
        m_data->currentPath = m_data->currentPath * matrix.rotate(-180/M_PI*radians);
        m_common->state.pathTransform.rotate(radians);
    }
}

void GraphicsContext::scale(const FloatSize& s)
{
    if (paintingDisabled())
        return;

    m_data->p()->scale(s.width(), s.height());

    if (!m_data->currentPath.isEmpty()) {
        QMatrix matrix;
        m_data->currentPath = m_data->currentPath * matrix.scale(1 / s.width(), 1 / s.height());
        m_common->state.pathTransform.scale(s.width(), s.height());
    }
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QRectF clipBounds = p->clipPath().boundingRect();
    QPainterPath newClip;
    newClip.setFillRule(Qt::OddEvenFill);
    newClip.addRect(clipBounds);
    newClip.addRect(QRect(rect));

    p->setClipPath(newClip, Qt::IntersectClip);
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    QPainter *p = m_data->p();
    QRectF clipBounds = p->clipPath().boundingRect();
    QPainterPath newClip;
    newClip.setFillRule(Qt::OddEvenFill);
    newClip.addRect(clipBounds);
    newClip.addEllipse(QRect(rect));

    p->setClipPath(newClip, Qt::IntersectClip);
}

void GraphicsContext::clipToImageBuffer(const FloatRect&, const ImageBuffer*)
{
    notImplemented();
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect,
                                              int thickness)
{
    if (paintingDisabled())
        return;

    clip(rect);
    QPainterPath path;

    // Add outer ellipse
    path.addEllipse(QRectF(rect.x(), rect.y(), rect.width(), rect.height()));

    // Add inner ellipse.
    path.addEllipse(QRectF(rect.x() + thickness, rect.y() + thickness,
                           rect.width() - (thickness * 2), rect.height() - (thickness * 2)));

    path.setFillRule(Qt::OddEvenFill);
    m_data->p()->setClipPath(path, Qt::IntersectClip);
}

void GraphicsContext::concatCTM(const TransformationMatrix& transform)
{
    if (paintingDisabled())
        return;

    m_data->p()->setMatrix(transform, true);

    // Transformations to the context shouldn't transform the currentPath. 
    // We have to undo every change made to the context from the currentPath to avoid wrong drawings.
    if (!m_data->currentPath.isEmpty() && transform.isInvertible()) {
        QMatrix matrix = transform.inverse();
        m_data->currentPath = m_data->currentPath * matrix;
        m_common->state.pathTransform.multiply(transform);
    }
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    notImplemented();
}

void GraphicsContext::setPlatformStrokeColor(const Color& color)
{
    if (paintingDisabled())
        return;
    QPainter *p = m_data->p();
    QPen newPen(p->pen());
    newPen.setColor(color);
    p->setPen(newPen);
}

void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle& strokeStyle)
{
    if (paintingDisabled())
        return;
    QPainter *p = m_data->p();
    QPen newPen(p->pen());
    newPen.setStyle(toQPenStyle(strokeStyle));
    p->setPen(newPen);
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    if (paintingDisabled())
        return;
    QPainter *p = m_data->p();
    QPen newPen(p->pen());
    newPen.setWidthF(thickness);
    p->setPen(newPen);
}

void GraphicsContext::setPlatformFillColor(const Color& color)
{
    if (paintingDisabled())
        return;
    m_data->p()->setBrush(QBrush(color));
}

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;
    m_data->p()->setRenderHint(QPainter::Antialiasing, enable);
}

#ifdef Q_WS_WIN
#include <windows.h>

HDC GraphicsContext::getWindowsContext(const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    // painting through native HDC is only supported for plugin, where mayCreateBitmap is always true
    Q_ASSERT(mayCreateBitmap);

    if (dstRect.isEmpty())
        return 0;

    // Create a bitmap DC in which to draw.
    BITMAPINFO bitmapInfo;
    bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth         = dstRect.width();
    bitmapInfo.bmiHeader.biHeight        = dstRect.height();
    bitmapInfo.bmiHeader.biPlanes        = 1;
    bitmapInfo.bmiHeader.biBitCount      = 32;
    bitmapInfo.bmiHeader.biCompression   = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage     = 0;
    bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biClrUsed       = 0;
    bitmapInfo.bmiHeader.biClrImportant  = 0;

    void* pixels = 0;
    HBITMAP bitmap = ::CreateDIBSection(NULL, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
    if (!bitmap)
        return 0;

    HDC displayDC = ::GetDC(0);
    HDC bitmapDC = ::CreateCompatibleDC(displayDC);
    ::ReleaseDC(0, displayDC);

    ::SelectObject(bitmapDC, bitmap);

    // Fill our buffer with clear if we're going to alpha blend.
    if (supportAlphaBlend) {
        BITMAP bmpInfo;
        GetObject(bitmap, sizeof(bmpInfo), &bmpInfo);
        int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
        memset(bmpInfo.bmBits, 0, bufferSize);
    }

#if !PLATFORM(WIN_CE)
    // Make sure we can do world transforms.
    SetGraphicsMode(bitmapDC, GM_ADVANCED);

    // Apply a translation to our context so that the drawing done will be at (0,0) of the bitmap.
    XFORM xform;
    xform.eM11 = 1.0f;
    xform.eM12 = 0.0f;
    xform.eM21 = 0.0f;
    xform.eM22 = 1.0f;
    xform.eDx = -dstRect.x();
    xform.eDy = -dstRect.y();
    ::SetWorldTransform(bitmapDC, &xform);
#endif

    return bitmapDC;
}

void GraphicsContext::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    // painting through native HDC is only supported for plugin, where mayCreateBitmap is always true
    Q_ASSERT(mayCreateBitmap);

    if (hdc) {

        if (!dstRect.isEmpty()) {

            HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));
            BITMAP info;
            GetObject(bitmap, sizeof(info), &info);
            ASSERT(info.bmBitsPixel == 32);

            QPixmap pixmap = QPixmap::fromWinHBITMAP(bitmap, supportAlphaBlend ? QPixmap::PremultipliedAlpha : QPixmap::NoAlpha);
            m_data->p()->drawPixmap(dstRect, pixmap);

            ::DeleteObject(bitmap);
        }

        ::DeleteDC(hdc);
    }
}
#endif

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality)
{
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    return InterpolationDefault;
}

}

// vim: ts=4 sw=4 et
