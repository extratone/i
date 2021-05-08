/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef RenderStyleConstants_h
#define RenderStyleConstants_h

namespace WebCore {

/*
 * WARNING:
 * --------
 *
 * The order of the values in the enums have to agree with the order specified
 * in CSSValueKeywords.in, otherwise some optimizations in the parser will fail,
 * and produce invalid results.
 */

// The difference between two styles.  The following values are used:
// (1) StyleDifferenceEqual - The two styles are identical
// (2) StyleDifferenceRecompositeLayer - The layer needs its position and transform updated, but no repaint
// (3) StyleDifferenceRepaint - The object just needs to be repainted.
// (4) StyleDifferenceRepaintLayer - The layer and its descendant layers needs to be repainted.
// (5) StyleDifferenceLayout - A layout is required.
enum StyleDifference {
    StyleDifferenceEqual,
#if USE(ACCELERATED_COMPOSITING)
    StyleDifferenceRecompositeLayer,
#endif
    StyleDifferenceRepaint,
    StyleDifferenceRepaintLayer,
    StyleDifferenceLayoutPositionedMovementOnly,
    StyleDifferenceLayout
};

// When some style properties change, different amounts of work have to be done depending on
// context (e.g. whether the property is changing on an element which has a compositing layer).
// A simple StyleDifference does not provide enough information so we return a bit mask of
// StyleDifferenceContextSensitiveProperties from RenderStyle::diff() too.
enum StyleDifferenceContextSensitiveProperty {
    ContextSensitivePropertyNone = 0,
    ContextSensitivePropertyTransform = (1 << 0),
    ContextSensitivePropertyOpacity = (1 << 1)
};

// Static pseudo styles. Dynamic ones are produced on the fly.
enum PseudoId {
    NOPSEUDO, FIRST_LINE, FIRST_LETTER, BEFORE, AFTER, SELECTION, FIRST_LINE_INHERITED, SCROLLBAR, FILE_UPLOAD_BUTTON, INPUT_PLACEHOLDER,
    SLIDER_THUMB, SEARCH_CANCEL_BUTTON, SEARCH_DECORATION, SEARCH_RESULTS_DECORATION, SEARCH_RESULTS_BUTTON, MEDIA_CONTROLS_PANEL,
    MEDIA_CONTROLS_PLAY_BUTTON, MEDIA_CONTROLS_MUTE_BUTTON, MEDIA_CONTROLS_TIMELINE, MEDIA_CONTROLS_TIMELINE_CONTAINER,
    MEDIA_CONTROLS_CURRENT_TIME_DISPLAY, MEDIA_CONTROLS_TIME_REMAINING_DISPLAY, MEDIA_CONTROLS_SEEK_BACK_BUTTON, 
    MEDIA_CONTROLS_SEEK_FORWARD_BUTTON, MEDIA_CONTROLS_FULLSCREEN_BUTTON, 
    SCROLLBAR_THUMB, SCROLLBAR_BUTTON, SCROLLBAR_TRACK, SCROLLBAR_TRACK_PIECE, SCROLLBAR_CORNER, RESIZER,

