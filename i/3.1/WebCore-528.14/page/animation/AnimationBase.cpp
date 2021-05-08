/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AnimationBase.h"

#include "AnimationControllerPrivate.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPropertyLonghand.h"
#include "CSSPropertyNames.h"
#include "CString.h"
#include "CompositeAnimation.h"
#include "Document.h"
#include "EventNames.h"
#include "FloatConversion.h"
#include "Frame.h"
#include "IdentityTransformOperation.h"
#include "ImplicitAnimation.h"
#include "KeyframeAnimation.h"
#include "MatrixTransformOperation.h"
#include "Matrix3DTransformOperation.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "UnitBezier.h"

#include <algorithm>

using namespace std;

namespace WebCore {

// The epsilon value we pass to UnitBezier::solve given that the animation is going to run over |dur| seconds. The longer the
// animation, the more precision we need in the timing function result to avoid ugly discontinuities.
static inline double solveEpsilon(double duration)
{
    return 1.0 / (200.0 * duration);
}

static inline double solveCubicBezierFunction(double p1x, double p1y, double p2x, double p2y, double t, double duration)
{
    // Convert from input time to parametric value in curve, then from
    // that to output time.
    UnitBezier bezier(p1x, p1y, p2x, p2y);
    return bezier.solve(t, solveEpsilon(duration));
}

static inline int blendFunc(const AnimationBase*, int from, int to, double progress)
{  
    return int(from + (to - from) * progress);
}

static inline double blendFunc(const AnimationBase*, double from, double to, double progress)
{  
    return from + (to - from) * progress;
}

static inline float blendFunc(const AnimationBase*, float from, float to, double progress)
{  
    return narrowPrecisionToFloat(from + (to - from) * progress);
}

static inline Color blendFunc(const AnimationBase* anim, const Color& from, const Color& to, double progress)
{
    // We need to preserve the state of the valid flag at the end of the animation
    if (progress == 1 && !to.isValid())
        return Color();

    return Color(blendFunc(anim, from.red(), to.red(), progress),
                 blendFunc(anim, from.green(), to.green(), progress),
                 blendFunc(anim, from.blue(), to.blue(), progress),
                 blendFunc(anim, from.alpha(), to.alpha(), progress));
}

static inline Length blendFunc(const AnimationBase*, const Length& from, const Length& to, double progress)
{  
    return to.blend(from, progress);
}

static inline IntSize blendFunc(const AnimationBase* anim, const IntSize& from, const IntSize& to, double progress)
{  
    return IntSize(blendFunc(anim, from.width(), to.width(), progress),
                   blendFunc(anim, from.height(), to.height(), progress));
}

static inline ShadowData* blendFunc(const AnimationBase* anim, const ShadowData* from, const ShadowData* to, double progress)
{  
    ASSERT(from && to);
    return new ShadowData(blendFunc(anim, from->x, to->x, progress), blendFunc(anim, from->y, to->y, progress), 
                          blendFunc(anim, from->blur, to->blur, progress), blendFunc(anim, from->color, to->color, progress));
}

static inline TransformOperations blendFunc(const AnimationBase* anim, const TransformOperations& from, const TransformOperations& to, double progress)
{    
    TransformOperations result;

    // If we have a transform function list, use that to do a per-function animation. Otherwise do a Matrix animation
    if (anim->isTransformFunctionListValid()) {
        unsigned fromSize = from.operations().size();
        unsigned toSize = to.operations().size();
        unsigned size = max(fromSize, toSize);
        for (unsigned i = 0; i < size; i++) {
            RefPtr<TransformOperation> fromOp = (i < fromSize) ? from.operations()[i].get() : 0;
            RefPtr<TransformOperation> toOp = (i < toSize) ? to.operations()[i].get() : 0;
            RefPtr<TransformOperation> blendedOp = toOp ? toOp->blend(fromOp.get(), progress) : (fromOp ? fromOp->blend(0, progress, true) : 0);
            if (blendedOp)
                result.operations().append(blendedOp);
            else {
                RefPtr<TransformOperation> identityOp = IdentityTransformOperation::create();
                if (progress > 0.5)
                    result.operations().append(toOp ? toOp : identityOp);
                else
                    result.operations().append(fromOp ? fromOp : identityOp);
            }
        }
    } else {
        // Convert the TransformOperations into matrices
        IntSize size = anim->renderer()->isBox() ? toRenderBox(anim->renderer())->borderBoxRect().size() : IntSize();
        TransformationMatrix fromT;
        TransformationMatrix toT;
        from.apply(size, fromT);
        to.apply(size, toT);
        
        toT.blend(fromT, progress);
        
        // Append the result
        result.operations().append(Matrix3DTransformOperation::create(toT));
    }
    return result;
}

static inline EVisibility blendFunc(const AnimationBase* anim, EVisibility from, EVisibility to, double progress)
{
    // Any non-zero result means we consider the object to be visible.  Only at 0 do we consider the object to be
    // invisible.   The invisible value we use (HIDDEN vs. COLLAPSE) depends on the specified from/to values.
    double fromVal = from == VISIBLE ? 1. : 0.;
    double toVal = to == VISIBLE ? 1. : 0.;
    if (fromVal == toVal)
        return to;
    double result = blendFunc(anim, fromVal, toVal, progress);
    return result > 0. ? VISIBLE : (to != VISIBLE ? to : from);
}

class PropertyWrapperBase;

static void addShorthandProperties();
static PropertyWrapperBase* wrapperForProperty(int propertyID);

class PropertyWrapperBase {
public:
    PropertyWrapperBase(int prop)
        : m_prop(prop)
    {
    }

