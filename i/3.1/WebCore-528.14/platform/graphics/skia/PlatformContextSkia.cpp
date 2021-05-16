/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "GraphicsContext.h"
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#include "SkiaUtils.h"

#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"

#include "SkBitmap.h"
#include "SkColorPriv.h"
#include "SkShader.h"
#include "SkDashPathEffect.h"

#include <wtf/MathExtras.h>

#if defined(__linux__)
#include "GdkSkia.h"
#endif

// State -----------------------------------------------------------------------

// Encapsulates the additional painting state information we store for each
// pushed graphics state.
struct PlatformContextSkia::State {
    State();
    State(const State&);
    ~State();

    // Common shader state.
    float m_alpha;
    SkPorterDuff::Mode m_porterDuffMode;
    SkShader* m_gradient;
    SkShader* m_pattern;
    bool m_useAntialiasing;
    SkDrawLooper* m_looper;

    // Fill.
    SkColor m_fillColor;

    // Stroke.
    WebCore::StrokeStyle m_strokeStyle;
    SkColor m_strokeColor;
    float m_strokeThickness;
    int m_dashRatio;  // Ratio of the length of a dash to its width.
    float m_miterLimit;
    SkPaint::Cap m_lineCap;
    SkPaint::Join m_lineJoin;
    SkDashPathEffect* m_dash;

    // Text. (See cTextFill & friends in GraphicsContext.h.)
    int m_textDrawingMode;

    // Helper function for applying the state's alpha value to the given input
    // color to produce a new output color.
    SkColor applyAlpha(SkColor) const;

private:
    // Not supported.
    void operator=(const State&);
};

// Note: Keep theses default values in sync with GraphicsContextState.
PlatformContextSkia::State::State()
    : m_alpha(1)
    , m_porterDuffMode(SkPorterDuff::kSrcOver_Mode)
    , m_gradient(0)
    , m_pattern(0)
    , m_useAntialiasing(true)
    , m_looper(0)
    , m_fillColor(0xFF000000)
    , m_strokeStyle(WebCore::SolidStroke)
    , m_strokeColor(WebCore::Color::black)
    , m_strokeThickness(0)
    , m_dashRatio(3)
    , m_miterLimit(4)
    , m_lineCap(SkPaint::kDefault_Cap)
    , m_lineJoin(SkPaint::kDefault_Join)
    , m_dash(0)
    , m_textDrawingMode(WebCore::cTextFill)
{
}

PlatformContextSkia::State::State(const State& other)
{
    memcpy(this, &other, sizeof(State));

    m_looper->safeRef();
    m_dash->safeRef();
    m_gradient->safeRef();
    m_pattern->safeRef();
}

PlatformContextSkia::State::~State()
{
    m_looper->safeUnref();
    m_dash->safeUnref();
    m_gradient->safeUnref();
    m_pattern->safeUnref();
}

SkColor PlatformContextSkia::State::applyAlpha(SkColor c) const
{
    int s = roundf(m_alpha * 256);
    if (s >= 256)
        return c;
    if (s < 0)
        return 0;

    int a = SkAlphaMul(SkColorGetA(c), s);
    return (c & 0x00FFFFFF) | (a << 24);
}

// PlatformContextSkia ---------------------------------------------------------

// Danger: canvas can be NULL.
PlatformContextSkia::PlatformContextSkia(skia::PlatformCanvas* canvas)
    : m_canvas(canvas)
    , m_stateStack(sizeof(State))
{
    m_stateStack.append(State());
    m_state = &m_stateStack.last();
#if defined(OS_LINUX)
    m_gdkskia = m_canvas ? gdk_skia_new(m_canvas) : 0;
#endif
}

PlatformContextSkia::~PlatformContextSkia()
{
#if defined(OS_LINUX)
    if (m_gdkskia) {
        g_object_unref(m_gdkskia);
        m_gdkskia = 0;
    }
#endif
}

void PlatformContextSkia::setCanvas(skia::PlatformCanvas* canvas)
{
    m_canvas = canvas;
}

void PlatformContextSkia::save()
{
    m_stateStack.append(*m_state);
    m_state = &m_stateStack.last();

    // Save our native canvas.
    canvas()->save();
}

void PlatformContextSkia::restore()
{
    m_stateStack.removeLast();
    m_state = &m_stateStack.last();

    // Restore our native canvas.
    canvas()->restore();
}

void PlatformContextSkia::drawRect(SkRect rect)
{
    SkPaint paint;
    int fillcolorNotTransparent = m_state->m_fillColor & 0xFF000000;
    if (fillcolorNotTransparent) {
        setupPaintForFilling(&paint);
        canvas()->drawRect(rect, paint);
    }

    if (m_state->m_strokeStyle != WebCore::NoStroke &&
        (m_state->m_strokeColor & 0xFF000000)) {
        if (fillcolorNotTransparent) {
            // This call is expensive so don't call it unnecessarily.
            paint.reset();
        }
        setupPaintForStroking(&paint, &rect, 0);
        canvas()->drawRect(rect, paint);
    }
}

void PlatformContextSkia::setupPaintCommon(SkPaint* paint) const
{
#ifdef SK_DEBUGx
    {
        SkPaint defaultPaint;
        SkASSERT(*paint == defaultPaint);
    }
#endif

    paint->setAntiAlias(m_state->m_useAntialiasing);
    paint->setPorterDuffXfermode(m_state->m_porterDuffMode);
    paint->setLooper(m_state->m_looper);

    if (m_state->m_gradient)
        paint->setShader(m_state->m_gradient);
    else if (m_state->m_pattern)
        paint->setShader(m_state->m_pattern);
}

