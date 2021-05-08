/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderStyle.h"

#include "cssstyleselector.h"
#include "RenderArena.h"

#include "StringHash.h"

namespace WebCore {

static RenderStyle* defaultStyle;

StyleSurroundData::StyleSurroundData()
    : margin(Fixed), padding(Auto)
{
}

StyleSurroundData::StyleSurroundData(const StyleSurroundData& o)
    : Shared<StyleSurroundData>()
    , offset(o.offset)
    , margin(o.margin)
    , padding(o.padding)
    , border(o.border)
{
}

bool StyleSurroundData::operator==(const StyleSurroundData& o) const
{
    return offset == o.offset && margin == o.margin && padding == o.padding && border == o.border;
}

StyleBoxData::StyleBoxData()
    : z_index(0), z_auto(true), boxSizing(CONTENT_BOX)
{
    // Initialize our min/max widths/heights.
    min_width = min_height = RenderStyle::initialMinSize();
    max_width = max_height = RenderStyle::initialMaxSize();
}

StyleBoxData::StyleBoxData(const StyleBoxData& o )
    : Shared<StyleBoxData>()
    , width(o.width)
    , height(o.height)
    , min_width(o.min_width)
    , max_width(o.max_width)
    , min_height(o.min_height)
    , max_height(o.max_height)
    , z_index(o.z_index)
    , z_auto(o.z_auto)
    , boxSizing(o.boxSizing)
{
}

bool StyleBoxData::operator==(const StyleBoxData& o) const
{
    return width == o.width &&
           height == o.height &&
           min_width == o.min_width &&
           max_width == o.max_width &&
           min_height == o.min_height &&
           max_height == o.max_height &&
           z_index == o.z_index &&
           z_auto == o.z_auto &&
           boxSizing == o.boxSizing;
}


StyleVisualData::StyleVisualData()
    : hasClip(false)
    , textDecoration(RenderStyle::initialTextDecoration())
    , colspan(1)
    , counter_increment(0)
    , counter_reset(0)
{
}

StyleVisualData::~StyleVisualData()
{
}

StyleVisualData::StyleVisualData(const StyleVisualData& o)
    : Shared<StyleVisualData>()
    , clip(o.clip)
    , hasClip(o.hasClip)
    , textDecoration(o.textDecoration)
    , colspan(o.colspan)
    , counter_increment(o.counter_increment)
    , counter_reset(o.counter_reset)
{
}

BackgroundLayer::BackgroundLayer()
    : m_image(RenderStyle::initialBackgroundImage())
    , m_bgAttachment(RenderStyle::initialBackgroundAttachment())
    , m_bgClip(RenderStyle::initialBackgroundClip())
    , m_bgOrigin(RenderStyle::initialBackgroundOrigin())
    , m_bgRepeat(RenderStyle::initialBackgroundRepeat())
    , m_bgComposite(RenderStyle::initialBackgroundComposite())
    , m_backgroundSize(RenderStyle::initialBackgroundSize())
    , m_imageSet(false)
    , m_attachmentSet(false)
    , m_clipSet(false)
    , m_originSet(false)
    , m_repeatSet(false)
    , m_xPosSet(false)
    , m_yPosSet(false)
    , m_compositeSet(false)
    , m_backgroundSizeSet(false)
    , m_next(0)
{
}

BackgroundLayer::BackgroundLayer(const BackgroundLayer& o)
    : m_image(o.m_image)
    , m_xPosition(o.m_xPosition)
    , m_yPosition(o.m_yPosition)
    , m_bgAttachment(o.m_bgAttachment)
    , m_bgClip(o.m_bgClip)
    , m_bgOrigin(o.m_bgOrigin)
    , m_bgRepeat(o.m_bgRepeat)
    , m_bgComposite(o.m_bgComposite)
    , m_backgroundSize(o.m_backgroundSize)
    , m_imageSet(o.m_imageSet)
    , m_attachmentSet(o.m_attachmentSet)
    , m_clipSet(o.m_clipSet)
    , m_originSet(o.m_originSet)
    , m_repeatSet(o.m_repeatSet)
    , m_xPosSet(o.m_xPosSet)
    , m_yPosSet(o.m_yPosSet)
    , m_compositeSet(o.m_compositeSet)
    , m_backgroundSizeSet(o.m_backgroundSizeSet)
    , m_next(o.m_next ? new BackgroundLayer(*o.m_next) : 0)
{
}

BackgroundLayer::~BackgroundLayer()
{
    delete m_next;
}

BackgroundLayer& BackgroundLayer::operator=(const BackgroundLayer& o)
{
    if (m_next != o.m_next) {
        delete m_next;
        m_next = o.m_next ? new BackgroundLayer(*o.m_next) : 0;
    }

    m_image = o.m_image;
    m_xPosition = o.m_xPosition;
    m_yPosition = o.m_yPosition;
    m_bgAttachment = o.m_bgAttachment;
    m_bgClip = o.m_bgClip;
    m_bgComposite = o.m_bgComposite;
    m_bgOrigin = o.m_bgOrigin;
    m_bgRepeat = o.m_bgRepeat;
    m_backgroundSize = o.m_backgroundSize;

    m_imageSet = o.m_imageSet;
    m_attachmentSet = o.m_attachmentSet;
    m_clipSet = o.m_clipSet;
    m_compositeSet = o.m_compositeSet;
    m_originSet = o.m_originSet;
    m_repeatSet = o.m_repeatSet;
    m_xPosSet = o.m_xPosSet;
    m_yPosSet = o.m_yPosSet;
    m_backgroundSizeSet = o.m_backgroundSizeSet;

    return *this;
}

bool BackgroundLayer::operator==(const BackgroundLayer& o) const
{
    return m_image == o.m_image && m_xPosition == o.m_xPosition && m_yPosition == o.m_yPosition &&
           m_bgAttachment == o.m_bgAttachment && m_bgClip == o.m_bgClip && 
           m_bgComposite == o.m_bgComposite && m_bgOrigin == o.m_bgOrigin && m_bgRepeat == o.m_bgRepeat &&
           m_backgroundSize.width == o.m_backgroundSize.width && m_backgroundSize.height == o.m_backgroundSize.height && 
           m_imageSet == o.m_imageSet && m_attachmentSet == o.m_attachmentSet && m_compositeSet == o.m_compositeSet && 
           m_repeatSet == o.m_repeatSet && m_xPosSet == o.m_xPosSet && m_yPosSet == o.m_yPosSet && 
           m_backgroundSizeSet == o.m_backgroundSizeSet && 
           ((m_next && o.m_next) ? *m_next == *o.m_next : m_next == o.m_next);
}

void BackgroundLayer::fillUnsetProperties()
{
    BackgroundLayer* curr;
    for (curr = this; curr && curr->isBackgroundImageSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_image = pattern->m_image;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isBackgroundXPositionSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_xPosition = pattern->m_xPosition;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isBackgroundYPositionSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_yPosition = pattern->m_yPosition;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isBackgroundAttachmentSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_bgAttachment = pattern->m_bgAttachment;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isBackgroundClipSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_bgClip = pattern->m_bgClip;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isBackgroundCompositeSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_bgComposite = pattern->m_bgComposite;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isBackgroundOriginSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_bgOrigin = pattern->m_bgOrigin;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isBackgroundRepeatSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_bgRepeat = pattern->m_bgRepeat;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isBackgroundSizeSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_backgroundSize = pattern->m_backgroundSize;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
}

void BackgroundLayer::cullEmptyLayers()
{
    BackgroundLayer *next;
    for (BackgroundLayer *p = this; p; p = next) {
        next = p->m_next;
        if (next && !next->isBackgroundImageSet() &&
            !next->isBackgroundXPositionSet() && !next->isBackgroundYPositionSet() &&
            !next->isBackgroundAttachmentSet() && !next->isBackgroundClipSet() &&
            !next->isBackgroundCompositeSet() && !next->isBackgroundOriginSet() &&
            !next->isBackgroundRepeatSet() && !next->isBackgroundSizeSet()) {
            delete next;
            p->m_next = 0;
            break;
        }
    }
}

StyleBackgroundData::StyleBackgroundData()
{
}

StyleBackgroundData::StyleBackgroundData(const StyleBackgroundData& o)
    : Shared<StyleBackgroundData>(), m_background(o.m_background), m_outline(o.m_outline)
{
}

bool StyleBackgroundData::operator==(const StyleBackgroundData& o) const
{
    return m_background == o.m_background && m_color == o.m_color && m_outline == o.m_outline;
}

StyleMarqueeData::StyleMarqueeData()
    : increment(RenderStyle::initialMarqueeIncrement())
    , speed(RenderStyle::initialMarqueeSpeed())
    , loops(RenderStyle::initialMarqueeLoopCount())
    , behavior(RenderStyle::initialMarqueeBehavior())
    , direction(RenderStyle::initialMarqueeDirection())
{
}

StyleMarqueeData::StyleMarqueeData(const StyleMarqueeData& o)
    : Shared<StyleMarqueeData>()
    , increment(o.increment)
    , speed(o.speed)
    , loops(o.loops)
    , behavior(o.behavior)
    , direction(o.direction) 
{
}

bool StyleMarqueeData::operator==(const StyleMarqueeData& o) const
{
    return increment == o.increment && speed == o.speed && direction == o.direction &&
           behavior == o.behavior && loops == o.loops;
}

StyleFlexibleBoxData::StyleFlexibleBoxData()
    : flex(RenderStyle::initialBoxFlex())
    , flex_group(RenderStyle::initialBoxFlexGroup())
    , ordinal_group(RenderStyle::initialBoxOrdinalGroup())
    , align(RenderStyle::initialBoxAlign())
    , pack(RenderStyle::initialBoxPack())
    , orient(RenderStyle::initialBoxOrient())
    , lines(RenderStyle::initialBoxLines())
{
}

StyleFlexibleBoxData::StyleFlexibleBoxData(const StyleFlexibleBoxData& o)
    : Shared<StyleFlexibleBoxData>()
    , flex(o.flex)
    , flex_group(o.flex_group)
    , ordinal_group(o.ordinal_group)
    , align(o.align)
    , pack(o.pack)
    , orient(o.orient)
    , lines(o.lines)
{
}

bool StyleFlexibleBoxData::operator==(const StyleFlexibleBoxData& o) const
{
    return flex == o.flex && flex_group == o.flex_group &&
           ordinal_group == o.ordinal_group && align == o.align &&
           pack == o.pack && orient == o.orient && lines == o.lines;
}

StyleCSS3NonInheritedData::StyleCSS3NonInheritedData()
    : lineClamp(RenderStyle::initialLineClamp())
    , opacity(RenderStyle::initialOpacity())
    , userDrag(RenderStyle::initialUserDrag())
    , userSelect(RenderStyle::initialUserSelect())
    , textOverflow(RenderStyle::initialTextOverflow())
    , marginTopCollapse(MCOLLAPSE)
    , marginBottomCollapse(MCOLLAPSE)
    , m_appearance(RenderStyle::initialAppearance())
#ifndef KHTML_NO_XBL
    , bindingURI(0)
#endif
{
}

StyleCSS3NonInheritedData::StyleCSS3NonInheritedData(const StyleCSS3NonInheritedData& o)
    : Shared<StyleCSS3NonInheritedData>()
    , lineClamp(o.lineClamp)
    , opacity(o.opacity)
    , flexibleBox(o.flexibleBox)
    , marquee(o.marquee)
    , userDrag(o.userDrag)
    , userSelect(o.userSelect)
    , textOverflow(o.textOverflow)
    , marginTopCollapse(o.marginTopCollapse)
    , marginBottomCollapse(o.marginBottomCollapse)
    , m_appearance(o.m_appearance)
#ifndef KHTML_NO_XBL
    , bindingURI(o.bindingURI ? o.bindingURI->copy() : 0)
#endif
{
}

StyleCSS3NonInheritedData::~StyleCSS3NonInheritedData()
{
#ifndef KHTML_NO_XBL
    delete bindingURI;
#endif
}

#ifndef KHTML_NO_XBL
bool StyleCSS3NonInheritedData::bindingsEquivalent(const StyleCSS3NonInheritedData& o) const
{
    if (this == &o) return true;
    if (!bindingURI && o.bindingURI || bindingURI && !o.bindingURI)
        return false;
    if (bindingURI && o.bindingURI && (*bindingURI != *o.bindingURI))
        return false;
    return true;
}
#endif

bool StyleCSS3NonInheritedData::operator==(const StyleCSS3NonInheritedData& o) const
{
    return opacity == o.opacity && flexibleBox == o.flexibleBox && marquee == o.marquee &&
           userDrag == o.userDrag && userSelect == o.userSelect && textOverflow == o.textOverflow &&
           marginTopCollapse == o.marginTopCollapse && marginBottomCollapse == o.marginBottomCollapse &&
           m_appearance == o.m_appearance
#ifndef KHTML_NO_XBL
           && bindingsEquivalent(o)
#endif
           && lineClamp == o.lineClamp && m_dashboardRegions == o.m_dashboardRegions
    ;
}

StyleCSS3InheritedData::StyleCSS3InheritedData()
    : textShadow(0)
    , textSecurity(RenderStyle::initialTextSecurity())
    , userModify(READ_ONLY)
    , wordBreak(RenderStyle::initialWordBreak())
    , wordWrap(RenderStyle::initialWordWrap())
    , nbspMode(NBNORMAL)
    , khtmlLineBreak(LBNORMAL)
    , textSizeAdjust(RenderStyle::initialTextSizeAdjust())
    , tapHighlightColor(RenderStyle::initialTapHighlightColor())
    , resize(RenderStyle::initialResize())
{

}

StyleCSS3InheritedData::StyleCSS3InheritedData(const StyleCSS3InheritedData& o)
    : Shared<StyleCSS3InheritedData>()
    , textShadow(o.textShadow ? new ShadowData(*o.textShadow) : 0)
    , highlight(o.highlight)
    , textSecurity(o.textSecurity)
    , userModify(o.userModify)
    , wordBreak(o.wordBreak)
    , wordWrap(o.wordWrap)
    , nbspMode(o.nbspMode)
    , khtmlLineBreak(o.khtmlLineBreak)
    , textSizeAdjust(o.textSizeAdjust)
    , tapHighlightColor(o.tapHighlightColor)
    , resize(o.resize)
{
}

StyleCSS3InheritedData::~StyleCSS3InheritedData()
{
    delete textShadow;
}

bool StyleCSS3InheritedData::operator==(const StyleCSS3InheritedData& o) const
{
    return userModify == o.userModify
        && shadowDataEquivalent(o)
        && highlight == o.highlight
        && wordBreak == o.wordBreak
        && wordWrap == o.wordWrap
        && nbspMode == o.nbspMode
        && khtmlLineBreak == o.khtmlLineBreak
        && textSizeAdjust == o.textSizeAdjust
        && tapHighlightColor == o.tapHighlightColor;
}

bool StyleCSS3InheritedData::shadowDataEquivalent(const StyleCSS3InheritedData& o) const
{
    if (!textShadow && o.textShadow || textShadow && !o.textShadow)
        return false;
    if (textShadow && o.textShadow && (*textShadow != *o.textShadow))
        return false;
    return true;
}

StyleInheritedData::StyleInheritedData()
    : indent(RenderStyle::initialTextIndent()), line_height(RenderStyle::initialLineHeight()), specified_line_height(RenderStyle::initialLineHeight()),
      style_image(RenderStyle::initialListStyleImage()),
      cursor_image(0), color(RenderStyle::initialColor()), 
      horizontal_border_spacing(RenderStyle::initialHorizontalBorderSpacing()), 
      vertical_border_spacing(RenderStyle::initialVerticalBorderSpacing()),
      widows(RenderStyle::initialWidows()), orphans(RenderStyle::initialOrphans()),
      page_break_inside(RenderStyle::initialPageBreak())
{
}

StyleInheritedData::~StyleInheritedData()
{
}

StyleInheritedData::StyleInheritedData(const StyleInheritedData& o )
    : Shared<StyleInheritedData>(),
      indent( o.indent ), line_height( o.line_height ), specified_line_height( o.specified_line_height ), style_image( o.style_image ),
      cursor_image( o.cursor_image ), font( o.font ),
      color( o.color ),
      horizontal_border_spacing( o.horizontal_border_spacing ),
      vertical_border_spacing( o.vertical_border_spacing ),
      widows(o.widows), orphans(o.orphans), page_break_inside(o.page_break_inside)
{
}

bool StyleInheritedData::operator==(const StyleInheritedData& o) const
{
    return
        indent == o.indent &&
        line_height == o.line_height &&
        specified_line_height == o.specified_line_height &&
        style_image == o.style_image &&
        cursor_image == o.cursor_image &&
        font == o.font &&
        color == o.color &&
        horizontal_border_spacing == o.horizontal_border_spacing &&
        vertical_border_spacing == o.vertical_border_spacing &&
        widows == o.widows &&
        orphans == o.orphans &&
        page_break_inside == o.page_break_inside;
}

// ----------------------------------------------------------

void* RenderStyle::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void RenderStyle::operator delete(void* ptr, size_t sz)
{
    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

void RenderStyle::arenaDelete(RenderArena *arena)
{
    RenderStyle *ps = pseudoStyle;
    RenderStyle *prev = 0;
    
    while (ps) {
        prev = ps;
        ps = ps->pseudoStyle;
        // to prevent a double deletion.
        // this works only because the styles below aren't really shared
        // Dirk said we need another construct as soon as these are shared
        prev->pseudoStyle = 0;
        prev->deref(arena);
    }
    delete content;
    
    delete this;
    
    // Recover the size left there for us by operator delete and free the memory.
    arena->free(*(size_t *)this, this);
}

inline RenderStyle *initDefaultStyle()
{
    if (!defaultStyle) {
        defaultStyle = ::new RenderStyle(true);
        defaultStyle->font().update();
    }
    return defaultStyle;
}

RenderStyle::RenderStyle()
    : box(initDefaultStyle()->box)
    , visual(defaultStyle->visual)
    , background(defaultStyle->background)
    , surround(defaultStyle->surround)
    , css3NonInheritedData(defaultStyle->css3NonInheritedData)
    , css3InheritedData(defaultStyle->css3InheritedData)
    , inherited(defaultStyle->inherited)
    , pseudoStyle(0)
    , content(0)
    , m_pseudoState(PseudoUnknown)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_ref(0)
#if SVG_SUPPORT
    , m_svgStyle(defaultStyle->m_svgStyle)
#endif
{
    setBitDefaults(); // Would it be faster to copy this from the default style?
}

RenderStyle::RenderStyle(bool)
    : pseudoStyle(0)
    , content(0)
    , m_pseudoState(PseudoUnknown)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_ref(1)
{
    setBitDefaults();

    box.init();
    visual.init();
    background.init();
    surround.init();
    css3NonInheritedData.init();
    css3NonInheritedData.access()->flexibleBox.init();
    css3NonInheritedData.access()->marquee.init();
    css3InheritedData.init();
    inherited.init();
    
#if SVG_SUPPORT
    m_svgStyle.init();
#endif
}

RenderStyle::RenderStyle(const RenderStyle& o)
    : inherited_flags(o.inherited_flags)
    , noninherited_flags(o.noninherited_flags)
    , box(o.box)
    , visual(o.visual )
    , background(o.background)
    , surround(o.surround)
    , css3NonInheritedData(o.css3NonInheritedData)
    , css3InheritedData(o.css3InheritedData)
    , inherited(o.inherited)
    , pseudoStyle(0)
    , content(0)
    , m_pseudoState(o.m_pseudoState)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_ref(0)
#if SVG_SUPPORT
    , m_svgStyle(o.m_svgStyle)
#endif
{
    if (o.content)
        content = new ContentData(o.content);
}

void RenderStyle::inheritFrom(const RenderStyle* inheritParent)
{
    css3InheritedData = inheritParent->css3InheritedData;
    inherited = inheritParent->inherited;
    inherited_flags = inheritParent->inherited_flags;
#if SVG_SUPPORT
    if (m_svgStyle != inheritParent->m_svgStyle)
        m_svgStyle.access()->inheritFrom(inheritParent->m_svgStyle.get());
#endif
}

RenderStyle::~RenderStyle()
{
}

bool RenderStyle::operator==(const RenderStyle& o) const
{
    // compare everything except the pseudoStyle pointer
    return inherited_flags == o.inherited_flags &&
            noninherited_flags == o.noninherited_flags &&
            box == o.box &&
            visual == o.visual &&
            background == o.background &&
            surround == o.surround &&
            (css3NonInheritedData.get() == o.css3NonInheritedData.get() || css3NonInheritedData == o.css3NonInheritedData) &&
            (css3InheritedData.get() == o.css3InheritedData.get() || css3InheritedData == o.css3InheritedData) &&
            (inherited.get() == o.inherited.get() || inherited == o.inherited)
#if SVG_SUPPORT
            && m_svgStyle == o.m_svgStyle
#endif
            ;
}

bool RenderStyle::isStyleAvailable() const
{
    return this != CSSStyleSelector::styleNotYetAvailable;
}

enum EPseudoBit { NO_BIT = 0x0, BEFORE_BIT = 0x1, AFTER_BIT = 0x2, FIRST_LINE_BIT = 0x4,
    FIRST_LETTER_BIT = 0x8, SELECTION_BIT = 0x10, FIRST_LINE_INHERITED_BIT = 0x20,
    FILE_UPLOAD_BUTTON_BIT = 0x40, SLIDER_THUMB_BIT = 0x80, SEARCH_CANCEL_BUTTON_BIT = 0x100, SEARCH_DECORATION_BIT = 0x200, 
    SEARCH_RESULTS_DECORATION_BIT = 0x400, SEARCH_RESULTS_BUTTON_BIT = 0x800 };

static inline int pseudoBit(RenderStyle::PseudoId pseudo)
{
    switch (pseudo) {
        case RenderStyle::BEFORE:
            return BEFORE_BIT;
        case RenderStyle::AFTER:
            return AFTER_BIT;
        case RenderStyle::FIRST_LINE:
            return FIRST_LINE_BIT;
        case RenderStyle::FIRST_LETTER:
            return FIRST_LETTER_BIT;
        case RenderStyle::SELECTION:
            return SELECTION_BIT;
        case RenderStyle::FIRST_LINE_INHERITED:
            return FIRST_LINE_INHERITED_BIT;
        case RenderStyle::FILE_UPLOAD_BUTTON:
            return FILE_UPLOAD_BUTTON_BIT;
        case RenderStyle::SLIDER_THUMB:
            return SLIDER_THUMB_BIT;
        case RenderStyle::SEARCH_CANCEL_BUTTON:
            return SEARCH_CANCEL_BUTTON_BIT;        
        case RenderStyle::SEARCH_DECORATION:
            return SEARCH_DECORATION_BIT;
        case RenderStyle::SEARCH_RESULTS_DECORATION:
            return SEARCH_RESULTS_DECORATION_BIT;
        case RenderStyle::SEARCH_RESULTS_BUTTON:
            return SEARCH_RESULTS_BUTTON_BIT;
        default:
            return NO_BIT;
    }
}

bool RenderStyle::hasPseudoStyle(PseudoId pseudo) const
{
    return pseudoBit(pseudo) & noninherited_flags._pseudoBits;
}

void RenderStyle::setHasPseudoStyle(PseudoId pseudo)
{
    noninherited_flags._pseudoBits |= pseudoBit(pseudo);
}

RenderStyle* RenderStyle::getPseudoStyle(PseudoId pid)
{
    if (!pseudoStyle || styleType() != NOPSEUDO)
        return 0;
    RenderStyle* ps = pseudoStyle;
    while (ps && ps->styleType() != pid)
        ps = ps->pseudoStyle;
    return ps;
}

void RenderStyle::addPseudoStyle(RenderStyle* pseudo)
{
    if (!pseudo)
        return;
    pseudo->ref();
    pseudo->pseudoStyle = pseudoStyle;
    pseudoStyle = pseudo;
}

bool RenderStyle::inheritedNotEqual( RenderStyle *other ) const
{
    return inherited_flags != other->inherited_flags ||
           inherited != other->inherited ||
#if SVG_SUPPORT
           m_svgStyle->inheritedNotEqual(other->m_svgStyle.get()) ||
#endif
           css3InheritedData != other->css3InheritedData;
}

inline unsigned computeFontHash(const Font& font)
{
    unsigned hashCodes[2] = {
        CaseInsensitiveHash::hash(font.fontDescription().family().family().impl()),
        font.fontDescription().specifiedSize()
    };
    return StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), 2 * sizeof(unsigned) / sizeof(UChar));
}

uint32_t RenderStyle::hashForTextAutosizing() const
{
    // Not a very smart hash.  Could be improved upon.
    uint32_t hash = 0;
    
    hash ^= css3NonInheritedData->m_appearance;
    hash ^= css3NonInheritedData->marginTopCollapse;
    hash ^= css3NonInheritedData->marginBottomCollapse;
    hash ^= css3NonInheritedData->lineClamp;
    hash ^= css3InheritedData->wordWrap;
    hash ^= css3InheritedData->nbspMode;
    hash ^= css3InheritedData->khtmlLineBreak;
    hash ^= inherited->specified_line_height.value();
    hash ^= computeFontHash(inherited->font);
    hash ^= inherited->horizontal_border_spacing;
    hash ^= inherited->vertical_border_spacing;
    hash ^= inherited_flags._box_direction;
    hash ^= inherited_flags._visuallyOrdered;
    hash ^= noninherited_flags._position;
    hash ^= noninherited_flags._floating;
    hash ^= visual->counter_increment;
    hash ^= visual->counter_reset;
    hash ^= css3NonInheritedData->textOverflow;
    hash ^= css3InheritedData->textSecurity;
    return hash;
}

bool RenderStyle::equalForTextAutosizing( const RenderStyle *other ) const
{
    if ( css3NonInheritedData->m_appearance == other->css3NonInheritedData->m_appearance &&
         css3NonInheritedData->marginTopCollapse == other->css3NonInheritedData->marginTopCollapse &&
         css3NonInheritedData->marginBottomCollapse == other->css3NonInheritedData->marginBottomCollapse &&
         (css3NonInheritedData->lineClamp == other->css3NonInheritedData->lineClamp) &&
         (css3InheritedData->textSizeAdjust == other->css3InheritedData->textSizeAdjust) &&
         (css3InheritedData->wordWrap == other->css3InheritedData->wordWrap) &&
         (css3InheritedData->nbspMode == other->css3InheritedData->nbspMode) &&
         (css3InheritedData->khtmlLineBreak == other->css3InheritedData->khtmlLineBreak) &&
         (inherited->specified_line_height == other->inherited->specified_line_height) &&
         (inherited->font.equalForTextAutoSizing(other->inherited->font)) &&
         (inherited->horizontal_border_spacing == other->inherited->horizontal_border_spacing) &&
         (inherited->vertical_border_spacing == other->inherited->vertical_border_spacing) &&
         (inherited_flags._box_direction == other->inherited_flags._box_direction) &&
         (inherited_flags._visuallyOrdered == other->inherited_flags._visuallyOrdered) &&
         (noninherited_flags._position == other->noninherited_flags._position) &&
         (noninherited_flags._floating == other->noninherited_flags._floating) &&
         visual->counter_increment == other->visual->counter_increment &&
         visual->counter_reset == other->visual->counter_reset &&
         css3NonInheritedData->textOverflow == other->css3NonInheritedData->textOverflow &&
         (css3InheritedData->textSecurity == other->css3InheritedData->textSecurity))
        return true;
    return false;
}

/*
  compares two styles. The result gives an idea of the action that
  needs to be taken when replacing the old style with a new one.

  CbLayout: The containing block of the object needs a relayout.
  Layout: the RenderObject needs a relayout after the style change
  Visible: The change is visible, but no relayout is needed
  NonVisible: The object does need neither repaint nor relayout after
       the change.

  ### TODO:
  A lot can be optimised here based on the display type, lots of
  optimisations are unimplemented, and currently result in the
  worst case result causing a relayout of the containing block.
*/
RenderStyle::Diff RenderStyle::diff( const RenderStyle *other ) const
{
#if SVG_SUPPORT
    // This is horribly inefficient.  Eventually we'll have to integrate
    // this more directly by calling: Diff svgDiff = svgStyle->diff(other)
    // and then checking svgDiff and returning from the appropriate places below.
    if (m_svgStyle != other->m_svgStyle)
        return Layout;
#endif

    // we anyway assume they are the same
//      EDisplay _effectiveDisplay : 5;

    // NonVisible:
//      ECursor _cursor_style : 4;

// ### this needs work to know more exactly if we need a relayout
//     or just a repaint

// non-inherited attributes
//     DataRef<StyleBoxData> box;
//     DataRef<StyleVisualData> visual;
//     DataRef<StyleSurroundData> surround;

// inherited attributes
//     DataRef<StyleInheritedData> inherited;

    if ( box->width != other->box->width ||
         box->min_width != other->box->min_width ||
         box->max_width != other->box->max_width ||
         box->height != other->box->height ||
         box->min_height != other->box->min_height ||
         box->max_height != other->box->max_height ||
         box->vertical_align != other->box->vertical_align ||
         box->boxSizing != other->box->boxSizing ||
         !(surround->margin == other->surround->margin) ||
         !(surround->padding == other->surround->padding) ||
         css3NonInheritedData->m_appearance != other->css3NonInheritedData->m_appearance ||
         css3NonInheritedData->marginTopCollapse != other->css3NonInheritedData->marginTopCollapse ||
         css3NonInheritedData->marginBottomCollapse != other->css3NonInheritedData->marginBottomCollapse ||
         *css3NonInheritedData->flexibleBox.get() != *other->css3NonInheritedData->flexibleBox.get() ||
         (css3NonInheritedData->lineClamp != other->css3NonInheritedData->lineClamp) ||
         (css3InheritedData->highlight != other->css3InheritedData->highlight) ||
         (css3InheritedData->textSizeAdjust != other->css3InheritedData->textSizeAdjust) ||
         (css3InheritedData->wordBreak != other->css3InheritedData->wordBreak) ||
         (css3InheritedData->wordWrap != other->css3InheritedData->wordWrap) ||
         (css3InheritedData->nbspMode != other->css3InheritedData->nbspMode) ||
         (css3InheritedData->khtmlLineBreak != other->css3InheritedData->khtmlLineBreak) ||
        !(inherited->indent == other->inherited->indent) ||
        !(inherited->line_height == other->inherited->line_height) ||
        !(inherited->specified_line_height == other->inherited->specified_line_height) ||
        !(inherited->style_image == other->inherited->style_image) ||
        !(inherited->cursor_image == other->inherited->cursor_image) ||
        !(inherited->font == other->inherited->font) ||
        !(inherited->horizontal_border_spacing == other->inherited->horizontal_border_spacing) ||
        !(inherited->vertical_border_spacing == other->inherited->vertical_border_spacing) ||
        !(inherited_flags._box_direction == other->inherited_flags._box_direction) ||
        !(inherited_flags._visuallyOrdered == other->inherited_flags._visuallyOrdered) ||
        !(inherited_flags._htmlHacks == other->inherited_flags._htmlHacks) ||
        !(noninherited_flags._position == other->noninherited_flags._position) ||
        !(noninherited_flags._floating == other->noninherited_flags._floating) ||
        !(noninherited_flags._originalDisplay == other->noninherited_flags._originalDisplay) ||
         visual->colspan != other->visual->colspan ||
         visual->counter_increment != other->visual->counter_increment ||
         visual->counter_reset != other->visual->counter_reset ||
         css3NonInheritedData->textOverflow != other->css3NonInheritedData->textOverflow ||
         (css3InheritedData->textSecurity != other->css3InheritedData->textSecurity))
        return Layout;
   
    // changes causing Layout changes:

// only for tables:
//     _border_collapse
//     EEmptyCell _empty_cells : 2 ;
//     ECaptionSide _caption_side : 2;
//     ETableLayout _table_layout : 1;
//     EPosition _position : 2;
//     EFloat _floating : 2;
    if ( ((int)noninherited_flags._effectiveDisplay) >= TABLE ) {
        // Stupid gcc gives a compile error on
        // a != other->b if a and b are bitflags. Using
        // !(a== other->b) instead.
        if ( !(inherited_flags._border_collapse == other->inherited_flags._border_collapse) ||
             !(inherited_flags._empty_cells == other->inherited_flags._empty_cells) ||
             !(inherited_flags._caption_side == other->inherited_flags._caption_side) ||
             !(noninherited_flags._table_layout == other->noninherited_flags._table_layout))
            return Layout;
        
        // In the collapsing border model, 'hidden' suppresses other borders, while 'none'
        // does not, so these style differences can be width differences.
        if (inherited_flags._border_collapse &&
            (borderTopStyle() == BHIDDEN && other->borderTopStyle() == BNONE ||
             borderTopStyle() == BNONE && other->borderTopStyle() == BHIDDEN ||
             borderBottomStyle() == BHIDDEN && other->borderBottomStyle() == BNONE ||
             borderBottomStyle() == BNONE && other->borderBottomStyle() == BHIDDEN ||
             borderLeftStyle() == BHIDDEN && other->borderLeftStyle() == BNONE ||
             borderLeftStyle() == BNONE && other->borderLeftStyle() == BHIDDEN ||
             borderRightStyle() == BHIDDEN && other->borderRightStyle() == BNONE ||
             borderRightStyle() == BNONE && other->borderRightStyle() == BHIDDEN))
            return Layout;
    }

// only for lists:
//      EListStyleType _list_style_type : 5 ;
//      EListStylePosition _list_style_position :1;
    if (noninherited_flags._effectiveDisplay == LIST_ITEM ) {
        if ( !(inherited_flags._list_style_type == other->inherited_flags._list_style_type) ||
             !(inherited_flags._list_style_position == other->inherited_flags._list_style_position) )
            return Layout;
    }

// ### These could be better optimised
//      ETextAlign _text_align : 3;
//      ETextTransform _text_transform : 4;
//      EDirection _direction : 1;
//      EWhiteSpace _white_space : 2;
//      EFontVariant _font_variant : 1;
//     EClear _clear : 2;
    if ( !(inherited_flags._text_align == other->inherited_flags._text_align) ||
         !(inherited_flags._text_transform == other->inherited_flags._text_transform) ||
         !(inherited_flags._direction == other->inherited_flags._direction) ||
         !(inherited_flags._white_space == other->inherited_flags._white_space) ||
         !(noninherited_flags._clear == other->noninherited_flags._clear) ||
         !css3InheritedData->shadowDataEquivalent(*other->css3InheritedData.get())
        )
        return Layout;

    // Overflow returns a layout hint.
    if (noninherited_flags._overflowX != other->noninherited_flags._overflowX ||
        noninherited_flags._overflowY != other->noninherited_flags._overflowY)
        return Layout;
        
// only for inline:
//     EVerticalAlign _vertical_align : 4;

    if ( !(noninherited_flags._effectiveDisplay == INLINE) &&
         !(noninherited_flags._vertical_align == other->noninherited_flags._vertical_align))
        return Layout;

    // If our border widths change, then we need to layout.  Other changes to borders
    // only necessitate a repaint.
    if (borderLeftWidth() != other->borderLeftWidth() ||
        borderTopWidth() != other->borderTopWidth() ||
        borderBottomWidth() != other->borderBottomWidth() ||
        borderRightWidth() != other->borderRightWidth())
        return Layout;

    // If regions change trigger a relayout to re-calc regions.
    if (!(css3NonInheritedData->m_dashboardRegions == other->css3NonInheritedData->m_dashboardRegions))
        return Layout;

    // Make sure these left/top/right/bottom checks stay below all layout checks and above
    // all visible checks.
    if (other->position() != StaticPosition) {
        if (!(surround->offset == other->surround->offset)) {
            // FIXME: We will need to do a bit of work in RenderObject/Box::setStyle before we
            // can stop doing a layout when relative positioned objects move.  In particular, we'll need
            // to update scrolling positions and figure out how to do a repaint properly of the updated layer.
            //if (other->position() == RelativePosition)
            //    return RepaintLayer;
            //else
                return Layout;
        }
        else if (box->z_index != other->box->z_index || box->z_auto != other->box->z_auto ||
                 !(visual->clip == other->visual->clip) || visual->hasClip != other->visual->hasClip)
            return RepaintLayer;
    }

    if (css3NonInheritedData->opacity != other->css3NonInheritedData->opacity)
        return RepaintLayer;

    // Repaint:
//      EVisibility _visibility : 2;
//      int _text_decoration : 4;
//     DataRef<StyleBackgroundData> background;
    if (inherited->color != other->inherited->color ||
        inherited_flags._visibility != other->inherited_flags._visibility ||
        !(inherited_flags._text_decorations == other->inherited_flags._text_decorations) ||
        !(inherited_flags._force_backgrounds_to_white == other->inherited_flags._force_backgrounds_to_white) ||
        !(surround->border == other->surround->border) ||
        *background.get() != *other->background.get() ||
        visual->textDecoration != other->visual->textDecoration ||
        css3InheritedData->userModify != other->css3InheritedData->userModify ||
        css3NonInheritedData->userSelect != other->css3NonInheritedData->userSelect ||
        css3NonInheritedData->userDrag != other->css3NonInheritedData->userDrag
        )
        return Repaint;

    return Equal;
}

void RenderStyle::adjustBackgroundLayers()
{
    if (backgroundLayers()->next()) {
        // First we cull out layers that have no properties set.
        accessBackgroundLayers()->cullEmptyLayers();
        
        // Next we repeat patterns into layers that don't have some properties set.
        accessBackgroundLayers()->fillUnsetProperties();
    }
}

void RenderStyle::setClip( Length top, Length right, Length bottom, Length left )
{
    StyleVisualData *data = visual.access();
    data->clip.top = top;
    data->clip.right = right;
    data->clip.bottom = bottom;
    data->clip.left = left;
}

bool RenderStyle::contentDataEquivalent(const RenderStyle* otherStyle) const
{
    ContentData* c1 = content;
    ContentData* c2 = otherStyle->content;

    while (c1 && c2) {
        if (c1->_contentType != c2->_contentType)
            return false;
        if (c1->_contentType == CONTENT_TEXT) {
            String c1Str(c1->_content.text);
            String c2Str(c2->_content.text);
            if (c1Str != c2Str)
                return false;
        }
        else if (c1->_contentType == CONTENT_OBJECT) {
            if (c1->_content.object != c2->_content.object)
                return false;
        }

        c1 = c1->_nextContent;
        c2 = c2->_nextContent;
    }

    return !c1 && !c2;
}

void RenderStyle::setContent(CachedResource* o, bool add)
{
    if (!o)
        return; // The object is null. Nothing to do. Just bail.

    ContentData* lastContent = content;
    while (lastContent && lastContent->_nextContent)
        lastContent = lastContent->_nextContent;

    bool reuseContent = !add;
    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clearContent();
        newContentData = content;
    }
    else
        newContentData = new ContentData;