    virtual ~PropertyWrapperBase() { }
    
    virtual bool isShorthandWrapper() const { return false; }
    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const = 0;
    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const = 0;

    int property() const { return m_prop; }

#if USE(ACCELERATED_COMPOSITING)
    virtual bool animationIsAccelerated() const { return false; }
#endif

private:
    int m_prop;
};

template <typename T>
class PropertyWrapperGetter : public PropertyWrapperBase {
public:
    PropertyWrapperGetter(int prop, T (RenderStyle::*getter)() const)
        : PropertyWrapperBase(prop)
        , m_getter(getter)
    {
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const
    {
       // If the style pointers are the same, don't bother doing the test.
       // If either is null, return false. If both are null, return true.
       if (!a && !b || a == b)
           return true;
       if (!a || !b)
            return false;
        return (a->*m_getter)() == (b->*m_getter)();
    }

protected:
    T (RenderStyle::*m_getter)() const;
};

template <typename T>
class PropertyWrapper : public PropertyWrapperGetter<T> {
public:
    PropertyWrapper(int prop, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
        : PropertyWrapperGetter<T>(prop, getter)
        , m_setter(setter)
    {
    }
    
    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const
    {
        (dst->*m_setter)(blendFunc(anim, (a->*PropertyWrapperGetter<T>::m_getter)(), (b->*PropertyWrapperGetter<T>::m_getter)(), progress));
    }

protected:
    void (RenderStyle::*m_setter)(T);
};

#if USE(ACCELERATED_COMPOSITING)
class PropertyWrapperAcceleratedOpacity : public PropertyWrapper<float> {
public:
    PropertyWrapperAcceleratedOpacity()
        : PropertyWrapper<float>(CSSPropertyOpacity, &RenderStyle::opacity, &RenderStyle::setOpacity)
    {
    }

    virtual bool animationIsAccelerated() const { return true; }

    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const
    {
        float fromOpacity = a->opacity();
        
        // This makes sure we put the object being animated into a RenderLayer during the animation
        dst->setOpacity(blendFunc(anim, (fromOpacity == 1) ? 0.999999f : fromOpacity, b->opacity(), progress));
    }
};

class PropertyWrapperAcceleratedTransform : public PropertyWrapper<const TransformOperations&> {
public:
    PropertyWrapperAcceleratedTransform()
        : PropertyWrapper<const TransformOperations&>(CSSPropertyWebkitTransform, &RenderStyle::transform, &RenderStyle::setTransform)
    {
    }
    
    virtual bool animationIsAccelerated() const { return true; }

    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const
    {
        dst->setTransform(blendFunc(anim, a->transform(), b->transform(), progress));
    }
};
#endif // USE(ACCELERATED_COMPOSITING)

class PropertyWrapperShadow : public PropertyWrapperGetter<ShadowData*> {
public:
    PropertyWrapperShadow(int prop, ShadowData* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(ShadowData*, bool))
        : PropertyWrapperGetter<ShadowData*>(prop, getter)
        , m_setter(setter)
    {
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const
    {
        ShadowData* shadowA = (a->*m_getter)();
        ShadowData* shadowB = (b->*m_getter)();

        if (!shadowA && shadowB || shadowA && !shadowB)
            return false;
        if (shadowA && shadowB && (*shadowA != *shadowB))
            return false;
        return true;
    }

    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const
    {
        ShadowData* shadowA = (a->*m_getter)();
        ShadowData* shadowB = (b->*m_getter)();
        ShadowData defaultShadowData(0, 0, 0, Color::transparent);

        if (!shadowA)
            shadowA = &defaultShadowData;
        if (!shadowB)
            shadowB = &defaultShadowData;

        (dst->*m_setter)(blendFunc(anim, shadowA, shadowB, progress), false);
    }

private:
    void (RenderStyle::*m_setter)(ShadowData*, bool);
};

class PropertyWrapperMaybeInvalidColor : public PropertyWrapperBase {
public:
    PropertyWrapperMaybeInvalidColor(int prop, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
        : PropertyWrapperBase(prop)
        , m_getter(getter)
        , m_setter(setter)
    {
    }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const
    {
        Color fromColor = (a->*m_getter)();
        Color toColor = (b->*m_getter)();
        if (!fromColor.isValid())
            fromColor = a->color();
        if (!toColor.isValid())
            toColor = b->color();

        return fromColor == toColor;
    }

    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const
    {
        Color fromColor = (a->*m_getter)();
        Color toColor = (b->*m_getter)();
        if (!fromColor.isValid())
            fromColor = a->color();
        if (!toColor.isValid())
            toColor = b->color();
        (dst->*m_setter)(blendFunc(anim, fromColor, toColor, progress));
    }

private:
    const Color& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Color&);
};

class ShorthandPropertyWrapper : public PropertyWrapperBase {
public:
    ShorthandPropertyWrapper(int property, const CSSPropertyLonghand& longhand)
        : PropertyWrapperBase(property)
    {
        for (unsigned i = 0; i < longhand.length(); ++i) {
            PropertyWrapperBase* wrapper = wrapperForProperty(longhand.properties()[i]);
            if (wrapper)
                m_propertyWrappers.append(wrapper);
        }
    }