void PlatformContextSkia::setupPaintForFilling(SkPaint* paint) const
{
    setupPaintCommon(paint);
    paint->setColor(m_state->applyAlpha(m_state->m_fillColor));
}

float PlatformContextSkia::setupPaintForStroking(SkPaint* paint, SkRect* rect, int length) const
{
    setupPaintCommon(paint);
    float width = m_state->m_strokeThickness;

    // This allows dashing and dotting to work properly for hairline strokes.
    if (width == 0)
        width = 1;

    paint->setColor(m_state->applyAlpha(m_state->m_strokeColor));
    paint->setStyle(SkPaint::kStroke_Style);
    paint->setStrokeWidth(SkFloatToScalar(width));
    paint->setStrokeCap(m_state->m_lineCap);
    paint->setStrokeJoin(m_state->m_lineJoin);
    paint->setStrokeMiter(SkFloatToScalar(m_state->m_miterLimit));

    if (rect != 0 && (static_cast<int>(roundf(width)) & 1))
        rect->inset(-SK_ScalarHalf, -SK_ScalarHalf);

    if (m_state->m_dash)
        paint->setPathEffect(m_state->m_dash);
    else {
        switch (m_state->m_strokeStyle) {
        case WebCore::NoStroke:
        case WebCore::SolidStroke:
            break;
        case WebCore::DashedStroke:
            width = m_state->m_dashRatio * width;
            // Fall through.
        case WebCore::DottedStroke:
            SkScalar dashLength;
            if (length) {
                // Determine about how many dashes or dots we should have.
                int numDashes = length / roundf(width);
                if (!(numDashes & 1))
                    numDashes++;    // Make it odd so we end on a dash/dot.
                // Use the number of dashes to determine the length of a
                // dash/dot, which will be approximately width
                dashLength = SkScalarDiv(SkIntToScalar(length), SkIntToScalar(numDashes));
            } else
                dashLength = SkFloatToScalar(width);
            SkScalar intervals[2] = { dashLength, dashLength };
            paint->setPathEffect(new SkDashPathEffect(intervals, 2, 0))->unref();
        }
    }

    return width;
}

void PlatformContextSkia::setDrawLooper(SkDrawLooper* dl)
{
    SkRefCnt_SafeAssign(m_state->m_looper, dl);
}

void PlatformContextSkia::setMiterLimit(float ml)
{
    m_state->m_miterLimit = ml;
}

void PlatformContextSkia::setAlpha(float alpha)
{
    m_state->m_alpha = alpha;
}

void PlatformContextSkia::setLineCap(SkPaint::Cap lc)
{
    m_state->m_lineCap = lc;
}

void PlatformContextSkia::setLineJoin(SkPaint::Join lj)
{
    m_state->m_lineJoin = lj;
}

void PlatformContextSkia::setPorterDuffMode(SkPorterDuff::Mode pdm)
{
    m_state->m_porterDuffMode = pdm;
}

void PlatformContextSkia::setFillColor(SkColor color)
{
    m_state->m_fillColor = color;
}

SkDrawLooper* PlatformContextSkia::getDrawLooper() const
{
    return m_state->m_looper;
}

WebCore::StrokeStyle PlatformContextSkia::getStrokeStyle() const
{
    return m_state->m_strokeStyle;
}

void PlatformContextSkia::setStrokeStyle(WebCore::StrokeStyle strokeStyle)
{
    m_state->m_strokeStyle = strokeStyle;
}

void PlatformContextSkia::setStrokeColor(SkColor strokeColor)
{
    m_state->m_strokeColor = strokeColor;
}

float PlatformContextSkia::getStrokeThickness() const
{
    return m_state->m_strokeThickness;
}

void PlatformContextSkia::setStrokeThickness(float thickness)
{
    m_state->m_strokeThickness = thickness;
}

int PlatformContextSkia::getTextDrawingMode() const
{
    return m_state->m_textDrawingMode;
}

void PlatformContextSkia::setTextDrawingMode(int mode)
{
  // cTextClip is never used, so we assert that it isn't set:
  // https://bugs.webkit.org/show_bug.cgi?id=21898
  ASSERT((mode & WebCore::cTextClip) == 0);
  m_state->m_textDrawingMode = mode;
}

void PlatformContextSkia::setUseAntialiasing(bool enable)
{
    m_state->m_useAntialiasing = enable;
}

SkColor PlatformContextSkia::fillColor() const
{
    return m_state->m_fillColor;
}

void PlatformContextSkia::beginPath()
{
    m_path.reset();
}

void PlatformContextSkia::addPath(const SkPath& path)
{
    m_path.addPath(path);
}

void PlatformContextSkia::setFillRule(SkPath::FillType fr)
{
    m_path.setFillType(fr);
}

void PlatformContextSkia::setGradient(SkShader* gradient)
{
    if (gradient != m_state->m_gradient) {
        m_state->m_gradient->safeUnref();
        m_state->m_gradient = gradient;
    }
}

void PlatformContextSkia::setPattern(SkShader* pattern)
{
    if (pattern != m_state->m_pattern) {
        m_state->m_pattern->safeUnref();
        m_state->m_pattern = pattern;
    }
}

void PlatformContextSkia::setDashPathEffect(SkDashPathEffect* dash)
{
    if (dash != m_state->m_dash) {
        m_state->m_dash->safeUnref();
        m_state->m_dash = dash;
    }
}

void PlatformContextSkia::paintSkPaint(const SkRect& rect,
                                       const SkPaint& paint)
{
    m_canvas->drawRect(rect, paint);
}

const SkBitmap* PlatformContextSkia::bitmap() const
{
    return &m_canvas->getDevice()->accessBitmap(false);
}

bool PlatformContextSkia::isPrinting()
{
    return m_canvas->getTopPlatformDevice().IsVectorial();
}