    FIRST_INTERNAL_PSEUDOID = FILE_UPLOAD_BUTTON
};

// These have been defined in the order of their precedence for border-collapsing. Do
// not change this order!
enum EBorderStyle { BNONE, BHIDDEN, INSET, GROOVE, RIDGE, OUTSET, DOTTED, DASHED, SOLID, DOUBLE };

enum EBorderPrecedence { BOFF, BTABLE, BCOLGROUP, BCOL, BROWGROUP, BROW, BCELL };

enum PseudoState { PseudoUnknown, PseudoNone, PseudoAnyLink, PseudoLink, PseudoVisited};

enum EPosition {
    StaticPosition, RelativePosition, AbsolutePosition, FixedPosition
};

enum EFloat {
    FNONE = 0, FLEFT, FRIGHT
};


enum EMarginCollapse { MCOLLAPSE, MSEPARATE, MDISCARD };

// Box attributes. Not inherited.

enum EBoxSizing { CONTENT_BOX, BORDER_BOX };

// Random visual rendering model attributes. Not inherited.

enum EOverflow {
    OVISIBLE, OHIDDEN, OSCROLL, OAUTO, OOVERLAY, OMARQUEE
};

enum EVerticalAlign {
    BASELINE, MIDDLE, SUB, SUPER, TEXT_TOP,
    TEXT_BOTTOM, TOP, BOTTOM, BASELINE_MIDDLE, LENGTH
};

enum EClear{
    CNONE = 0, CLEFT = 1, CRIGHT = 2, CBOTH = 3
};

enum ETableLayout {
    TAUTO, TFIXED
};

enum EUnicodeBidi {
    UBNormal, Embed, Override
};

enum EFillBox {
    BorderFillBox, PaddingFillBox, ContentFillBox, TextFillBox
};

enum EFillRepeat {
    RepeatFill, RepeatXFill, RepeatYFill, NoRepeatFill
};

enum EFillLayerType {
    BackgroundFillLayer, MaskFillLayer
};

// CSS3 Marquee Properties

enum EMarqueeBehavior { MNONE, MSCROLL, MSLIDE, MALTERNATE };
enum EMarqueeDirection { MAUTO = 0, MLEFT = 1, MRIGHT = -1, MUP = 2, MDOWN = -2, MFORWARD = 3, MBACKWARD = -3 };

// CSS3 Flexible Box Properties

enum EBoxAlignment { BSTRETCH, BSTART, BCENTER, BEND, BJUSTIFY, BBASELINE };
enum EBoxOrient { HORIZONTAL, VERTICAL };
enum EBoxLines { SINGLE, MULTIPLE };
enum EBoxDirection { BNORMAL, BREVERSE };

enum ETextSecurity {
    TSNONE, TSDISC, TSCIRCLE, TSSQUARE
};

// CSS3 User Modify Properties

enum EUserModify {
    READ_ONLY, READ_WRITE, READ_WRITE_PLAINTEXT_ONLY
};

// CSS3 User Drag Values

enum EUserDrag {
    DRAG_AUTO, DRAG_NONE, DRAG_ELEMENT
};

// CSS3 User Select Values

enum EUserSelect {
    SELECT_NONE, SELECT_TEXT
};

// Word Break Values. Matches WinIE, rather than CSS3

enum EWordBreak {
    NormalWordBreak, BreakAllWordBreak, BreakWordBreak
};

enum EWordWrap {
    NormalWordWrap, BreakWordWrap
};

enum ENBSPMode {
    NBNORMAL, SPACE
};

enum EKHTMLLineBreak {
    LBNORMAL, AFTER_WHITE_SPACE
};

enum EMatchNearestMailBlockquoteColor {
    BCNORMAL, MATCH
};

enum EResize {
    RESIZE_NONE, RESIZE_BOTH, RESIZE_HORIZONTAL, RESIZE_VERTICAL
};

enum EListStyleType {
     DISC, CIRCLE, SQUARE, LDECIMAL, DECIMAL_LEADING_ZERO,
     LOWER_ROMAN, UPPER_ROMAN, LOWER_GREEK,
     LOWER_ALPHA, LOWER_LATIN, UPPER_ALPHA, UPPER_LATIN,
     HEBREW, ARMENIAN, GEORGIAN, CJK_IDEOGRAPHIC,
     HIRAGANA, KATAKANA, HIRAGANA_IROHA, KATAKANA_IROHA, LNONE
};

enum StyleContentType {
    CONTENT_NONE, CONTENT_OBJECT, CONTENT_TEXT, CONTENT_COUNTER
};

enum EBorderFit { BorderFitBorder, BorderFitLines };

enum ETimingFunctionType { LinearTimingFunction, CubicBezierTimingFunction };

enum EAnimPlayState {
    AnimPlayStatePlaying = 0x0,
    AnimPlayStatePaused = 0x1
};

enum EWhiteSpace {
    NORMAL, PRE, PRE_WRAP, PRE_LINE, NOWRAP, KHTML_NOWRAP
};

enum ETextAlign {
    TAAUTO, LEFT, RIGHT, CENTER, JUSTIFY, WEBKIT_LEFT, WEBKIT_RIGHT, WEBKIT_CENTER
};

enum ETextTransform {
    CAPITALIZE, UPPERCASE, LOWERCASE, TTNONE
};

enum ETextDecoration {
    TDNONE = 0x0 , UNDERLINE = 0x1, OVERLINE = 0x2, LINE_THROUGH= 0x4, BLINK = 0x8
};

enum EPageBreak {
    PBAUTO, PBALWAYS, PBAVOID
};

enum EEmptyCell {
    SHOW, HIDE
};

enum ECaptionSide {
    CAPTOP, CAPBOTTOM, CAPLEFT, CAPRIGHT
};

enum EListStylePosition { OUTSIDE, INSIDE };

enum EVisibility { VISIBLE, HIDDEN, COLLAPSE };

enum ECursor {
    // The following must match the order in CSSValueKeywords.in.
    CURSOR_AUTO,
    CURSOR_CROSS,
    CURSOR_DEFAULT,
    CURSOR_POINTER,
    CURSOR_MOVE,
    CURSOR_VERTICAL_TEXT,
    CURSOR_CELL,
    CURSOR_CONTEXT_MENU,
    CURSOR_ALIAS,
    CURSOR_PROGRESS,
    CURSOR_NO_DROP,
    CURSOR_NOT_ALLOWED,
    CURSOR_WEBKIT_ZOOM_IN,
    CURSOR_WEBKIT_ZOOM_OUT,
    CURSOR_E_RESIZE,
    CURSOR_NE_RESIZE,
    CURSOR_NW_RESIZE,
    CURSOR_N_RESIZE,
    CURSOR_SE_RESIZE,
    CURSOR_SW_RESIZE,
    CURSOR_S_RESIZE,
    CURSOR_W_RESIZE,
    CURSOR_EW_RESIZE,
    CURSOR_NS_RESIZE,
    CURSOR_NESW_RESIZE,
    CURSOR_NWSE_RESIZE,
    CURSOR_COL_RESIZE,
    CURSOR_ROW_RESIZE,
    CURSOR_TEXT,
    CURSOR_WAIT,
    CURSOR_HELP,
    CURSOR_ALL_SCROLL,
    CURSOR_WEBKIT_GRAB,
    CURSOR_WEBKIT_GRABBING,