    virtual bool isShorthandWrapper() const { return true; }

    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const
    {
        Vector<PropertyWrapperBase*>::const_iterator end = m_propertyWrappers.end();
        for (Vector<PropertyWrapperBase*>::const_iterator it = m_propertyWrappers.begin(); it != end; ++it) {
            if (!(*it)->equals(a, b))
                return false;
        }
        return true;
    }

    virtual void blend(const AnimationBase* anim, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress) const
    {
        Vector<PropertyWrapperBase*>::const_iterator end = m_propertyWrappers.end();
        for (Vector<PropertyWrapperBase*>::const_iterator it = m_propertyWrappers.begin(); it != end; ++it)
            (*it)->blend(anim, dst, a, b, progress);
    }

private:
    Vector<PropertyWrapperBase*> m_propertyWrappers;
};


static Vector<PropertyWrapperBase*>* gPropertyWrappers = 0;
static int gPropertyWrapperMap[numCSSProperties];

static const int cInvalidPropertyWrapperIndex = -1;


static void ensurePropertyMap()
{
    // FIXME: This data is never destroyed. Maybe we should ref count it and toss it when the last AnimationController is destroyed?
    if (gPropertyWrappers == 0) {
        gPropertyWrappers = new Vector<PropertyWrapperBase*>();

        // build the list of property wrappers to do the comparisons and blends
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyLeft, &RenderStyle::left, &RenderStyle::setLeft));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyRight, &RenderStyle::right, &RenderStyle::setRight));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyTop, &RenderStyle::top, &RenderStyle::setTop));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyBottom, &RenderStyle::bottom, &RenderStyle::setBottom));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWidth, &RenderStyle::width, &RenderStyle::setWidth));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyHeight, &RenderStyle::height, &RenderStyle::setHeight));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyBorderLeftWidth, &RenderStyle::borderLeftWidth, &RenderStyle::setBorderLeftWidth));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyBorderRightWidth, &RenderStyle::borderRightWidth, &RenderStyle::setBorderRightWidth));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyBorderTopWidth, &RenderStyle::borderTopWidth, &RenderStyle::setBorderTopWidth));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyBorderBottomWidth, &RenderStyle::borderBottomWidth, &RenderStyle::setBorderBottomWidth));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginLeft, &RenderStyle::marginLeft, &RenderStyle::setMarginLeft));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginRight, &RenderStyle::marginRight, &RenderStyle::setMarginRight));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginTop, &RenderStyle::marginTop, &RenderStyle::setMarginTop));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginBottom, &RenderStyle::marginBottom, &RenderStyle::setMarginBottom));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingLeft, &RenderStyle::paddingLeft, &RenderStyle::setPaddingLeft));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingRight, &RenderStyle::paddingRight, &RenderStyle::setPaddingRight));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingTop, &RenderStyle::paddingTop, &RenderStyle::setPaddingTop));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingBottom, &RenderStyle::paddingBottom, &RenderStyle::setPaddingBottom));
        gPropertyWrappers->append(new PropertyWrapper<const Color&>(CSSPropertyColor, &RenderStyle::color, &RenderStyle::setColor));
        gPropertyWrappers->append(new PropertyWrapper<const Color&>(CSSPropertyBackgroundColor, &RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor));
        gPropertyWrappers->append(new PropertyWrapper<int>(CSSPropertyFontSize, &RenderStyle::fontSize, &RenderStyle::setBlendedFontSize));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyWebkitColumnRuleWidth, &RenderStyle::columnRuleWidth, &RenderStyle::setColumnRuleWidth));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWebkitColumnGap, &RenderStyle::columnGap, &RenderStyle::setColumnGap));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyWebkitColumnCount, &RenderStyle::columnCount, &RenderStyle::setColumnCount));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWebkitColumnWidth, &RenderStyle::columnWidth, &RenderStyle::setColumnWidth));
        gPropertyWrappers->append(new PropertyWrapper<short>(CSSPropertyWebkitBorderHorizontalSpacing, &RenderStyle::horizontalBorderSpacing, &RenderStyle::setHorizontalBorderSpacing));
        gPropertyWrappers->append(new PropertyWrapper<short>(CSSPropertyWebkitBorderVerticalSpacing, &RenderStyle::verticalBorderSpacing, &RenderStyle::setVerticalBorderSpacing));
        gPropertyWrappers->append(new PropertyWrapper<int>(CSSPropertyZIndex, &RenderStyle::zIndex, &RenderStyle::setZIndex));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyLineHeight, &RenderStyle::lineHeight, &RenderStyle::setLineHeight));
        gPropertyWrappers->append(new PropertyWrapper<int>(CSSPropertyOutlineOffset, &RenderStyle::outlineOffset, &RenderStyle::setOutlineOffset));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyOutlineWidth, &RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyLetterSpacing, &RenderStyle::letterSpacing, &RenderStyle::setLetterSpacing));
        gPropertyWrappers->append(new PropertyWrapper<int>(CSSPropertyWordSpacing, &RenderStyle::wordSpacing, &RenderStyle::setWordSpacing));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWebkitPerspective, &RenderStyle::perspective, &RenderStyle::setPerspective));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWebkitPerspectiveOriginX, &RenderStyle::perspectiveOriginX, &RenderStyle::setPerspectiveOriginX));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWebkitPerspectiveOriginY, &RenderStyle::perspectiveOriginY, &RenderStyle::setPerspectiveOriginY));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWebkitTransformOriginX, &RenderStyle::transformOriginX, &RenderStyle::setTransformOriginX));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWebkitTransformOriginY, &RenderStyle::transformOriginY, &RenderStyle::setTransformOriginY));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWebkitTransformOriginZ, &RenderStyle::transformOriginZ, &RenderStyle::setTransformOriginZ));
        gPropertyWrappers->append(new PropertyWrapper<const IntSize&>(CSSPropertyWebkitBorderTopLeftRadius, &RenderStyle::borderTopLeftRadius, &RenderStyle::setBorderTopLeftRadius));
        gPropertyWrappers->append(new PropertyWrapper<const IntSize&>(CSSPropertyWebkitBorderTopRightRadius, &RenderStyle::borderTopRightRadius, &RenderStyle::setBorderTopRightRadius));
        gPropertyWrappers->append(new PropertyWrapper<const IntSize&>(CSSPropertyWebkitBorderBottomLeftRadius, &RenderStyle::borderBottomLeftRadius, &RenderStyle::setBorderBottomLeftRadius));
        gPropertyWrappers->append(new PropertyWrapper<const IntSize&>(CSSPropertyWebkitBorderBottomRightRadius, &RenderStyle::borderBottomRightRadius, &RenderStyle::setBorderBottomRightRadius));
        gPropertyWrappers->append(new PropertyWrapper<EVisibility>(CSSPropertyVisibility, &RenderStyle::visibility, &RenderStyle::setVisibility));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyZoom, &RenderStyle::zoom, &RenderStyle::setZoom));
        
