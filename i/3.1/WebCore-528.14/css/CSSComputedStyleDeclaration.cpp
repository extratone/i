/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"
#include "CSSComputedStyleDeclaration.h"

#include "AnimationController.h"
#include "CSSBorderImageValue.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSReflectValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueList.h"
#include "CachedImage.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Pair.h"
#include "Rect.h"
#include "RenderBox.h"
#include "RenderLayer.h"
#include "ShadowValue.h"
#include "WebKitCSSTransformValue.h"


#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

namespace WebCore {

// List of all properties we know how to compute, omitting shorthands.
static const int computedProperties[] = {
    CSSPropertyBackgroundAttachment,
    CSSPropertyBackgroundColor,
    CSSPropertyBackgroundImage,
    // more specific background-position-x/y are non-standard
    CSSPropertyBackgroundPosition,
    CSSPropertyBackgroundRepeat,
    CSSPropertyBorderBottomColor,
    CSSPropertyBorderBottomStyle,
    CSSPropertyBorderBottomWidth,
    CSSPropertyBorderCollapse,
    CSSPropertyBorderLeftColor,
    CSSPropertyBorderLeftStyle,
    CSSPropertyBorderLeftWidth,
    CSSPropertyBorderRightColor,
    CSSPropertyBorderRightStyle,
    CSSPropertyBorderRightWidth,
    CSSPropertyBorderTopColor,
    CSSPropertyBorderTopStyle,
    CSSPropertyBorderTopWidth,
    CSSPropertyBottom,
    CSSPropertyCaptionSide,
    CSSPropertyClear,
    CSSPropertyColor,
    CSSPropertyCursor,
    CSSPropertyDirection,
    CSSPropertyDisplay,
    CSSPropertyEmptyCells,
    CSSPropertyFloat,
    CSSPropertyFontFamily,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
    CSSPropertyFontVariant,
    CSSPropertyFontWeight,
    CSSPropertyHeight,
    CSSPropertyLeft,
    CSSPropertyLetterSpacing,
    CSSPropertyLineHeight,
    CSSPropertyListStyleImage,
    CSSPropertyListStylePosition,
    CSSPropertyListStyleType,
    CSSPropertyMarginBottom,
    CSSPropertyMarginLeft,
    CSSPropertyMarginRight,
    CSSPropertyMarginTop,
    CSSPropertyMaxHeight,
    CSSPropertyMaxWidth,
    CSSPropertyMinHeight,
    CSSPropertyMinWidth,
    CSSPropertyOpacity,
    CSSPropertyOrphans,
    CSSPropertyOutlineColor,
    CSSPropertyOutlineStyle,
    CSSPropertyOutlineWidth,
    CSSPropertyOverflowX,
    CSSPropertyOverflowY,
    CSSPropertyPaddingBottom,
    CSSPropertyPaddingLeft,
    CSSPropertyPaddingRight,
    CSSPropertyPaddingTop,
    CSSPropertyPageBreakAfter,
    CSSPropertyPageBreakBefore,
    CSSPropertyPageBreakInside,
    CSSPropertyPointerEvents,
    CSSPropertyPosition,
    CSSPropertyResize,
    CSSPropertyRight,
    CSSPropertyTableLayout,
    CSSPropertyTextAlign,
    CSSPropertyTextDecoration,
    CSSPropertyTextIndent,
    CSSPropertyTextShadow,
    CSSPropertyTextTransform,
    CSSPropertyTop,
    CSSPropertyUnicodeBidi,
    CSSPropertyVerticalAlign,
    CSSPropertyVisibility,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWidth,
    CSSPropertyWordSpacing,
    CSSPropertyWordWrap,
    CSSPropertyZIndex,
    CSSPropertyZoom,

    CSSPropertyWebkitAnimationDelay,
    CSSPropertyWebkitAnimationDirection,
    CSSPropertyWebkitAnimationDuration,
    CSSPropertyWebkitAnimationIterationCount,
    CSSPropertyWebkitAnimationName,
    CSSPropertyWebkitAnimationPlayState,
    CSSPropertyWebkitAnimationTimingFunction,
    CSSPropertyWebkitAppearance,
    CSSPropertyWebkitBackfaceVisibility,
    CSSPropertyWebkitBackgroundClip,
    CSSPropertyWebkitBackgroundComposite,
    CSSPropertyWebkitBackgroundOrigin,
    CSSPropertyWebkitBackgroundSize,
    CSSPropertyWebkitBorderFit,
    CSSPropertyWebkitBorderImage,
    CSSPropertyWebkitBorderHorizontalSpacing,
    CSSPropertyWebkitBorderVerticalSpacing,
    CSSPropertyWebkitBoxAlign,
    CSSPropertyWebkitBoxDirection,
    CSSPropertyWebkitBoxFlex,
    CSSPropertyWebkitBoxFlexGroup,
    CSSPropertyWebkitBoxLines,
    CSSPropertyWebkitBoxOrdinalGroup,
    CSSPropertyWebkitBoxOrient,
    CSSPropertyWebkitBoxPack,
    CSSPropertyWebkitBoxReflect,
    CSSPropertyWebkitBoxShadow,
    CSSPropertyWebkitBoxSizing,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyWebkitColumnCount,
    CSSPropertyWebkitColumnGap,
    CSSPropertyWebkitColumnRuleColor,
    CSSPropertyWebkitColumnRuleStyle,
    CSSPropertyWebkitColumnRuleWidth,
    CSSPropertyWebkitColumnWidth,
    CSSPropertyWebkitHighlight,
    CSSPropertyWebkitLineBreak,
    CSSPropertyWebkitLineClamp,
    CSSPropertyWebkitMarginBottomCollapse,
    CSSPropertyWebkitMarginTopCollapse,
    CSSPropertyWebkitMarqueeDirection,
    CSSPropertyWebkitMarqueeIncrement,
    CSSPropertyWebkitMarqueeRepetition,
    CSSPropertyWebkitMarqueeStyle,
    CSSPropertyWebkitMaskAttachment,
    CSSPropertyWebkitMaskBoxImage,
    CSSPropertyWebkitMaskImage,
    CSSPropertyWebkitMaskPosition,
    CSSPropertyWebkitMaskRepeat,
    CSSPropertyWebkitMaskClip,
    CSSPropertyWebkitMaskComposite,
    CSSPropertyWebkitMaskOrigin,
    CSSPropertyWebkitMaskSize,
    CSSPropertyWebkitNbspMode,
    CSSPropertyWebkitPerspective,
    CSSPropertyWebkitPerspectiveOrigin,
    CSSPropertyWebkitRtlOrdering,
    CSSPropertyWebkitTapHighlightColor,
    CSSPropertyWebkitTouchCallout,
    CSSPropertyWebkitCompositionFillColor,
    CSSPropertyWebkitCompositionFrameColor,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextSecurity,
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,
    CSSPropertyWebkitTransform,
    CSSPropertyWebkitTransformOrigin,
    CSSPropertyWebkitTransformStyle,
    CSSPropertyWebkitTransitionDelay,
    CSSPropertyWebkitTransitionDuration,
    CSSPropertyWebkitTransitionProperty,
    CSSPropertyWebkitTransitionTimingFunction,
    CSSPropertyWebkitUserDrag,
    CSSPropertyWebkitUserModify,
    CSSPropertyWebkitUserSelect,
#if ENABLE(DASHBOARD_SUPPORT)
    CSSPropertyWebkitDashboardRegion,
#endif
    CSSPropertyWebkitBorderBottomLeftRadius,
    CSSPropertyWebkitBorderBottomRightRadius,
    CSSPropertyWebkitBorderTopLeftRadius,
    CSSPropertyWebkitBorderTopRightRadius
    
#if ENABLE(SVG)
    ,
    CSSPropertyClipPath,
    CSSPropertyClipRule,
    CSSPropertyMask,
    CSSPropertyFilter,
    CSSPropertyFloodColor,
    CSSPropertyFloodOpacity,
    CSSPropertyLightingColor,
    CSSPropertyStopColor,
    CSSPropertyStopOpacity,
    CSSPropertyColorInterpolation,
    CSSPropertyColorInterpolationFilters,
    CSSPropertyColorRendering,
    CSSPropertyFill,
    CSSPropertyFillOpacity,
    CSSPropertyFillRule,
    CSSPropertyImageRendering,
    CSSPropertyMarkerEnd,
    CSSPropertyMarkerMid,
    CSSPropertyMarkerStart,
    CSSPropertyShapeRendering,
    CSSPropertyStroke,
    CSSPropertyStrokeDasharray,
    CSSPropertyStrokeDashoffset,
    CSSPropertyStrokeLinecap,
    CSSPropertyStrokeLinejoin,
    CSSPropertyStrokeMiterlimit,
    CSSPropertyStrokeOpacity,
    CSSPropertyStrokeWidth,
    CSSPropertyTextRendering,
    CSSPropertyAlignmentBaseline,
    CSSPropertyBaselineShift,
    CSSPropertyDominantBaseline,
    CSSPropertyKerning,
    CSSPropertyTextAnchor,
    CSSPropertyWritingMode,
    CSSPropertyGlyphOrientationHorizontal,
    CSSPropertyGlyphOrientationVertical
#endif
};

const unsigned numComputedProperties = sizeof(computedProperties) / sizeof(computedProperties[0]);

static PassRefPtr<CSSValue> valueForShadow(const ShadowData* shadow)
{
    if (!shadow)
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);

    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    for (const ShadowData* s = shadow; s; s = s->next) {
        RefPtr<CSSPrimitiveValue> x = CSSPrimitiveValue::create(s->x, CSSPrimitiveValue::CSS_PX);
        RefPtr<CSSPrimitiveValue> y = CSSPrimitiveValue::create(s->y, CSSPrimitiveValue::CSS_PX);
        RefPtr<CSSPrimitiveValue> blur = CSSPrimitiveValue::create(s->blur, CSSPrimitiveValue::CSS_PX);
        RefPtr<CSSPrimitiveValue> color = CSSPrimitiveValue::createColor(s->color.rgb());
        list->prepend(ShadowValue::create(x.release(), y.release(), blur.release(), color.release()));
    }
    return list.release();
}