    // The following are handled as exceptions so don't need to match.
    CURSOR_COPY,
    CURSOR_NONE
};

enum EDisplay {
    INLINE, BLOCK, LIST_ITEM, RUN_IN, COMPACT, INLINE_BLOCK,
    TABLE, INLINE_TABLE, TABLE_ROW_GROUP,
    TABLE_HEADER_GROUP, TABLE_FOOTER_GROUP, TABLE_ROW,
    TABLE_COLUMN_GROUP, TABLE_COLUMN, TABLE_CELL,
    TABLE_CAPTION, BOX, INLINE_BOX, NONE
};

enum EPointerEvents {
    PE_NONE, PE_AUTO, PE_STROKE, PE_FILL, PE_PAINTED, PE_VISIBLE,
    PE_VISIBLE_STROKE, PE_VISIBLE_FILL, PE_VISIBLE_PAINTED, PE_ALL
};

enum ETransformStyle3D {
    TransformStyle3DFlat, TransformStyle3DPreserve3D
};

enum EBackfaceVisibility {
    BackfaceVisibilityVisible, BackfaceVisibilityHidden
};

enum EImageLoadingBorder {
    IMAGE_LOADING_BORDER_OUTLINE, IMAGE_LOADING_BORDER_NONE
};

} // namespace WebCore

#endif // RenderStyleConstants_h
