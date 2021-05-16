/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#include "StyleRareNonInheritedData.h"

#include "CSSStyleSelector.h"
#include "RenderStyle.h"

namespace WebCore {

StyleRareNonInheritedData::StyleRareNonInheritedData()
    : lineClamp(RenderStyle::initialLineClamp())
    , opacity(RenderStyle::initialOpacity())
    , m_content(0)
    , m_counterDirectives(0)
    , userDrag(RenderStyle::initialUserDrag())
    , textOverflow(RenderStyle::initialTextOverflow())
    , marginTopCollapse(MCOLLAPSE)
    , marginBottomCollapse(MCOLLAPSE)
    , matchNearestMailBlockquoteColor(RenderStyle::initialMatchNearestMailBlockquoteColor())
    , m_appearance(RenderStyle::initialAppearance())
    , m_borderFit(RenderStyle::initialBorderFit())
#if USE(ACCELERATED_COMPOSITING)
    , m_runningAcceleratedAnimation(false)
#endif
    , m_imageLoadingBorder(false)
    , m_boxShadow(0)
    , m_animations(0)
    , m_transitions(0)
    , m_mask(FillLayer(MaskFillLayer))
    , m_transformStyle3D(RenderStyle::initialTransformStyle3D())
    , m_backfaceVisibility(RenderStyle::initialBackfaceVisibility())
    , m_perspective(RenderStyle::initialPerspective())
    , m_perspectiveOriginX(RenderStyle::initialPerspectiveOriginX())
    , m_perspectiveOriginY(RenderStyle::initialPerspectiveOriginY())
#if ENABLE(XBL)
    , bindingURI(0)
#endif
{
}

StyleRareNonInheritedData::StyleRareNonInheritedData(const StyleRareNonInheritedData& o)
    : RefCounted<StyleRareNonInheritedData>()
    , lineClamp(o.lineClamp)
    , opacity(o.opacity)
    , flexibleBox(o.flexibleBox)
    , marquee(o.marquee)
    , m_multiCol(o.m_multiCol)
    , m_transform(o.m_transform)
    , m_content(0)
    , m_counterDirectives(0)
    , userDrag(o.userDrag)
    , textOverflow(o.textOverflow)
    , marginTopCollapse(o.marginTopCollapse)
    , marginBottomCollapse(o.marginBottomCollapse)
    , matchNearestMailBlockquoteColor(o.matchNearestMailBlockquoteColor)
    , m_appearance(o.m_appearance)
    , m_borderFit(o.m_borderFit)
#if USE(ACCELERATED_COMPOSITING)
    , m_runningAcceleratedAnimation(o.m_runningAcceleratedAnimation)
#endif
    , m_imageLoadingBorder(o.m_imageLoadingBorder)
    , m_boxShadow(o.m_boxShadow ? new ShadowData(*o.m_boxShadow) : 0)
    , m_boxReflect(o.m_boxReflect)
    , m_animations(o.m_animations ? new AnimationList(*o.m_animations) : 0)
    , m_transitions(o.m_transitions ? new AnimationList(*o.m_transitions) : 0)
    , m_mask(o.m_mask)
    , m_maskBoxImage(o.m_maskBoxImage)
    , m_transformStyle3D(o.m_transformStyle3D)
    , m_backfaceVisibility(o.m_backfaceVisibility)
    , m_perspective(o.m_perspective)
    , m_perspectiveOriginX(o.m_perspectiveOriginX)
    , m_perspectiveOriginY(o.m_perspectiveOriginY)
#if ENABLE(XBL)
    , bindingURI(o.bindingURI ? o.bindingURI->copy() : 0)
#endif
{
}

StyleRareNonInheritedData::~StyleRareNonInheritedData()
{
}

#if ENABLE(XBL)
bool StyleRareNonInheritedData::bindingsEquivalent(const StyleRareNonInheritedData& o) const
{
    if (this == &o) return true;
    if (!bindingURI && o.bindingURI || bindingURI && !o.bindingURI)
        return false;
    if (bindingURI && o.bindingURI && (*bindingURI != *o.bindingURI))
        return false;
    return true;
}
#endif

bool StyleRareNonInheritedData::operator==(const StyleRareNonInheritedData& o) const
{
    return lineClamp == o.lineClamp
#if ENABLE(DASHBOARD_SUPPORT)
        && m_dashboardRegions == o.m_dashboardRegions
#endif
        && opacity == o.opacity
        && flexibleBox == o.flexibleBox
        && marquee == o.marquee
        && m_multiCol == o.m_multiCol
        && m_transform == o.m_transform
        && m_content == o.m_content
        && m_counterDirectives == o.m_counterDirectives
        && userDrag == o.userDrag
        && textOverflow == o.textOverflow
        && marginTopCollapse == o.marginTopCollapse
        && marginBottomCollapse == o.marginBottomCollapse
        && matchNearestMailBlockquoteColor == o.matchNearestMailBlockquoteColor
        && m_appearance == o.m_appearance
        && m_borderFit == o.m_borderFit
#if USE(ACCELERATED_COMPOSITING)
        && !m_runningAcceleratedAnimation && !o.m_runningAcceleratedAnimation
#endif
        && m_imageLoadingBorder == o.m_imageLoadingBorder
        && shadowDataEquivalent(o)
        && reflectionDataEquivalent(o)
        && animationDataEquivalent(o)
        && transitionDataEquivalent(o)
        && m_mask == o.m_mask
        && m_maskBoxImage == o.m_maskBoxImage
#if ENABLE(XBL)
        && bindingsEquivalent(o)
#endif
        && (m_transformStyle3D == o.m_transformStyle3D)
        && (m_backfaceVisibility == o.m_backfaceVisibility)
        && (m_perspective == o.m_perspective)
        && (m_perspectiveOriginX == o.m_perspectiveOriginX)
        && (m_perspectiveOriginY == o.m_perspectiveOriginY)
        ;
}

bool StyleRareNonInheritedData::shadowDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (!m_boxShadow && o.m_boxShadow || m_boxShadow && !o.m_boxShadow)
        return false;
    if (m_boxShadow && o.m_boxShadow && (*m_boxShadow != *o.m_boxShadow))
        return false;
    return true;
}

bool StyleRareNonInheritedData::reflectionDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (m_boxReflect != o.m_boxReflect) {
        if (!m_boxReflect || !o.m_boxReflect)
            return false;
        return *m_boxReflect == *o.m_boxReflect;
    }
    return true;

}

bool StyleRareNonInheritedData::animationDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (!m_animations && o.m_animations || m_animations && !o.m_animations)
        return false;
    if (m_animations && o.m_animations && (*m_animations != *o.m_animations))
        return false;
    return true;
}

bool StyleRareNonInheritedData::transitionDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (!m_transitions && o.m_transitions || m_transitions && !o.m_transitions)
        return false;
    if (m_transitions && o.m_transitions && (*m_transitions != *o.m_transitions))
        return false;
    return true;
}

} // namespace WebCore