#if USE(ACCELERATED_COMPOSITING)
        gPropertyWrappers->append(new PropertyWrapperAcceleratedOpacity());
        gPropertyWrappers->append(new PropertyWrapperAcceleratedTransform());
#else
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyOpacity, &RenderStyle::opacity, &RenderStyle::setOpacity));
        gPropertyWrappers->append(new PropertyWrapper<const TransformOperations&>(CSSPropertyWebkitTransform, &RenderStyle::transform, &RenderStyle::setTransform));
#endif

        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyWebkitColumnRuleColor, &RenderStyle::columnRuleColor, &RenderStyle::setColumnRuleColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyWebkitTextStrokeColor, &RenderStyle::textStrokeColor, &RenderStyle::setTextStrokeColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyWebkitTextFillColor, &RenderStyle::textFillColor, &RenderStyle::setTextFillColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyBorderLeftColor, &RenderStyle::borderLeftColor, &RenderStyle::setBorderLeftColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyBorderRightColor, &RenderStyle::borderRightColor, &RenderStyle::setBorderRightColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyBorderTopColor, &RenderStyle::borderTopColor, &RenderStyle::setBorderTopColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyBorderBottomColor, &RenderStyle::borderBottomColor, &RenderStyle::setBorderBottomColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyOutlineColor, &RenderStyle::outlineColor, &RenderStyle::setOutlineColor));

        // These are for shadows
        gPropertyWrappers->append(new PropertyWrapperShadow(CSSPropertyWebkitBoxShadow, &RenderStyle::boxShadow, &RenderStyle::setBoxShadow));
        gPropertyWrappers->append(new PropertyWrapperShadow(CSSPropertyTextShadow, &RenderStyle::textShadow, &RenderStyle::setTextShadow));

#if ENABLE(SVG)
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyFillOpacity, &RenderStyle::fillOpacity, &RenderStyle::setFillOpacity));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyFloodOpacity, &RenderStyle::floodOpacity, &RenderStyle::setFloodOpacity));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyStrokeOpacity, &RenderStyle::strokeOpacity, &RenderStyle::setStrokeOpacity));
#endif

        // TODO:
        // 
        //  CSSPropertyBackground, CSSPropertyBackgroundPosition
        //  CSSPropertyMinWidth, CSSPropertyMaxWidth, CSSPropertyMinHeight, CSSPropertyMaxHeight
        //  CSSPropertyTextIndent
        //  CSSPropertyVerticalAlign
        //  CSSPropertyWebkitBackgroundOrigin
        //  CSSPropertyWebkitBackgroundSize
        //  CSSPropertyWebkitMaskPosition
        //  CSSPropertyWebkitMaskOrigin
        //  CSSPropertyWebkitMaskSize
        // 
        // Compound properties that have components that should be animatable:
        // 
        //  CSSPropertyWebkitColumns
        //  CSSPropertyWebkitMask
        //  CSSPropertyWebkitBoxReflect

        // Make sure unused slots have a value
        for (unsigned int i = 0; i < static_cast<unsigned int>(numCSSProperties); ++i)
            gPropertyWrapperMap[i] = cInvalidPropertyWrapperIndex;

        // First we put the non-shorthand property wrappers into the map, so the shorthand-building
        // code can find them.
        size_t n = gPropertyWrappers->size();
        for (unsigned int i = 0; i < n; ++i) {
            ASSERT((*gPropertyWrappers)[i]->property() - firstCSSProperty < numCSSProperties);
            gPropertyWrapperMap[(*gPropertyWrappers)[i]->property() - firstCSSProperty] = i;
        }
        
        // Now add the shorthand wrappers.
        addShorthandProperties();
    }
}