static int valueForRepeatRule(int rule)
{
    switch (rule) {
        case RepeatImageRule:
            return CSSValueRepeat;
        case RoundImageRule:
            return CSSValueRound;
        default:
            return CSSValueStretch;
    }
}
        
static PassRefPtr<CSSValue> valueForNinePieceImage(const NinePieceImage& image)
{
    if (!image.hasImage())
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    
    // Image first.
    RefPtr<CSSValue> imageValue;
    if (image.image())
        imageValue = image.image()->cssValue();
    
    // Create the slices.
    RefPtr<CSSPrimitiveValue> top;
    if (image.m_slices.top().isPercent())
        top = CSSPrimitiveValue::create(image.m_slices.top().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        top = CSSPrimitiveValue::create(image.m_slices.top().value(), CSSPrimitiveValue::CSS_NUMBER);
        
    RefPtr<CSSPrimitiveValue> right;
    if (image.m_slices.right().isPercent())
        right = CSSPrimitiveValue::create(image.m_slices.right().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        right = CSSPrimitiveValue::create(image.m_slices.right().value(), CSSPrimitiveValue::CSS_NUMBER);
        
    RefPtr<CSSPrimitiveValue> bottom;
    if (image.m_slices.bottom().isPercent())
        bottom = CSSPrimitiveValue::create(image.m_slices.bottom().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        bottom = CSSPrimitiveValue::create(image.m_slices.bottom().value(), CSSPrimitiveValue::CSS_NUMBER);
    
    RefPtr<CSSPrimitiveValue> left;
    if (image.m_slices.left().isPercent())
        left = CSSPrimitiveValue::create(image.m_slices.left().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        left = CSSPrimitiveValue::create(image.m_slices.left().value(), CSSPrimitiveValue::CSS_NUMBER);

    RefPtr<Rect> rect = Rect::create();
    rect->setTop(top);
    rect->setRight(right);
    rect->setBottom(bottom);
    rect->setLeft(left);

    return CSSBorderImageValue::create(imageValue, rect, valueForRepeatRule(image.m_horizontalRule), valueForRepeatRule(image.m_verticalRule));
}

static PassRefPtr<CSSValue> valueForReflection(const StyleReflection* reflection)
{
    if (!reflection)
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);

    RefPtr<CSSPrimitiveValue> offset;
    if (reflection->offset().isPercent())
        offset = CSSPrimitiveValue::create(reflection->offset().percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        offset = CSSPrimitiveValue::create(reflection->offset().value(), CSSPrimitiveValue::CSS_PX);
    
    return CSSReflectValue::create(reflection->direction(), offset.release(), valueForNinePieceImage(reflection->mask()));
}

static PassRefPtr<CSSValue> getPositionOffsetValue(RenderStyle* style, int propertyID)
{
    if (!style)
        return 0;

    Length l;
    switch (propertyID) {
        case CSSPropertyLeft:
            l = style->left();
            break;
        case CSSPropertyRight:
            l = style->right();
            break;
        case CSSPropertyTop:
            l = style->top();
            break;
        case CSSPropertyBottom:
            l = style->bottom();
            break;
        default:
            return 0;
    }

    if (style->position() == AbsolutePosition || style->position() == FixedPosition)
        return CSSPrimitiveValue::create(l);

    if (style->position() == RelativePosition)
        // FIXME: It's not enough to simply return "auto" values for one offset if the other side is defined.
        // In other words if left is auto and right is not auto, then left's computed value is negative right().
        // So we should get the opposite length unit and see if it is auto.
        return CSSPrimitiveValue::create(l);

    return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
}

static PassRefPtr<CSSPrimitiveValue> currentColorOrValidColor(RenderStyle* style, const Color& color)
{
    if (!color.isValid())
        return CSSPrimitiveValue::createColor(style->color().rgb());
    return CSSPrimitiveValue::createColor(color.rgb());
}

static PassRefPtr<CSSValue> getBorderRadiusCornerValue(IntSize radius)
{
    if (radius.width() == radius.height())
        return CSSPrimitiveValue::create(radius.width(), CSSPrimitiveValue::CSS_PX);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::create(radius.width(), CSSPrimitiveValue::CSS_PX));
    list->append(CSSPrimitiveValue::create(radius.height(), CSSPrimitiveValue::CSS_PX));
    return list.release();
}

static IntRect sizingBox(RenderObject* renderer)
{
    if (!renderer->isBox())
        return IntRect();
    
    RenderBox* box = toRenderBox(renderer);
    return box->style()->boxSizing() == CONTENT_BOX ? box->contentBoxRect() : box->borderBoxRect();
}

static inline bool hasCompositedLayer(RenderObject* renderer)
{
    return renderer && renderer->hasLayer() && toRenderBox(renderer)->layer()->isComposited();
}

static PassRefPtr<CSSValue> computedTransform(RenderObject* renderer, const RenderStyle* style)
{
    if (!renderer || style->transform().operations().isEmpty())
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    
    IntRect box = sizingBox(renderer);

    TransformationMatrix transform;
    style->applyTransform(transform, box.size(), RenderStyle::ExcludeTransformOrigin);
    // Note that this does not flatten to an affine transform if ENABLE(3D_RENDERING) is off, by design.

    RefPtr<WebKitCSSTransformValue> transformVal;

    // FIXME: Need to print out individual functions (https://bugs.webkit.org/show_bug.cgi?id=23924)
    if (transform.isAffine()) {
        transformVal = WebKitCSSTransformValue::create(WebKitCSSTransformValue::MatrixTransformOperation);

        transformVal->append(CSSPrimitiveValue::create(transform.a(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.b(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.c(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.d(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.e(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.f(), CSSPrimitiveValue::CSS_NUMBER));
    } else {
        transformVal = WebKitCSSTransformValue::create(WebKitCSSTransformValue::Matrix3DTransformOperation);

        transformVal->append(CSSPrimitiveValue::create(transform.m11(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m12(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m13(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m14(), CSSPrimitiveValue::CSS_NUMBER));

        transformVal->append(CSSPrimitiveValue::create(transform.m21(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m22(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m23(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m24(), CSSPrimitiveValue::CSS_NUMBER));

        transformVal->append(CSSPrimitiveValue::create(transform.m31(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m32(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m33(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m34(), CSSPrimitiveValue::CSS_NUMBER));

        transformVal->append(CSSPrimitiveValue::create(transform.m41(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m42(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m43(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m44(), CSSPrimitiveValue::CSS_NUMBER));
    }

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(transformVal);

    return list.release();
}

static PassRefPtr<CSSValue> getDelayValue(const AnimationList* animList)
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list->append(CSSPrimitiveValue::create(animList->animation(i)->delay(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDelay() is used for both transitions and animations
        list->append(CSSPrimitiveValue::create(Animation::initialAnimationDelay(), CSSPrimitiveValue::CSS_S));
    }
    return list.release();
}

static PassRefPtr<CSSValue> getDurationValue(const AnimationList* animList)
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list->append(CSSPrimitiveValue::create(animList->animation(i)->duration(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDuration() is used for both transitions and animations
        list->append(CSSPrimitiveValue::create(Animation::initialAnimationDuration(), CSSPrimitiveValue::CSS_S));
    }
    return list.release();
}

static PassRefPtr<CSSValue> getTimingFunctionValue(const AnimationList* animList)
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i) {
            const TimingFunction& tf = animList->animation(i)->timingFunction();
            list->append(CSSTimingFunctionValue::create(tf.x1(), tf.y1(), tf.x2(), tf.y2()));
        }
    } else {
        // Note that initialAnimationTimingFunction() is used for both transitions and animations
        const TimingFunction& tf = Animation::initialAnimationTimingFunction();
        list->append(CSSTimingFunctionValue::create(tf.x1(), tf.y1(), tf.x2(), tf.y2()));
    }
    return list.release();
}

CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(PassRefPtr<Node> n)
    : m_node(n)
{
}

CSSComputedStyleDeclaration::~CSSComputedStyleDeclaration()
{
}

String CSSComputedStyleDeclaration::cssText() const
{
    String result("");

    for (unsigned i = 0; i < numComputedProperties; i++) {
        if (i)
            result += " ";
        result += getPropertyName(static_cast<CSSPropertyID>(computedProperties[i]));
        result += ": ";
        result += getPropertyValue(computedProperties[i]);
        result += ";";
    }

    return result;
}

void CSSComputedStyleDeclaration::setCssText(const String&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(int propertyID) const
{
    return getPropertyCSSValue(propertyID, UpdateLayout);
}

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(int propertyID, EUpdateLayout updateLayout) const
{
    Node* node = m_node.get();
    if (!node)
        return 0;

    // Make sure our layout is up to date before we allow a query on these attributes.
    if (updateLayout)
        node->document()->updateLayoutIgnorePendingStylesheets();

    RenderObject* renderer = node->renderer();

    RefPtr<RenderStyle> style;
    if (renderer && hasCompositedLayer(renderer) && AnimationController::supportsAcceleratedAnimationOfProperty(static_cast<CSSPropertyID>(propertyID)))
        style = renderer->animation()->getAnimatedStyleForRenderer(renderer);
    else
       style = node->computedStyle();
    if (!style)
        return 0;

    switch (static_cast<CSSPropertyID>(propertyID)) {
        case CSSPropertyInvalid:
            break;

        case CSSPropertyBackgroundColor:
            return CSSPrimitiveValue::createColor(style->backgroundColor().rgb());
        case CSSPropertyBackgroundImage:
            if (style->backgroundImage())
                return style->backgroundImage()->cssValue();
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyWebkitBackgroundSize: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            list->append(CSSPrimitiveValue::create(style->backgroundSize().width()));
            list->append(CSSPrimitiveValue::create(style->backgroundSize().height()));
            return list.release();
        }  
        case CSSPropertyBackgroundRepeat:
            return CSSPrimitiveValue::create(style->backgroundRepeat());
        case CSSPropertyWebkitBackgroundComposite:
            return CSSPrimitiveValue::create(style->backgroundComposite());
        case CSSPropertyBackgroundAttachment:
            if (style->backgroundAttachment())
                return CSSPrimitiveValue::createIdentifier(CSSValueScroll);
            return CSSPrimitiveValue::createIdentifier(CSSValueFixed);
        case CSSPropertyWebkitBackgroundClip:
        case CSSPropertyWebkitBackgroundOrigin: {
            EFillBox box = (propertyID == CSSPropertyWebkitBackgroundClip ? style->backgroundClip() : style->backgroundOrigin());
            return CSSPrimitiveValue::create(box);
        }
        case CSSPropertyBackgroundPosition: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

            list->append(CSSPrimitiveValue::create(style->backgroundXPosition()));
            list->append(CSSPrimitiveValue::create(style->backgroundYPosition()));

            return list.release();
        }
        case CSSPropertyBackgroundPositionX:
            return CSSPrimitiveValue::create(style->backgroundXPosition());
        case CSSPropertyBackgroundPositionY:
            return CSSPrimitiveValue::create(style->backgroundYPosition());
        case CSSPropertyBorderCollapse:
            if (style->borderCollapse())
                return CSSPrimitiveValue::createIdentifier(CSSValueCollapse);
            return CSSPrimitiveValue::createIdentifier(CSSValueSeparate);
        case CSSPropertyBorderSpacing: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            list->append(CSSPrimitiveValue::create(style->horizontalBorderSpacing(), CSSPrimitiveValue::CSS_PX));
            list->append(CSSPrimitiveValue::create(style->verticalBorderSpacing(), CSSPrimitiveValue::CSS_PX));
            return list.release();
        }  
        case CSSPropertyWebkitBorderHorizontalSpacing:
            return CSSPrimitiveValue::create(style->horizontalBorderSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyWebkitBorderVerticalSpacing:
            return CSSPrimitiveValue::create(style->verticalBorderSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyBorderTopColor:
            return currentColorOrValidColor(style.get(), style->borderTopColor());
        case CSSPropertyBorderRightColor:
            return currentColorOrValidColor(style.get(), style->borderRightColor());
        case CSSPropertyBorderBottomColor:
            return currentColorOrValidColor(style.get(), style->borderBottomColor());
        case CSSPropertyBorderLeftColor:
            return currentColorOrValidColor(style.get(), style->borderLeftColor());
        case CSSPropertyBorderTopStyle:
            return CSSPrimitiveValue::create(style->borderTopStyle());
        case CSSPropertyBorderRightStyle:
            return CSSPrimitiveValue::create(style->borderRightStyle());
        case CSSPropertyBorderBottomStyle:
            return CSSPrimitiveValue::create(style->borderBottomStyle());
        case CSSPropertyBorderLeftStyle:
            return CSSPrimitiveValue::create(style->borderLeftStyle());
        case CSSPropertyBorderTopWidth:
            return CSSPrimitiveValue::create(style->borderTopWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyBorderRightWidth:
            return CSSPrimitiveValue::create(style->borderRightWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyBorderBottomWidth:
            return CSSPrimitiveValue::create(style->borderBottomWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyBorderLeftWidth:
            return CSSPrimitiveValue::create(style->borderLeftWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyBottom:
            return getPositionOffsetValue(style.get(), CSSPropertyBottom);
        case CSSPropertyWebkitBoxAlign:
            return CSSPrimitiveValue::create(style->boxAlign());
        case CSSPropertyWebkitBoxDirection:
            return CSSPrimitiveValue::create(style->boxDirection());
        case CSSPropertyWebkitBoxFlex:
            return CSSPrimitiveValue::create(style->boxFlex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxFlexGroup:
            return CSSPrimitiveValue::create(style->boxFlexGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxLines:
            return CSSPrimitiveValue::create(style->boxLines());
        case CSSPropertyWebkitBoxOrdinalGroup:
            return CSSPrimitiveValue::create(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxOrient:
            return CSSPrimitiveValue::create(style->boxOrient());
        case CSSPropertyWebkitBoxPack: {
            EBoxAlignment boxPack = style->boxPack();
            ASSERT(boxPack != BSTRETCH);
            ASSERT(boxPack != BBASELINE);
            if (boxPack == BJUSTIFY || boxPack== BBASELINE)
                return 0;
            return CSSPrimitiveValue::create(boxPack);
        }
        case CSSPropertyWebkitBoxReflect:
            return valueForReflection(style->boxReflect());
        case CSSPropertyWebkitBoxShadow:
            return valueForShadow(style->boxShadow());
        case CSSPropertyCaptionSide:
            return CSSPrimitiveValue::create(style->captionSide());
        case CSSPropertyClear:
            return CSSPrimitiveValue::create(style->clear());
        case CSSPropertyColor:
            return CSSPrimitiveValue::createColor(style->color().rgb());
        case CSSPropertyWebkitColumnCount:
            if (style->hasAutoColumnCount())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->columnCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitColumnGap:
            if (style->hasNormalColumnGap())
                return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
            return CSSPrimitiveValue::create(style->columnGap(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitColumnRuleColor:
            return currentColorOrValidColor(style.get(), style->columnRuleColor());
        case CSSPropertyWebkitColumnRuleStyle:
            return CSSPrimitiveValue::create(style->columnRuleStyle());
        case CSSPropertyWebkitColumnRuleWidth:
            return CSSPrimitiveValue::create(style->columnRuleWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyWebkitColumnBreakAfter:
            return CSSPrimitiveValue::create(style->columnBreakAfter());
        case CSSPropertyWebkitColumnBreakBefore:
            return CSSPrimitiveValue::create(style->columnBreakBefore());
        case CSSPropertyWebkitColumnBreakInside:
            return CSSPrimitiveValue::create(style->columnBreakInside());
        case CSSPropertyWebkitColumnWidth:
            if (style->hasAutoColumnWidth())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->columnWidth(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyCursor: {
            RefPtr<CSSValueList> list;
            CursorList* cursors = style->cursors();
            if (cursors && cursors->size() > 0) {
                list = CSSValueList::createCommaSeparated();
                for (unsigned i = 0; i < cursors->size(); ++i)
                    list->append(CSSPrimitiveValue::create((*cursors)[i].cursorImage->url(), CSSPrimitiveValue::CSS_URI));
            }
            RefPtr<CSSValue> value = CSSPrimitiveValue::create(style->cursor());
            if (list) {
                list->append(value);
                return list.release();
            }
            return value.release();
        }
        case CSSPropertyDirection:
            return CSSPrimitiveValue::create(style->direction());
        case CSSPropertyDisplay:
            return CSSPrimitiveValue::create(style->display());
        case CSSPropertyEmptyCells:
            return CSSPrimitiveValue::create(style->emptyCells());
        case CSSPropertyFloat:
            return CSSPrimitiveValue::create(style->floating());
        case CSSPropertyFontFamily:
            // FIXME: This only returns the first family.
            return CSSPrimitiveValue::create(style->fontDescription().family().family().string(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyFontSize:
            return CSSPrimitiveValue::create(style->fontDescription().computedPixelSize(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyWebkitBinding:
            break;
        case CSSPropertyFontStyle:
            if (style->fontDescription().italic())
                return CSSPrimitiveValue::createIdentifier(CSSValueItalic);
            return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
        case CSSPropertyFontVariant:
            if (style->fontDescription().smallCaps())
                return CSSPrimitiveValue::createIdentifier(CSSValueSmallCaps);
            return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
        case CSSPropertyFontWeight:
            switch (style->fontDescription().weight()) {
                case FontWeight100:
                    return CSSPrimitiveValue::createIdentifier(CSSValue100);
                case FontWeight200:
                    return CSSPrimitiveValue::createIdentifier(CSSValue200);
                case FontWeight300:
                    return CSSPrimitiveValue::createIdentifier(CSSValue300);
                case FontWeightNormal:
                    return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
                case FontWeight500:
                    return CSSPrimitiveValue::createIdentifier(CSSValue500);
                case FontWeight600:
                    return CSSPrimitiveValue::createIdentifier(CSSValue600);
                case FontWeightBold:
                    return CSSPrimitiveValue::createIdentifier(CSSValueBold);
                case FontWeight800:
                    return CSSPrimitiveValue::createIdentifier(CSSValue800);
                case FontWeight900:
                    return CSSPrimitiveValue::createIdentifier(CSSValue900);
            }
            ASSERT_NOT_REACHED();
            return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
        case CSSPropertyHeight:
            if (renderer)
                return CSSPrimitiveValue::create(sizingBox(renderer).height(), CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(style->height());
        case CSSPropertyWebkitHighlight:
            if (style->highlight() == nullAtom)
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(style->highlight(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitBorderFit:
            if (style->borderFit() == BorderFitBorder)
                return CSSPrimitiveValue::createIdentifier(CSSValueBorder);
            return CSSPrimitiveValue::createIdentifier(CSSValueLines);
        case CSSPropertyLeft:
            return getPositionOffsetValue(style.get(), CSSPropertyLeft);
        case CSSPropertyLetterSpacing:
            if (!style->letterSpacing())
                return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
            return CSSPrimitiveValue::create(style->letterSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyWebkitLineClamp:
            if (style->lineClamp() == -1)
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(style->lineClamp(), CSSPrimitiveValue::CSS_PERCENTAGE);
        case CSSPropertyLineHeight: {
            Length length = style->lineHeight();
            if (length.isNegative())
                return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
            if (length.isPercent())
                // This is imperfect, because it doesn't include the zoom factor and the real computation
                // for how high to be in pixels does include things like minimum font size and the zoom factor.
                // On the other hand, since font-size doesn't include the zoom factor, we really can't do
                // that here either.
                return CSSPrimitiveValue::create(static_cast<int>(length.percent() * style->fontDescription().specifiedSize()) / 100, CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(length.value(), CSSPrimitiveValue::CSS_PX);
        }
        case CSSPropertyListStyleImage:
            if (style->listStyleImage())
                return style->listStyleImage()->cssValue();
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyListStylePosition:
            return CSSPrimitiveValue::create(style->listStylePosition());
        case CSSPropertyListStyleType:
            return CSSPrimitiveValue::create(style->listStyleType());
        case CSSPropertyMarginTop:
            if (renderer && renderer->isBox())
                // FIXME: Supposed to return the percentage if percentage was specified.
                return CSSPrimitiveValue::create(toRenderBox(renderer)->marginTop(), CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(style->marginTop());
        case CSSPropertyMarginRight:
            if (renderer && renderer->isBox())
                // FIXME: Supposed to return the percentage if percentage was specified.
                return CSSPrimitiveValue::create(toRenderBox(renderer)->marginRight(), CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(style->marginRight());
        case CSSPropertyMarginBottom:
            if (renderer && renderer->isBox())
                // FIXME: Supposed to return the percentage if percentage was specified.
                return CSSPrimitiveValue::create(toRenderBox(renderer)->marginBottom(), CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(style->marginBottom());
        case CSSPropertyMarginLeft:
            if (renderer && renderer->isBox())
                // FIXME: Supposed to return the percentage if percentage was specified.
                return CSSPrimitiveValue::create(toRenderBox(renderer)->marginLeft(), CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(style->marginLeft());
        case CSSPropertyWebkitMarqueeDirection:
            return CSSPrimitiveValue::create(style->marqueeDirection());
        case CSSPropertyWebkitMarqueeIncrement:
            return CSSPrimitiveValue::create(style->marqueeIncrement());
        case CSSPropertyWebkitMarqueeRepetition:
            if (style->marqueeLoopCount() < 0)
                return CSSPrimitiveValue::createIdentifier(CSSValueInfinite);
            return CSSPrimitiveValue::create(style->marqueeLoopCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitMarqueeStyle:
            return CSSPrimitiveValue::create(style->marqueeBehavior());
        case CSSPropertyWebkitMaskImage:
            if (style->maskImage())
                return style->maskImage()->cssValue();
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyWebkitMaskSize: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            list->append(CSSPrimitiveValue::create(style->maskSize().width()));
            list->append(CSSPrimitiveValue::create(style->maskSize().height()));
            return list.release();
        }  
        case CSSPropertyWebkitMaskRepeat:
            return CSSPrimitiveValue::create(style->maskRepeat());
        case CSSPropertyWebkitMaskAttachment:
            if (style->maskAttachment())
                return CSSPrimitiveValue::createIdentifier(CSSValueScroll);
            return CSSPrimitiveValue::createIdentifier(CSSValueFixed);
        case CSSPropertyWebkitMaskComposite:
            return CSSPrimitiveValue::create(style->maskComposite());
        case CSSPropertyWebkitMaskClip:
        case CSSPropertyWebkitMaskOrigin: {
            EFillBox box = (propertyID == CSSPropertyWebkitMaskClip ? style->maskClip() : style->maskOrigin());
            return CSSPrimitiveValue::create(box);
        }
        case CSSPropertyWebkitMaskPosition: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

            list->append(CSSPrimitiveValue::create(style->maskXPosition()));
            list->append(CSSPrimitiveValue::create(style->maskYPosition()));

            return list.release();
        }
        case CSSPropertyWebkitMaskPositionX:
            return CSSPrimitiveValue::create(style->maskXPosition());
        case CSSPropertyWebkitMaskPositionY:
            return CSSPrimitiveValue::create(style->maskYPosition());
        case CSSPropertyWebkitUserModify:
            return CSSPrimitiveValue::create(style->userModify());
        case CSSPropertyMaxHeight: {
            const Length& maxHeight = style->maxHeight();
            if (maxHeight.isFixed() && maxHeight.value() == undefinedLength)
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(maxHeight);
        }
        case CSSPropertyMaxWidth: {
            const Length& maxWidth = style->maxWidth();
            if (maxWidth.isFixed() && maxWidth.value() == undefinedLength)
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(maxWidth);
        }
        case CSSPropertyMinHeight:
            return CSSPrimitiveValue::create(style->minHeight());
        case CSSPropertyMinWidth:
            return CSSPrimitiveValue::create(style->minWidth());
        case CSSPropertyOpacity:
            return CSSPrimitiveValue::create(style->opacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOrphans:
            return CSSPrimitiveValue::create(style->orphans(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOutlineColor:
            return currentColorOrValidColor(style.get(), style->outlineColor());
        case CSSPropertyOutlineStyle:
            if (style->outlineStyleIsAuto())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->outlineStyle());
        case CSSPropertyOutlineWidth:
            return CSSPrimitiveValue::create(style->outlineWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyOverflow:
            return CSSPrimitiveValue::create(max(style->overflowX(), style->overflowY()));
        case CSSPropertyOverflowX:
            return CSSPrimitiveValue::create(style->overflowX());
        case CSSPropertyOverflowY:
            return CSSPrimitiveValue::create(style->overflowY());
        case CSSPropertyPaddingTop:
            if (renderer && renderer->isBox())
                return CSSPrimitiveValue::create(toRenderBox(renderer)->paddingTop(false), CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(style->paddingTop());
        case CSSPropertyPaddingRight:
            if (renderer && renderer->isBox())
                return CSSPrimitiveValue::create(toRenderBox(renderer)->paddingRight(false), CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(style->paddingRight());
        case CSSPropertyPaddingBottom:
            if (renderer && renderer->isBox())
                return CSSPrimitiveValue::create(toRenderBox(renderer)->paddingBottom(false), CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(style->paddingBottom());
        case CSSPropertyPaddingLeft:
            if (renderer && renderer->isBox())
                return CSSPrimitiveValue::create(toRenderBox(renderer)->paddingLeft(false), CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(style->paddingLeft());
        case CSSPropertyPageBreakAfter:
            return CSSPrimitiveValue::create(style->pageBreakAfter());
        case CSSPropertyPageBreakBefore:
            return CSSPrimitiveValue::create(style->pageBreakBefore());
        case CSSPropertyPageBreakInside: {
            EPageBreak pageBreak = style->pageBreakInside();
            ASSERT(pageBreak != PBALWAYS);
            if (pageBreak == PBALWAYS)
                return 0;
            return CSSPrimitiveValue::create(style->pageBreakInside());
        }
        case CSSPropertyPosition:
            return CSSPrimitiveValue::create(style->position());
        case CSSPropertyRight:
            return getPositionOffsetValue(style.get(), CSSPropertyRight);
        case CSSPropertyTableLayout:
            return CSSPrimitiveValue::create(style->tableLayout());
        case CSSPropertyTextAlign:
            return CSSPrimitiveValue::create(style->textAlign());
        case CSSPropertyTextDecoration: {
            String string;
            if (style->textDecoration() & UNDERLINE)
                string += "underline";
            if (style->textDecoration() & OVERLINE) {
                if (string.length())
                    string += " ";
                string += "overline";
            }
            if (style->textDecoration() & LINE_THROUGH) {
                if (string.length())
                    string += " ";
                string += "line-through";
            }
            if (style->textDecoration() & BLINK) {
                if (string.length())
                    string += " ";
                string += "blink";
            }
            if (!string.length())
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(string, CSSPrimitiveValue::CSS_STRING);
        }
        case CSSPropertyWebkitTextDecorationsInEffect: {
            String string;
            if (style->textDecorationsInEffect() & UNDERLINE)
                string += "underline";
            if (style->textDecorationsInEffect() & OVERLINE) {
                if (string.length())
                    string += " ";
                string += "overline";
            }
            if (style->textDecorationsInEffect() & LINE_THROUGH) {
                if (string.length())
                    string += " ";
                string += "line-through";
            }
            if (style->textDecorationsInEffect() & BLINK) {
                if (string.length())
                    string += " ";
                string += "blink";
            }
            if (!string.length())
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(string, CSSPrimitiveValue::CSS_STRING);
        }
        case CSSPropertyWebkitTextFillColor:
            return currentColorOrValidColor(style.get(), style->textFillColor());
        case CSSPropertyTextIndent:
            return CSSPrimitiveValue::create(style->textIndent());
        case CSSPropertyTextShadow:
            return valueForShadow(style->textShadow());
        case CSSPropertyWebkitTextSecurity:
            return CSSPrimitiveValue::create(style->textSecurity());
        case CSSPropertyWebkitTextSizeAdjust:
            if (style->textSizeAdjust().isAuto()) 
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            else if (style->textSizeAdjust().isNone())
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            else
                return CSSPrimitiveValue::create(style->textSizeAdjust().percentage(), CSSPrimitiveValue::CSS_PERCENTAGE);
        case CSSPropertyWebkitTextStrokeColor:
            return currentColorOrValidColor(style.get(), style->textStrokeColor());
        case CSSPropertyWebkitTextStrokeWidth:
            return CSSPrimitiveValue::create(style->textStrokeWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyTextTransform:
            return CSSPrimitiveValue::create(style->textTransform());
        case CSSPropertyTop:
            return getPositionOffsetValue(style.get(), CSSPropertyTop);
        case CSSPropertyUnicodeBidi:
            return CSSPrimitiveValue::create(style->unicodeBidi());
        case CSSPropertyVerticalAlign:
            switch (style->verticalAlign()) {
                case BASELINE:
                    return CSSPrimitiveValue::createIdentifier(CSSValueBaseline);
                case MIDDLE:
                    return CSSPrimitiveValue::createIdentifier(CSSValueMiddle);
                case SUB:
                    return CSSPrimitiveValue::createIdentifier(CSSValueSub);
                case SUPER:
                    return CSSPrimitiveValue::createIdentifier(CSSValueSuper);
                case TEXT_TOP:
                    return CSSPrimitiveValue::createIdentifier(CSSValueTextTop);
                case TEXT_BOTTOM:
                    return CSSPrimitiveValue::createIdentifier(CSSValueTextBottom);
                case TOP:
                    return CSSPrimitiveValue::createIdentifier(CSSValueTop);
                case BOTTOM:
                    return CSSPrimitiveValue::createIdentifier(CSSValueBottom);
                case BASELINE_MIDDLE:
                    return CSSPrimitiveValue::createIdentifier(CSSValueWebkitBaselineMiddle);
                case LENGTH:
                    return CSSPrimitiveValue::create(style->verticalAlignLength());
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSSPropertyVisibility:
            return CSSPrimitiveValue::create(style->visibility());
        case CSSPropertyWhiteSpace:
            return CSSPrimitiveValue::create(style->whiteSpace());
        case CSSPropertyWidows:
            return CSSPrimitiveValue::create(style->widows(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWidth:
            if (renderer)
                return CSSPrimitiveValue::create(sizingBox(renderer).width(), CSSPrimitiveValue::CSS_PX);
            return CSSPrimitiveValue::create(style->width());
        case CSSPropertyWordBreak:
            return CSSPrimitiveValue::create(style->wordBreak());
        case CSSPropertyWordSpacing:
            return CSSPrimitiveValue::create(style->wordSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyWordWrap:
            return CSSPrimitiveValue::create(style->wordWrap());
        case CSSPropertyWebkitLineBreak:
            return CSSPrimitiveValue::create(style->khtmlLineBreak());
        case CSSPropertyWebkitNbspMode:
            return CSSPrimitiveValue::create(style->nbspMode());
        case CSSPropertyWebkitMatchNearestMailBlockquoteColor:
            return CSSPrimitiveValue::create(style->matchNearestMailBlockquoteColor());
        case CSSPropertyResize:
            return CSSPrimitiveValue::create(style->resize());
        case CSSPropertyZIndex:
            if (style->hasAutoZIndex())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->zIndex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyZoom:
            return CSSPrimitiveValue::create(style->zoom(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxSizing:
            if (style->boxSizing() == CONTENT_BOX)
                return CSSPrimitiveValue::createIdentifier(CSSValueContentBox);
            return CSSPrimitiveValue::createIdentifier(CSSValueBorderBox);
#if ENABLE(DASHBOARD_SUPPORT)
        case CSSPropertyWebkitDashboardRegion:
        {
            const Vector<StyleDashboardRegion>& regions = style->dashboardRegions();
            unsigned count = regions.size();
            if (count == 1 && regions[0].type == StyleDashboardRegion::None)
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);

            RefPtr<DashboardRegion> firstRegion;
            DashboardRegion* previousRegion = 0;
            for (unsigned i = 0; i < count; i++) {
                RefPtr<DashboardRegion> region = DashboardRegion::create();
                StyleDashboardRegion styleRegion = regions[i];

                region->m_label = styleRegion.label;
                LengthBox offset = styleRegion.offset;
                region->setTop(CSSPrimitiveValue::create(offset.top().value(), CSSPrimitiveValue::CSS_PX));
                region->setRight(CSSPrimitiveValue::create(offset.right().value(), CSSPrimitiveValue::CSS_PX));
                region->setBottom(CSSPrimitiveValue::create(offset.bottom().value(), CSSPrimitiveValue::CSS_PX));
                region->setLeft(CSSPrimitiveValue::create(offset.left().value(), CSSPrimitiveValue::CSS_PX));
                region->m_isRectangle = (styleRegion.type == StyleDashboardRegion::Rectangle);
                region->m_isCircle = (styleRegion.type == StyleDashboardRegion::Circle);

                if (previousRegion)
                    previousRegion->m_next = region;
                else
                    firstRegion = region;
                previousRegion = region.get();
            }
            return CSSPrimitiveValue::create(firstRegion.release());
        }
#endif
        case CSSPropertyWebkitAnimationDelay:
            return getDelayValue(style->animations());
        case CSSPropertyWebkitAnimationDirection: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    if (t->animation(i)->direction())
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueAlternate));
                    else
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueNormal));
                }
            } else
                list->append(CSSPrimitiveValue::createIdentifier(CSSValueNormal));
            return list.release();
        }
        case CSSPropertyWebkitAnimationDuration:
            return getDurationValue(style->animations());
        case CSSPropertyWebkitAnimationIterationCount: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int iterationCount = t->animation(i)->iterationCount();
                    if (iterationCount == Animation::IterationCountInfinite)
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueInfinite));
                    else
                        list->append(CSSPrimitiveValue::create(iterationCount, CSSPrimitiveValue::CSS_NUMBER));
                }
            } else
                list->append(CSSPrimitiveValue::create(Animation::initialAnimationIterationCount(), CSSPrimitiveValue::CSS_NUMBER));
            return list.release();
        }
        case CSSPropertyWebkitAnimationName: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    list->append(CSSPrimitiveValue::create(t->animation(i)->name(), CSSPrimitiveValue::CSS_STRING));
                }
            } else
                list->append(CSSPrimitiveValue::createIdentifier(CSSValueNone));
            return list.release();
        }
        case CSSPropertyWebkitAnimationPlayState: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int prop = t->animation(i)->playState();
                    if (prop == AnimPlayStatePlaying)
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueRunning));
                    else
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValuePaused));
                }
            } else
                list->append(CSSPrimitiveValue::createIdentifier(CSSValueRunning));
            return list.release();
        }
        case CSSPropertyWebkitAnimationTimingFunction:
            return getTimingFunctionValue(style->animations());
        case CSSPropertyWebkitAppearance:
            return CSSPrimitiveValue::create(style->appearance());
        case CSSPropertyWebkitBackfaceVisibility:
            return CSSPrimitiveValue::createIdentifier((style->backfaceVisibility() == BackfaceVisibilityHidden) ? CSSValueHidden : CSSValueVisible);
        case CSSPropertyWebkitBorderImage:
            return valueForNinePieceImage(style->borderImage());
        case CSSPropertyWebkitMaskBoxImage:
            return valueForNinePieceImage(style->maskBoxImage());
        case CSSPropertyWebkitFontSizeDelta:
            // Not a real style property -- used by the editing engine -- so has no computed value.
            break;
        case CSSPropertyWebkitMarginBottomCollapse:
            return CSSPrimitiveValue::create(style->marginBottomCollapse());
        case CSSPropertyWebkitMarginTopCollapse:
            return CSSPrimitiveValue::create(style->marginTopCollapse());
        case CSSPropertyWebkitPerspective:
            if (!style->hasPerspective())
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(style->perspective(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitPerspectiveOrigin: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                IntRect box = sizingBox(renderer);
                list->append(CSSPrimitiveValue::create(style->perspectiveOriginX().calcMinValue(box.width()), CSSPrimitiveValue::CSS_PX));
                list->append(CSSPrimitiveValue::create(style->perspectiveOriginY().calcMinValue(box.height()), CSSPrimitiveValue::CSS_PX));
            }
            else {
                list->append(CSSPrimitiveValue::create(style->perspectiveOriginX()));
                list->append(CSSPrimitiveValue::create(style->perspectiveOriginY()));
            }
            return list.release();
        }
        case CSSPropertyWebkitRtlOrdering:
            if (style->visuallyOrdered())
                return CSSPrimitiveValue::createIdentifier(CSSValueVisual);
            return CSSPrimitiveValue::createIdentifier(CSSValueLogical);
        case CSSPropertyWebkitTapHighlightColor:
            return currentColorOrValidColor(style.get(), style->tapHighlightColor());
        case CSSPropertyWebkitTouchCallout: {
            return style->touchCalloutEnabled() ? CSSPrimitiveValue::createIdentifier(CSSValueDefault) : CSSPrimitiveValue::createIdentifier(CSSValueNone);
        }
        case CSSPropertyWebkitCompositionFillColor:
            return currentColorOrValidColor(style.get(), style->compositionFillColor());
        case CSSPropertyWebkitCompositionFrameColor:
            return currentColorOrValidColor(style.get(), style->compositionFrameColor());
        case CSSPropertyWebkitUserDrag:
            return CSSPrimitiveValue::create(style->userDrag());
        case CSSPropertyWebkitUserSelect:
            return CSSPrimitiveValue::create(style->userSelect());
        case CSSPropertyWebkitBorderBottomLeftRadius:
            return getBorderRadiusCornerValue(style->borderBottomLeftRadius());
        case CSSPropertyWebkitBorderBottomRightRadius:
            return getBorderRadiusCornerValue(style->borderBottomRightRadius());
        case CSSPropertyWebkitBorderTopLeftRadius:
            return getBorderRadiusCornerValue(style->borderTopLeftRadius());
        case CSSPropertyWebkitBorderTopRightRadius:
            return getBorderRadiusCornerValue(style->borderTopRightRadius());
        case CSSPropertyClip:
        {
            if (style->hasClip()) {
                RefPtr<Rect> rect = Rect::create();
                rect->setTop(CSSPrimitiveValue::create(style->clip().top().value(), CSSPrimitiveValue::CSS_PX));
                rect->setRight(CSSPrimitiveValue::create(style->clip().right().value(), CSSPrimitiveValue::CSS_PX));
                rect->setBottom(CSSPrimitiveValue::create(style->clip().bottom().value(), CSSPrimitiveValue::CSS_PX));
                rect->setLeft(CSSPrimitiveValue::create(style->clip().left().value(), CSSPrimitiveValue::CSS_PX));
                return CSSPrimitiveValue::create(rect.release());
            }
            return 0;
        }
        case CSSPropertyWebkitImageLoadingBorder:
            return CSSPrimitiveValue::create(style->imageLoadingBorder());
            break;
        case CSSPropertyWebkitTransform:
            return computedTransform(renderer, style.get());
        case CSSPropertyWebkitTransformOrigin: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                IntRect box = sizingBox(renderer);
                list->append(CSSPrimitiveValue::create(style->transformOriginX().calcMinValue(box.width()), CSSPrimitiveValue::CSS_PX));
                list->append(CSSPrimitiveValue::create(style->transformOriginY().calcMinValue(box.height()), CSSPrimitiveValue::CSS_PX));
                if (style->transformOriginZ() != 0)
                    list->append(CSSPrimitiveValue::create(style->transformOriginZ(), CSSPrimitiveValue::CSS_PX));
            } else {
                list->append(CSSPrimitiveValue::create(style->transformOriginX()));
                list->append(CSSPrimitiveValue::create(style->transformOriginY()));
                if (style->transformOriginZ() != 0)
                    list->append(CSSPrimitiveValue::create(style->transformOriginZ(), CSSPrimitiveValue::CSS_PX));
            }
            return list.release();
        }
        case CSSPropertyWebkitTransformStyle:
            return CSSPrimitiveValue::createIdentifier((style->transformStyle3D() == TransformStyle3DPreserve3D) ? CSSValuePreserve3d : CSSValueFlat);
        case CSSPropertyWebkitTransitionDelay:
            return getDelayValue(style->transitions());
        case CSSPropertyWebkitTransitionDuration:
            return getDurationValue(style->transitions());
        case CSSPropertyWebkitTransitionProperty: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->transitions();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int prop = t->animation(i)->property();
                    RefPtr<CSSValue> propertyValue;
                    if (prop == cAnimateNone)
                        propertyValue = CSSPrimitiveValue::createIdentifier(CSSValueNone);
                    else if (prop == cAnimateAll)
                        propertyValue = CSSPrimitiveValue::createIdentifier(CSSValueAll);
                    else
                        propertyValue = CSSPrimitiveValue::create(getPropertyName(static_cast<CSSPropertyID>(prop)), CSSPrimitiveValue::CSS_STRING);
                    list->append(propertyValue);
                }
            } else
                list->append(CSSPrimitiveValue::createIdentifier(CSSValueAll));
            return list.release();
        }
        case CSSPropertyWebkitTransitionTimingFunction:
            return getTimingFunctionValue(style->transitions());
        case CSSPropertyPointerEvents:
            return CSSPrimitiveValue::create(style->pointerEvents());
        case CSSPropertyBackground:
        case CSSPropertyBorder:
        case CSSPropertyBorderBottom:
        case CSSPropertyBorderColor:
        case CSSPropertyBorderLeft:
        case CSSPropertyBorderRight:
        case CSSPropertyBorderStyle:
        case CSSPropertyBorderTop:
        case CSSPropertyBorderWidth:
        case CSSPropertyContent:
        case CSSPropertyCounterIncrement:
        case CSSPropertyCounterReset:
        case CSSPropertyFont:
        case CSSPropertyFontStretch:
        case CSSPropertyListStyle:
        case CSSPropertyMargin:
        case CSSPropertyOutline:
        case CSSPropertyOutlineOffset:
        case CSSPropertyPadding:
        case CSSPropertyPage:
        case CSSPropertyQuotes:
        case CSSPropertyScrollbar3dlightColor:
        case CSSPropertyScrollbarArrowColor:
        case CSSPropertyScrollbarDarkshadowColor:
        case CSSPropertyScrollbarFaceColor:
        case CSSPropertyScrollbarHighlightColor:
        case CSSPropertyScrollbarShadowColor:
        case CSSPropertyScrollbarTrackColor:
        case CSSPropertySrc: // Only used in @font-face rules.
        case CSSPropertySize:
        case CSSPropertyTextLineThrough:
        case CSSPropertyTextLineThroughColor:
        case CSSPropertyTextLineThroughMode:
        case CSSPropertyTextLineThroughStyle:
        case CSSPropertyTextLineThroughWidth:
        case CSSPropertyTextOverflow:
        case CSSPropertyTextOverline:
        case CSSPropertyTextOverlineColor:
        case CSSPropertyTextOverlineMode:
        case CSSPropertyTextOverlineStyle:
        case CSSPropertyTextOverlineWidth:
        case CSSPropertyTextUnderline:
        case CSSPropertyTextUnderlineColor:
        case CSSPropertyTextUnderlineMode:
        case CSSPropertyTextUnderlineStyle:
        case CSSPropertyTextUnderlineWidth:
        case CSSPropertyUnicodeRange: // Only used in @font-face rules.
        case CSSPropertyWebkitAnimation:
        case CSSPropertyWebkitBorderRadius:
        case CSSPropertyWebkitColumns:
        case CSSPropertyWebkitColumnRule:
        case CSSPropertyWebkitMarginCollapse:
        case CSSPropertyWebkitMarginStart:
        case CSSPropertyWebkitMarquee:
        case CSSPropertyWebkitMarqueeSpeed:
        case CSSPropertyWebkitMask:
        case CSSPropertyWebkitPaddingStart:
        case CSSPropertyWebkitPerspectiveOriginX:
        case CSSPropertyWebkitPerspectiveOriginY:
        case CSSPropertyWebkitTextStroke:
        case CSSPropertyWebkitTransformOriginX:
        case CSSPropertyWebkitTransformOriginY:
        case CSSPropertyWebkitTransformOriginZ:
        case CSSPropertyWebkitTransition:
        case CSSPropertyWebkitVariableDeclarationBlock:
            // FIXME: The above are unimplemented.
            break;
#if ENABLE(SVG)
        // FIXME: This default case ruins the point of using an enum for
        // properties -- it prevents us from getting a warning when we
        // forget to list a property above.
        default:
            return getSVGPropertyCSSValue(propertyID, DoNotUpdateLayout);
#endif
    }

    LOG_ERROR("unimplemented propertyID: %d", propertyID);
    return 0;
}

String CSSComputedStyleDeclaration::getPropertyValue(int propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();
    return "";
}

bool CSSComputedStyleDeclaration::getPropertyPriority(int /*propertyID*/) const
{
    // All computed styles have a priority of false (not "important").
    return false;
}

String CSSComputedStyleDeclaration::removeProperty(int /*propertyID*/, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
    return String();
}

void CSSComputedStyleDeclaration::setProperty(int /*propertyID*/, const String& /*value*/, bool /*important*/, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

unsigned CSSComputedStyleDeclaration::length() const
{
    Node* node = m_node.get();
    if (!node)
        return 0;

    RenderStyle* style = node->computedStyle();
    if (!style)
        return 0;

    return numComputedProperties;
}

String CSSComputedStyleDeclaration::item(unsigned i) const
{
    if (i >= length())
        return String();

    return getPropertyName(static_cast<CSSPropertyID>(computedProperties[i]));
}

// This is the list of properties we want to copy in the copyInheritableProperties() function.
// It is the intersection of the list of inherited CSS properties and the
// properties for which we have a computed implementation in this file.
const int inheritableProperties[] = {
    CSSPropertyBorderCollapse,
    CSSPropertyColor,
    CSSPropertyFontFamily,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
    CSSPropertyFontVariant,
    CSSPropertyFontWeight,
    CSSPropertyLetterSpacing,
    CSSPropertyLineHeight,
    CSSPropertyOrphans,
    CSSPropertyTextAlign,
    CSSPropertyTextIndent,
    CSSPropertyTextTransform,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWordSpacing,
    CSSPropertyWebkitBorderHorizontalSpacing,
    CSSPropertyWebkitBorderVerticalSpacing,
    CSSPropertyWebkitTapHighlightColor,
    CSSPropertyWebkitCompositionFillColor,
    CSSPropertyWebkitCompositionFrameColor,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextSizeAdjust,
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,
};

const unsigned numInheritableProperties = sizeof(inheritableProperties) / sizeof(inheritableProperties[0]);

void CSSComputedStyleDeclaration::removeComputedInheritablePropertiesFrom(CSSMutableStyleDeclaration* declaration)
{
    declaration->removePropertiesInSet(inheritableProperties, numInheritableProperties);
}

PassRefPtr<CSSMutableStyleDeclaration> CSSComputedStyleDeclaration::copyInheritableProperties() const
{
    RefPtr<CSSMutableStyleDeclaration> style = copyPropertiesInSet(inheritableProperties, numInheritableProperties);
    if (style && m_node && m_node->computedStyle()) {
        // If a node's text fill color is invalid, then its children use 
        // their font-color as their text fill color (they don't
        // inherit it).  Likewise for stroke color.
        ExceptionCode ec = 0;
        if (!m_node->computedStyle()->textFillColor().isValid())
            style->removeProperty(CSSPropertyWebkitTextFillColor, ec);
        if (!m_node->computedStyle()->textStrokeColor().isValid())
            style->removeProperty(CSSPropertyWebkitTextStrokeColor, ec);
        ASSERT(ec == 0);
    }
    return style.release();
}

PassRefPtr<CSSMutableStyleDeclaration> CSSComputedStyleDeclaration::copy() const
{
    return copyPropertiesInSet(computedProperties, numComputedProperties);
}

PassRefPtr<CSSMutableStyleDeclaration> CSSComputedStyleDeclaration::makeMutable()
{
    return copy();
}

} // namespace WebCore
