// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "Path.h"

#include "FloatRect.h"
#include "ImageBuffer.h"
#include "StrokeStyleApplier.h"

#include "SkPath.h"
#include "SkRegion.h"
#include "SkiaUtils.h"

#include <wtf/MathExtras.h>

namespace WebCore {

Path::Path()
{
    m_path = new SkPath;
}

Path::Path(const Path& other)
{
    m_path = new SkPath(*other.m_path);
}

Path::~Path()
{
    delete m_path;
}

Path& Path::operator=(const Path& other)
{
    *m_path = *other.m_path;
    return *this;
}

bool Path::isEmpty() const
{
    return m_path->isEmpty();
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    return SkPathContainsPoint(m_path, point,
      rule == RULE_NONZERO ? SkPath::kWinding_FillType : SkPath::kEvenOdd_FillType);
}

void Path::translate(const FloatSize& size)
{
    m_path->offset(WebCoreFloatToSkScalar(size.width()), WebCoreFloatToSkScalar(size.height()));
}

FloatRect Path::boundingRect() const
{
    SkRect rect;
    m_path->computeBounds(&rect, SkPath::kExact_BoundsType);
    return rect;
}

void Path::moveTo(const FloatPoint& point)
{
    m_path->moveTo(point);
}

void Path::addLineTo(const FloatPoint& point)
{
    m_path->lineTo(point);
}

void Path::addQuadCurveTo(const FloatPoint& cp, const FloatPoint& ep)
{
    m_path->quadTo(cp, ep);
}

void Path::addBezierCurveTo(const FloatPoint& p1, const FloatPoint& p2, const FloatPoint& ep)
{
    m_path->cubicTo(p1, p2, ep);
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    m_path->arcTo(p1, p2, WebCoreFloatToSkScalar(radius));
}

void Path::closeSubpath()
{
    m_path->close();
}

void Path::addArc(const FloatPoint& p, float r, float sa, float ea, bool anticlockwise) {
    SkScalar cx = WebCoreFloatToSkScalar(p.x());
    SkScalar cy = WebCoreFloatToSkScalar(p.y());
    SkScalar radius = WebCoreFloatToSkScalar(r);

    SkRect oval;
    oval.set(cx - radius, cy - radius, cx + radius, cy + radius);

    float sweep = ea - sa;
    // check for a circle
    if (sweep >= 2 * piFloat || sweep <= -2 * piFloat)
        m_path->addOval(oval);
    else {
        SkScalar startDegrees = WebCoreFloatToSkScalar(sa * 180 / piFloat);
        SkScalar sweepDegrees = WebCoreFloatToSkScalar(sweep * 180 / piFloat);

        // Counterclockwise arcs should be drawn with negative sweeps, while
        // clockwise arcs should be drawn with positive sweeps. Check to see
        // if the situation is reversed and correct it by adding or subtracting
        // a full circle
        if (anticlockwise && sweepDegrees > 0) {
            sweepDegrees -= SkIntToScalar(360);
        } else if (!anticlockwise && sweepDegrees < 0) {
            sweepDegrees += SkIntToScalar(360);
        }

        m_path->arcTo(oval, startDegrees, sweepDegrees, false);
    }
}

void Path::addRect(const FloatRect& rect)
{
    m_path->addRect(rect);
}

void Path::addEllipse(const FloatRect& rect)
{
    m_path->addOval(rect);
}

void Path::clear()
{
    m_path->reset();
}

static FloatPoint* convertPathPoints(FloatPoint dst[], const SkPoint src[], int count)
{
    for (int i = 0; i < count; i++) {
        dst[i].setX(SkScalarToFloat(src[i].fX));
        dst[i].setY(SkScalarToFloat(src[i].fY));
    }
    return dst;
}

void Path::apply(void* info, PathApplierFunction function) const
{
    SkPath::Iter iter(*m_path, false);
    SkPoint pts[4];
    PathElement pathElement;
    FloatPoint pathPoints[3];

    for (;;) {
        switch (iter.next(pts)) {
        case SkPath::kMove_Verb:
            pathElement.type = PathElementMoveToPoint;
            pathElement.points = convertPathPoints(pathPoints, &pts[0], 1);
            break;
        case SkPath::kLine_Verb:
            pathElement.type = PathElementAddLineToPoint;
            pathElement.points = convertPathPoints(pathPoints, &pts[1], 1);
            break;
        case SkPath::kQuad_Verb:
            pathElement.type = PathElementAddQuadCurveToPoint;
            pathElement.points = convertPathPoints(pathPoints, &pts[1], 2);
            break;
        case SkPath::kCubic_Verb:
            pathElement.type = PathElementAddCurveToPoint;
            pathElement.points = convertPathPoints(pathPoints, &pts[1], 3);
            break;
        case SkPath::kClose_Verb:
            pathElement.type = PathElementCloseSubpath;
            pathElement.points = convertPathPoints(pathPoints, 0, 0);
            break;
        case SkPath::kDone_Verb:
            return;
        }
        function(info, &pathElement);
    }
}

void Path::transform(const TransformationMatrix& xform)
{
    m_path->transform(xform);
}

String Path::debugString() const
{
    String result;

    SkPath::Iter iter(*m_path, false);
    SkPoint pts[4];

    int numPoints = m_path->getPoints(0, 0);
    SkPath::Verb verb;

    do {
        verb = iter.next(pts);
        switch (verb) {
        case SkPath::kMove_Verb:
            result += String::format("M%.2f,%.2f ", pts[0].fX, pts[0].fY);
            numPoints -= 1;
            break;
        case SkPath::kLine_Verb:
          if (!iter.isCloseLine()) {
                result += String::format("L%.2f,%.2f ", pts[1].fX, pts[1].fY); 
                numPoints -= 1;
            }
            break;
        case SkPath::kQuad_Verb:
            result += String::format("Q%.2f,%.2f,%.2f,%.2f ",
                pts[1].fX, pts[1].fY,
                pts[2].fX, pts[2].fY);
            numPoints -= 2;
            break;
        case SkPath::kCubic_Verb:
            result += String::format("C%.2f,%.2f,%.2f,%.2f,%.2f,%.2f ",
                pts[1].fX, pts[1].fY,
                pts[2].fX, pts[2].fY,
                pts[3].fX, pts[3].fY);
            numPoints -= 3;
            break;
        case SkPath::kClose_Verb:
            result += "Z ";
            break;
        case SkPath::kDone_Verb:
            break;
        }
    } while (verb != SkPath::kDone_Verb);

    // If you have a path that ends with an M, Skia will not iterate the
    // trailing M. That's nice of it, but Apple's paths output the trailing M
    // and we want out layout dumps to look like theirs
    if (numPoints) {
        ASSERT(numPoints==1);
        m_path->getLastPt(pts);
        result += String::format("M%.2f,%.2f ", pts[0].fX, pts[0].fY);
    }

    return result.stripWhiteSpace();
}

// Computes the bounding box for the stroke and style currently selected into
// the given bounding box. This also takes into account the stroke width.
static FloatRect boundingBoxForCurrentStroke(const GraphicsContext* context)
{
    SkPaint paint;
    context->platformContext()->setupPaintForStroking(&paint, 0, 0);
    SkPath boundingPath;
    paint.getFillPath(context->platformContext()->currentPath(), &boundingPath);
    SkRect r;
    boundingPath.computeBounds(&r, SkPath::kExact_BoundsType);
    return r;
}

FloatRect Path::strokeBoundingRect(StrokeStyleApplier* applier)
{
    GraphicsContext* scratch = scratchContext();
    scratch->save();
    scratch->beginPath();
    scratch->addPath(*this);

    if (applier)
        applier->strokeStyle(scratch);

    FloatRect r = boundingBoxForCurrentStroke(scratch);
    scratch->restore();
    return r;
}

bool Path::strokeContains(StrokeStyleApplier* applier, const FloatPoint& point) const
{
    ASSERT(applier);
    GraphicsContext* scratch = scratchContext();
    scratch->save();

    applier->strokeStyle(scratch);

    SkPaint paint;
    scratch->platformContext()->setupPaintForStroking(&paint, 0, 0);
    SkPath strokePath;
    paint.getFillPath(*platformPath(), &strokePath);
    bool contains = SkPathContainsPoint(&strokePath, point,
                                        SkPath::kWinding_FillType);

    scratch->restore();
    return contains;
}
} // namespace WebCore