static void addPropertyWrapper(int propertyID, PropertyWrapperBase* wrapper)
{
    int propIndex = propertyID - firstCSSProperty;

    ASSERT(gPropertyWrapperMap[propIndex] == cInvalidPropertyWrapperIndex);

    unsigned wrapperIndex = gPropertyWrappers->size();
    gPropertyWrappers->append(wrapper);
    gPropertyWrapperMap[propIndex] = wrapperIndex;
}

static void addShorthandProperties()
{
    static const int animatableShorthandProperties[] = {
        CSSPropertyBackground,      // for background-color
        CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft,
        CSSPropertyBorderColor, 
        CSSPropertyBorderWidth,
        CSSPropertyBorder,
        CSSPropertyBorderSpacing,
        CSSPropertyMargin,
        CSSPropertyOutline,
        CSSPropertyPadding,
        CSSPropertyWebkitTextStroke,
        CSSPropertyWebkitColumnRule,
        CSSPropertyWebkitBorderRadius,
        CSSPropertyWebkitTransformOrigin
    };

    for (unsigned i = 0; i < sizeof(animatableShorthandProperties) / sizeof(animatableShorthandProperties[0]); ++i) {
        int propertyID = animatableShorthandProperties[i];
        CSSPropertyLonghand longhand = longhandForProperty(propertyID);
        if (longhand.length() > 0)
            addPropertyWrapper(propertyID, new ShorthandPropertyWrapper(propertyID, longhand));
    }

    // 'font' is not in the shorthand map.
    static const int animatableFontProperties[] = {
        CSSPropertyFontSize,
        CSSPropertyFontWeight
    };

    CSSPropertyLonghand fontLonghand(animatableFontProperties, sizeof(animatableFontProperties) / sizeof(animatableFontProperties[0]));
    addPropertyWrapper(CSSPropertyFont, new ShorthandPropertyWrapper(CSSPropertyFont, fontLonghand));
}

static PropertyWrapperBase* wrapperForProperty(int propertyID)
{
    int propIndex = propertyID - firstCSSProperty;
    if (propIndex >= 0 && propIndex < numCSSProperties) {
        int wrapperIndex = gPropertyWrapperMap[propIndex];
        if (wrapperIndex >= 0)
            return (*gPropertyWrappers)[wrapperIndex];
    }
    return 0;
}

AnimationBase::AnimationBase(const Animation* transition, RenderObject* renderer, CompositeAnimation* compAnim)
    : m_animState(AnimationStateNew)
    , m_isAnimating(false)
    , m_startTime(0)
    , m_pauseTime(-1)
    , m_requestedStartTime(0)
    , m_object(renderer)
    , m_animation(const_cast<Animation*>(transition))
    , m_compAnim(compAnim)
    , m_fallbackAnimating(false)
    , m_transformFunctionListValid(false)
    , m_nextIterationDuration(-1)
    , m_next(0)
{
    // Compute the total duration
    m_totalDuration = -1;
    if (m_animation->iterationCount() > 0)
        m_totalDuration = m_animation->duration() * m_animation->iterationCount();
}

AnimationBase::~AnimationBase()
{
    m_compAnim->animationController()->removeFromStyleAvailableWaitList(this);
    m_compAnim->animationController()->removeFromStartTimeResponseWaitList(this);
}

bool AnimationBase::propertiesEqual(int prop, const RenderStyle* a, const RenderStyle* b)
{
    ensurePropertyMap();
    if (prop == cAnimateAll) {
        size_t n = gPropertyWrappers->size();
        for (unsigned int i = 0; i < n; ++i) {
            PropertyWrapperBase* wrapper = (*gPropertyWrappers)[i];
            // No point comparing shorthand wrappers for 'all'.
            if (!wrapper->isShorthandWrapper() && !wrapper->equals(a, b))
                return false;
        }
    } else {
        PropertyWrapperBase* wrapper = wrapperForProperty(prop);
        if (wrapper)
            return wrapper->equals(a, b);
    }
    return true;
}

int AnimationBase::getPropertyAtIndex(int i, bool& isShorthand)
{
    ensurePropertyMap();
    if (i < 0 || i >= static_cast<int>(gPropertyWrappers->size()))
        return CSSPropertyInvalid;

    PropertyWrapperBase* wrapper = (*gPropertyWrappers)[i];
    isShorthand = wrapper->isShorthandWrapper();
    return wrapper->property();
}

int AnimationBase::getNumProperties()
{
    ensurePropertyMap();
    return gPropertyWrappers->size();
}

// Returns true if we need to start animation timers
bool AnimationBase::blendProperties(const AnimationBase* anim, int prop, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double progress)
{
    ASSERT(prop != cAnimateAll);

    ensurePropertyMap();
    PropertyWrapperBase* wrapper = wrapperForProperty(prop);
    if (wrapper) {
        wrapper->blend(anim, dst, a, b, progress);
#if USE(ACCELERATED_COMPOSITING)
        return !wrapper->animationIsAccelerated() || anim->isFallbackAnimating();
#else
        return true;
#endif
    }

    return false;
}