    if (lastContent && !reuseContent)
        lastContent->_nextContent = newContentData;
    else
        content = newContentData;

    newContentData->_content.object = o;
    newContentData->_contentType = CONTENT_OBJECT;
}

void RenderStyle::setContent(StringImpl* s, bool add)
{
    if (!s)
        return; // The string is null. Nothing to do. Just bail.
    
    ContentData* lastContent = content;
    while (lastContent && lastContent->_nextContent)
        lastContent = lastContent->_nextContent;

    bool reuseContent = !add;
    if (add && lastContent) {
        if (lastContent->_contentType == CONTENT_TEXT) {
            // We can augment the existing string and share this ContentData node.
            StringImpl* oldStr = lastContent->_content.text;
            StringImpl* newStr = oldStr->copy();
            newStr->ref();
            oldStr->deref();
            newStr->append(s);
            lastContent->_content.text = newStr;
            return;
        }
    }

    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clearContent();
        newContentData = content;
    }
    else
        newContentData = new ContentData;
    
    if (lastContent && !reuseContent)
        lastContent->_nextContent = newContentData;
    else
        content = newContentData;
    
    newContentData->_content.text = s;
    newContentData->_content.text->ref();
    newContentData->_contentType = CONTENT_TEXT;
}

ContentData::~ContentData()
{
    clearContent();
}

