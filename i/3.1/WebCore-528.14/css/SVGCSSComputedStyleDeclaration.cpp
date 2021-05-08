/*
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>
    Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>

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

#if ENABLE(SVG)
#include "CSSComputedStyleDeclaration.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "Document.h"

namespace WebCore {

static PassRefPtr<CSSPrimitiveValue> glyphOrientationToCSSPrimitiveValue(EGlyphOrientation orientation)
{
    switch (orientation) {
        case GO_0DEG:
            return CSSPrimitiveValue::create(0.0f, CSSPrimitiveValue::CSS_DEG);
        case GO_90DEG:
            return CSSPrimitiveValue::create(90.0f, CSSPrimitiveValue::CSS_DEG);
        case GO_180DEG:
            return CSSPrimitiveValue::create(180.0f, CSSPrimitiveValue::CSS_DEG);
        case GO_270DEG:
            return CSSPrimitiveValue::create(270.0f, CSSPrimitiveValue::CSS_DEG);
        default:
            return 0;
    }
}

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getSVGPropertyCSSValue(int propertyID, EUpdateLayout updateLayout) const
{
    Node* node = m_node.get();
    if (!node)
        return 0;
    
    // Make sure our layout is up to date before we allow a query on these attributes.
    if (updateLayout)
        node->document()->updateLayout();
        
    RenderStyle* style = node->computedStyle();
    if (!style)
        return 0;
    
    const SVGRenderStyle* svgStyle = style->svgStyle();
    if (!svgStyle)
        return 0;
    
    switch (static_cast<CSSPropertyID>(propertyID)) {
        case CSSPropertyClipRule:
            return CSSPrimitiveValue::create(svgStyle->clipRule());
        case CSSPropertyFloodOpacity:
            return CSSPrimitiveValue::create(svgStyle->floodOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyStopOpacity:
            return CSSPrimitiveValue::create(svgStyle->stopOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyColorInterpolation:
            return CSSPrimitiveValue::create(svgStyle->colorInterpolation());
        case CSSPropertyColorInterpolationFilters:
            return CSSPrimitiveValue::create(svgStyle->colorInterpolationFilters());
        case CSSPropertyFillOpacity:
            return CSSPrimitiveValue::create(svgStyle->fillOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyFillRule:
            return CSSPrimitiveValue::create(svgStyle->fillRule());
        case CSSPropertyColorRendering:
            return CSSPrimitiveValue::create(svgStyle->colorRendering());
        case CSSPropertyImageRendering:
            return CSSPrimitiveValue::create(svgStyle->imageRendering());
        case CSSPropertyShapeRendering:
            return CSSPrimitiveValue::create(svgStyle->shapeRendering());
        case CSSPropertyStrokeLinecap:
            return CSSPrimitiveValue::create(svgStyle->capStyle());
        case CSSPropertyStrokeLinejoin:
            return CSSPrimitiveValue::create(svgStyle->joinStyle());
        case CSSPropertyStrokeMiterlimit:
            return CSSPrimitiveValue::create(svgStyle->strokeMiterLimit(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyStrokeOpacity:
            return CSSPrimitiveValue::create(svgStyle->strokeOpacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyTextRendering:
            return CSSPrimitiveValue::create(svgStyle->textRendering());
        case CSSPropertyAlignmentBaseline:
            return CSSPrimitiveValue::create(svgStyle->alignmentBaseline());
        case CSSPropertyDominantBaseline:
            return CSSPrimitiveValue::create(svgStyle->dominantBaseline());
        case CSSPropertyTextAnchor:
            return CSSPrimitiveValue::create(svgStyle->textAnchor());
        case CSSPropertyWritingMode:
            return CSSPrimitiveValue::create(svgStyle->writingMode());
        case CSSPropertyClipPath:
            if (!svgStyle->clipPath().isEmpty())
                return CSSPrimitiveValue::create(svgStyle->clipPath(), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyMask:
            if (!svgStyle->maskElement().isEmpty())
                return CSSPrimitiveValue::create(svgStyle->maskElement(), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyFilter:
            if (!svgStyle->filter().isEmpty())
                return CSSPrimitiveValue::create(svgStyle->filter(), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyFloodColor:
            return CSSPrimitiveValue::createColor(svgStyle->floodColor().rgb());
        case CSSPropertyLightingColor:
            return CSSPrimitiveValue::createColor(svgStyle->lightingColor().rgb());
        case CSSPropertyStopColor:
            return CSSPrimitiveValue::createColor(svgStyle->stopColor().rgb());
        case CSSPropertyFill:
            return svgStyle->fillPaint();
        case CSSPropertyKerning:
            return svgStyle->kerning();
        case CSSPropertyMarkerEnd:
            if (!svgStyle->endMarker().isEmpty())
                return CSSPrimitiveValue::create(svgStyle->endMarker(), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyMarkerMid:
            if (!svgStyle->midMarker().isEmpty())
                return CSSPrimitiveValue::create(svgStyle->midMarker(), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyMarkerStart:
            if (!svgStyle->startMarker().isEmpty())
                return CSSPrimitiveValue::create(svgStyle->startMarker(), CSSPrimitiveValue::CSS_URI);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyStroke:
            return svgStyle->strokePaint();
        case CSSPropertyStrokeDasharray:
            return svgStyle->strokeDashArray();
        case CSSPropertyStrokeDashoffset:
            return svgStyle->strokeDashOffset();
        case CSSPropertyStrokeWidth:
            return svgStyle->strokeWidth();
        case CSSPropertyBaselineShift: {
            switch (svgStyle->baselineShift()) {
                case BS_BASELINE:
                    return CSSPrimitiveValue::createIdentifier(CSSValueBaseline);
                case BS_SUPER:
                    return CSSPrimitiveValue::createIdentifier(CSSValueSuper);
                case BS_SUB:
                    return CSSPrimitiveValue::createIdentifier(CSSValueSub);
                case BS_LENGTH:
                    return svgStyle->baselineShiftValue();
            }
        }
        case CSSPropertyGlyphOrientationHorizontal:
            return glyphOrientationToCSSPrimitiveValue(svgStyle->glyphOrientationHorizontal());
        case CSSPropertyGlyphOrientationVertical: {
            if (RefPtr<CSSPrimitiveValue> value = glyphOrientationToCSSPrimitiveValue(svgStyle->glyphOrientationVertical()))
                return value.release();

            if (svgStyle->glyphOrientationVertical() == GO_AUTO)
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);

            return 0;
        }
        case CSSPropertyMarker:
        case CSSPropertyEnableBackground:
        case CSSPropertyColorProfile:
            // the above properties are not yet implemented in the engine
            break;
    default:
        // If you crash here, it's because you added a css property and are not handling it
        // in either this switch statement or the one in CSSComputedStyleDelcaration::getPropertyCSSValue
        ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", propertyID);
    }
    LOG_ERROR("unimplemented propertyID: %d", propertyID);
    return 0;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