#if USE(ACCELERATED_COMPOSITING)
bool AnimationBase::animationOfPropertyIsAccelerated(int prop)
{
    ensurePropertyMap();
    PropertyWrapperBase* wrapper = wrapperForProperty(prop);
    return wrapper ? wrapper->animationIsAccelerated() : false;
}
#endif

void AnimationBase::setChanged(Node* node)
{
    ASSERT(!node || (node->document() && !node->document()->inPageCache()));
    if (node)
        node->setChanged(AnimationStyleChange);
}

double AnimationBase::duration() const
{
    return m_animation->duration();
}

bool AnimationBase::playStatePlaying() const
{
    return m_animation->playState() == AnimPlayStatePlaying;
}

bool AnimationBase::animationsMatch(const Animation* anim) const
{
    return m_animation->animationsMatch(anim);
}

void AnimationBase::updateStateMachine(AnimStateInput input, double param)
{
    // If we get AnimationStateInputRestartAnimation then we force a new animation, regardless of state.
    if (input == AnimationStateInputMakeNew) {
        if (m_animState == AnimationStateStartWaitStyleAvailable)
            m_compAnim->animationController()->removeFromStyleAvailableWaitList(this);
        m_animState = AnimationStateNew;
        m_startTime = 0;
        m_pauseTime = -1;
        m_requestedStartTime = 0;
        m_nextIterationDuration = -1;
        endAnimation(false);
        return;
    }

    if (input == AnimationStateInputRestartAnimation) {
        if (m_animState == AnimationStateStartWaitStyleAvailable)
            m_compAnim->animationController()->removeFromStyleAvailableWaitList(this);
        m_animState = AnimationStateNew;
        m_startTime = 0;
        m_pauseTime = -1;
        m_requestedStartTime = 0;
        m_nextIterationDuration = -1;
        endAnimation(false);

        if (!paused())
            updateStateMachine(AnimationStateInputStartAnimation, -1);
        return;
    }

    if (input == AnimationStateInputEndAnimation) {
        if (m_animState == AnimationStateStartWaitStyleAvailable)
            m_compAnim->animationController()->removeFromStyleAvailableWaitList(this);
        m_animState = AnimationStateDone;
        endAnimation(true);
        return;
    }

    if (input == AnimationStateInputPauseOverride) {
        if (m_animState == AnimationStateStartWaitResponse) {
            // If we are in AnimationStateStartWaitResponse, the animation will get canceled before 
            // we get a response, so move to the next state.
            endAnimation(false);
            updateStateMachine(AnimationStateInputStartTimeSet, beginAnimationUpdateTime());
        }
        return;
    }

    if (input == AnimationStateInputResumeOverride) {
        if (m_animState == AnimationStateLooping || m_animState == AnimationStateEnding) {
            // Start the animation
            startAnimation(m_startTime);
        }
        return;
    }

    // Execute state machine
    switch (m_animState) {
        case AnimationStateNew:
            ASSERT(input == AnimationStateInputStartAnimation || input == AnimationStateInputPlayStateRunnning || input == AnimationStateInputPlayStatePaused);
            if (input == AnimationStateInputStartAnimation || input == AnimationStateInputPlayStateRunnning) {
                m_requestedStartTime = beginAnimationUpdateTime();
                m_animState = AnimationStateStartWaitTimer;
            }
            break;
        case AnimationStateStartWaitTimer:
            ASSERT(input == AnimationStateInputStartTimerFired || input == AnimationStateInputPlayStatePaused);

            if (input == AnimationStateInputStartTimerFired) {
                ASSERT(param >= 0);
                // Start timer has fired, tell the animation to start and wait for it to respond with start time
                m_animState = AnimationStateStartWaitStyleAvailable;
                m_compAnim->animationController()->addToStyleAvailableWaitList(this);

                // Trigger a render so we can start the animation
                if (m_object)
                    m_compAnim->animationController()->addNodeChangeToDispatch(m_object->element());
            } else {
                ASSERT(!paused());
                // We're waiting for the start timer to fire and we got a pause. Cancel the timer, pause and wait
                m_pauseTime = beginAnimationUpdateTime();
                m_animState = AnimationStatePausedWaitTimer;
            }
            break;
        case AnimationStateStartWaitStyleAvailable:
            ASSERT(input == AnimationStateInputStyleAvailable || input == AnimationStateInputPlayStatePaused);

            // Start timer has fired, tell the animation to start and wait for it to respond with start time
            m_animState = AnimationStateStartWaitResponse;

            overrideAnimations();

            // Send start event, if needed
            onAnimationStart(0); // The elapsedTime is always 0 here

            // Start the animation
            if (overridden()) {
                // We won't try to start accelerated animations if we are overridden and
                // just move on to the next state.
                m_animState = AnimationStateStartWaitResponse;
                m_fallbackAnimating = true;
                updateStateMachine(AnimationStateInputStartTimeSet, beginAnimationUpdateTime());
            }
            else {
                bool started = startAnimation(0);
                m_compAnim->animationController()->addToStartTimeResponseWaitList(this, started);
                m_fallbackAnimating = !started;
            }
            break;
        case AnimationStateStartWaitResponse:
            ASSERT(input == AnimationStateInputStartTimeSet || input == AnimationStateInputPlayStatePaused);

            if (input == AnimationStateInputStartTimeSet) {
                ASSERT(param >= 0);
                // We have a start time, set it, unless the startTime is already set
                if (m_startTime <= 0)
                    m_startTime = param;

                // Decide whether to go into looping or ending state
                goIntoEndingOrLoopingState();

                // Dispatch updateRendering so we can start the animation
                if (m_object)
                    m_compAnim->animationController()->addNodeChangeToDispatch(m_object->element());
            } else {
                // We are pausing while waiting for a start response. Cancel the animation and wait. When 
                // we unpause, we will act as though the start timer just fired
                m_pauseTime = -1;
                endAnimation(false);
                m_animState = AnimationStatePausedWaitResponse;
            }
            break;
        case AnimationStateLooping:
            ASSERT(input == AnimationStateInputLoopTimerFired || input == AnimationStateInputPlayStatePaused);

            if (input == AnimationStateInputLoopTimerFired) {
                ASSERT(param >= 0);
                // Loop timer fired, loop again or end.
                onAnimationIteration(param);

                // Decide whether to go into looping or ending state
                goIntoEndingOrLoopingState();
            } else {
                // We are pausing while running. Cancel the animation and wait
                m_pauseTime = beginAnimationUpdateTime();
                endAnimation(false);
                m_animState = AnimationStatePausedRun;
            }
            break;
        case AnimationStateEnding:
            ASSERT(input == AnimationStateInputEndTimerFired || input == AnimationStateInputPlayStatePaused);

            if (input == AnimationStateInputEndTimerFired) {
                ASSERT(param >= 0);
                // End timer fired, finish up
                onAnimationEnd(param);

                m_animState = AnimationStateDone;
                
                if (m_object) {
                    resumeOverriddenAnimations();

                    // Fire off another style change so we can set the final value
                    m_compAnim->animationController()->addNodeChangeToDispatch(m_object->element());
                }
            } else {
                // We are pausing while running. Cancel the animation and wait
                m_pauseTime = beginAnimationUpdateTime();
                endAnimation(false);
                m_animState = AnimationStatePausedRun;
            }
            // |this| may be deleted here
            break;
        case AnimationStatePausedWaitTimer:
            ASSERT(input == AnimationStateInputPlayStateRunnning);
            ASSERT(paused());
            // Update the times
            m_startTime += beginAnimationUpdateTime() - m_pauseTime;
            m_pauseTime = -1;

            // we were waiting for the start timer to fire, go back and wait again
            m_animState = AnimationStateNew;
            updateStateMachine(AnimationStateInputStartAnimation, 0);
            break;
        case AnimationStatePausedWaitResponse:
        case AnimationStatePausedRun:
            // We treat these two cases the same. The only difference is that, when we are in
            // AnimationStatePausedWaitResponse, we don't yet have a valid startTime, so we send 0 to startAnimation.
            // When the AnimationStateInputStartTimeSet comes in and we were in AnimationStatePausedRun, we will notice
            // that we have already set the startTime and will ignore it.
            ASSERT(input == AnimationStateInputPlayStateRunnning || input == AnimationStateInputStartTimeSet);
            ASSERT(paused());
            
            // If we are paused, but we get the callback that notifies us that an accelerated animation started,
            // then we ignore the start time and just move into the paused-run state.
            if (m_animState == AnimationStatePausedWaitResponse && input == AnimationStateInputStartTimeSet) {
                m_animState = AnimationStatePausedRun;
                ASSERT(m_startTime == 0);
                m_startTime = param;
                m_pauseTime += m_startTime;
                break;
            }
            
            // Update the times
            if (m_animState == AnimationStatePausedRun)
                m_startTime += beginAnimationUpdateTime() - m_pauseTime;
            else
                m_startTime = 0;
            m_pauseTime = -1;

            // We were waiting for a begin time response from the animation, go back and wait again
            m_animState = AnimationStateStartWaitResponse;

            // Start the animation
            if (overridden()) {
                // We won't try to start accelerated animations if we are overridden and
                // just move on to the next state.
                updateStateMachine(AnimationStateInputStartTimeSet, beginAnimationUpdateTime());
                m_fallbackAnimating = true;
            } else {
                bool started = startAnimation(m_startTime);
                m_compAnim->animationController()->addToStartTimeResponseWaitList(this, started);
                m_fallbackAnimating = !started;
            }
            break;
        case AnimationStateDone:
            // We're done. Stay in this state until we are deleted
            break;
    }
}
    