ContentData::ContentData(ContentData *contentData)
{
    _contentType = contentData->_contentType;
    _content = contentData->_content;
    if (_contentType == CONTENT_TEXT)
        _content.text->ref();
    _nextContent = contentData->_nextContent;
}

void ContentData::clearContent()
{
    delete _nextContent;
    _nextContent = 0;
    
    switch (_contentType)
    {
        case CONTENT_OBJECT:
            _content.object = 0;
            break;
        case CONTENT_TEXT:
            _content.text->deref();
            _content.text = 0;
        default:
            ;
    }
}

#ifndef KHTML_NO_XBL
BindingURI::BindingURI(StringImpl* uri) 
:m_next(0)
{ 
    m_uri = uri;
    if (uri) uri->ref();
}

BindingURI::~BindingURI()
{
    if (m_uri)
        m_uri->deref();
    delete m_next;
}

BindingURI* BindingURI::copy()
{
    BindingURI* newBinding = new BindingURI(m_uri);
    if (next()) {
        BindingURI* nextCopy = next()->copy();
        newBinding->setNext(nextCopy);
    }
    
    return newBinding;
}

bool BindingURI::operator==(const BindingURI& o) const
{
    if ((m_next && !o.m_next) || (!m_next && o.m_next) ||
        (m_next && o.m_next && *m_next != *o.m_next))
        return false;
    
    if (m_uri == o.m_uri)
        return true;
    if (!m_uri || !o.m_uri)
        return false;
    
    return String(m_uri) == String(o.m_uri);
}