void AnimationBase::fireAnimationEventsIfNeeded()
{
    // If we are waiting for the delay time to expire and it has, go to the next state
    if (m_animState != AnimationStateStartWaitTimer && m_animState != AnimationStateLooping && m_animState != AnimationStateEnding)
        return;

    // We have to make sure to keep a ref to the this pointer, because it could get destroyed
    // during an animation callback that might get called. Since the owner is a CompositeAnimation
    // and it ref counts this object, we will keep a ref to that instead. That way the AnimationBase
    // can still access the resources of its CompositeAnimation as needed.
    RefPtr<AnimationBase> protector(this);
    RefPtr<CompositeAnimation> compProtector(m_compAnim);
    
    // Check for start timeout
    if (m_animState == AnimationStateStartWaitTimer) {
        if (beginAnimationUpdateTime() - m_requestedStartTime >= m_animation->delay())
            updateStateMachine(AnimationStateInputStartTimerFired, 0);
        return;
    }
    
    double elapsedDuration = beginAnimationUpdateTime() - m_startTime;
    // FIXME: we need to ensure that elapsedDuration is never < 0. If it is, this suggests that
    // we had a recalcStyle() outside of beginAnimationUpdate()/endAnimationUpdate().
    // Also check in getTimeToNextEvent().
    elapsedDuration = max(elapsedDuration, 0.0);
    
    // Check for end timeout
    if (m_totalDuration >= 0 && elapsedDuration >= m_totalDuration) {
        // Fire an end event
        updateStateMachine(AnimationStateInputEndTimerFired, m_totalDuration);
    } else {
        // Check for iteration timeout
        if (m_nextIterationDuration < 0) {
            // Hasn't been set yet, set it
            double durationLeft = m_animation->duration() - fmod(elapsedDuration, m_animation->duration());
            m_nextIterationDuration = elapsedDuration + durationLeft;
        }
        
        if (elapsedDuration >= m_nextIterationDuration) {
            // Set to the next iteration
            double previous = m_nextIterationDuration;
            double durationLeft = m_animation->duration() - fmod(elapsedDuration, m_animation->duration());
            m_nextIterationDuration = elapsedDuration + durationLeft;
            
            // Send the event
            updateStateMachine(AnimationStateInputLoopTimerFired, previous);
        }
    }
}

void AnimationBase::updatePlayState(bool run)
{
    if (paused() == run || isNew())
        updateStateMachine(run ? AnimationStateInputPlayStateRunnning : AnimationStateInputPlayStatePaused, -1);
}

double AnimationBase::timeToNextService()
{
    // Returns the time at which next service is required. -1 means no service is required. 0 means 
    // service is required now, and > 0 means service is required that many seconds in the future.
    if (paused() || isNew())
        return -1;
    
    if (m_animState == AnimationStateStartWaitTimer) {
        double timeFromNow = m_animation->delay() - (beginAnimationUpdateTime() - m_requestedStartTime);
        return max(timeFromNow, 0.0);
    }
    
    fireAnimationEventsIfNeeded();
        
    // In all other cases, we need service right away.
    return 0;
}

double AnimationBase::progress(double scale, double offset, const TimingFunction* tf) const
{
    if (preActive())
        return 0;

    double elapsedTime = getElapsedTime();

    double dur = m_animation->duration();
    if (m_animation->iterationCount() > 0)
        dur *= m_animation->iterationCount();

    if (postActive() || !m_animation->duration() || (m_animation->iterationCount() > 0 && elapsedTime >= dur))
        return 1.0;

    // Compute the fractional time, taking into account direction.
    // There is no need to worry about iterations, we assume that we would have
    // short circuited above if we were done.
    double fractionalTime = elapsedTime / m_animation->duration();
    int integralTime = static_cast<int>(fractionalTime);
    fractionalTime -= integralTime;

    if (m_animation->direction() && (integralTime & 1))
        fractionalTime = 1 - fractionalTime;

    if (scale != 1 || offset)
        fractionalTime = (fractionalTime - offset) * scale;
        
    if (!tf)
        tf = &m_animation->timingFunction();

    if (tf->type() == LinearTimingFunction)
        return fractionalTime;

    // Cubic bezier.
    double result = solveCubicBezierFunction(tf->x1(),
                                            tf->y1(),
                                            tf->x2(),
                                            tf->y2(),
                                            fractionalTime, m_animation->duration());
    return result;
}

void AnimationBase::getTimeToNextEvent(double& time, bool& isLooping) const
{
    // Decide when the end or loop event needs to fire
    double totalDuration = -1;
    if (m_animation->iterationCount() > 0)
        totalDuration = m_animation->duration() * m_animation->iterationCount();

    const double elapsedDuration = max(beginAnimationUpdateTime() - m_startTime, 0.0);
    double durationLeft = 0;
    double nextIterationTime = m_totalDuration;

    if (m_totalDuration < 0 || elapsedDuration < m_totalDuration) {
        durationLeft = m_animation->duration() - fmod(elapsedDuration, m_animation->duration());
        nextIterationTime = elapsedDuration + durationLeft;
    }
    
    if (m_totalDuration < 0 || nextIterationTime < m_totalDuration) {
        // We are not at the end yet
        ASSERT(nextIterationTime > 0);
        isLooping = true;
    } else {
        // We are at the end
        isLooping = false;
    }
    
    time = durationLeft;
}

void AnimationBase::goIntoEndingOrLoopingState()
{
    double t;
    bool isLooping;
    getTimeToNextEvent(t, isLooping);
    m_animState = isLooping ? AnimationStateLooping : AnimationStateEnding;
}
  
void AnimationBase::pauseAtTime(double t)
{
    updatePlayState(false);
    m_pauseTime = m_startTime + t - m_animation->delay();
}

double AnimationBase::beginAnimationUpdateTime() const
{
    return m_compAnim->animationController()->beginAnimationUpdateTime();
}

double AnimationBase::getElapsedTime() const
{
    if (paused())    
        return m_pauseTime - m_startTime;
    if (m_startTime <= 0)
        return 0;
    if (postActive())
        return 1;
    return beginAnimationUpdateTime() - m_startTime;
}
    
} // namespace WebCore