void RenderStyle::addBindingURI(StringImpl* uri)
{
    BindingURI* binding = new BindingURI(uri);
    if (!bindingURIs())
        SET_VAR(css3NonInheritedData, bindingURI, binding)
    else 
        for (BindingURI* b = bindingURIs(); b; b = b->next()) {
            if (!b->next())
                b->setNext(binding);
        }
}
#endif

void RenderStyle::setTextShadow(ShadowData* val, bool add)
{
    StyleCSS3InheritedData* css3Data = css3InheritedData.access(); 
    if (!add) {
        delete css3Data->textShadow;
        css3Data->textShadow = val;
        return;
    }

    ShadowData* last = css3Data->textShadow;
    while (last->next) last = last->next;
    last->next = val;
}

ShadowData::ShadowData(const ShadowData& o)
:x(o.x), y(o.y), blur(o.blur), color(o.color)
{
    next = o.next ? new ShadowData(*o.next) : 0;
}

bool ShadowData::operator==(const ShadowData& o) const
{
    if ((next && !o.next) || (!next && o.next) ||
        (next && o.next && *next != *o.next))
        return false;
    
    return x == o.x && y == o.y && blur == o.blur && color == o.color;
}

const DeprecatedValueList<StyleDashboardRegion>& RenderStyle::initialDashboardRegions()
{ 
    static DeprecatedValueList<StyleDashboardRegion> emptyList;
    return emptyList;
}

const DeprecatedValueList<StyleDashboardRegion>& RenderStyle::noneDashboardRegions()
{ 
    static DeprecatedValueList<StyleDashboardRegion> noneList;
    static bool noneListInitialized = false;
    
    if (!noneListInitialized) {
        StyleDashboardRegion region;
        region.label = "";
        region.offset.top  = Length();
        region.offset.right = Length();
        region.offset.bottom = Length();
        region.offset.left = Length();
        region.type = StyleDashboardRegion::None;
        noneList.append (region);
        noneListInitialized = true;
    }
    return noneList;
}

}
