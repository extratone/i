/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "RenderBoxModelObject.h"

#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderView.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

bool RenderBoxModelObject::s_wasFloating = false;

RenderBoxModelObject::RenderBoxModelObject(Node* node)
    : RenderObject(node)
    , m_layer(0)
{
}

RenderBoxModelObject::~RenderBoxModelObject()
{
}

void RenderBoxModelObject::destroy()
{
    // This must be done before we destroy the RenderObject.
    if (m_layer)
        m_layer->clearClipRects();
    RenderObject::destroy();
}

bool RenderBoxModelObject::hasSelfPaintingLayer() const
{
    return m_layer && m_layer->isSelfPaintingLayer();
}

void RenderBoxModelObject::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    s_wasFloating = isFloating();
    RenderObject::styleWillChange(diff, newStyle);
}

void RenderBoxModelObject::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderObject::styleDidChange(diff, oldStyle);
    updateBoxModelInfoFromStyle();
    
    if (requiresLayer()) {
        if (!layer()) {
            if (s_wasFloating && isFloating())
                setChildNeedsLayout(true);
            m_layer = new (renderArena()) RenderLayer(this);
            setHasLayer(true);
            m_layer->insertOnlyThisLayer();
            if (parent() && !needsLayout() && containingBlock())
                m_layer->updateLayerPositions();
        }
    } else if (layer() && layer()->parent()) {
        
        RenderLayer* layer = m_layer;
        m_layer = 0;
        setHasLayer(false);
        setHasTransform(false); // Either a transform wasn't specified or the object doesn't support transforms, so just null out the bit.
        setHasReflection(false);
        layer->removeOnlyThisLayer();
        if (s_wasFloating && isFloating())
            setChildNeedsLayout(true);
    }

    if (m_layer)
        m_layer->styleChanged(diff, oldStyle);
}

void RenderBoxModelObject::updateBoxModelInfoFromStyle()
{
    // Set the appropriate bits for a box model object.  Since all bits are cleared in styleWillChange,
    // we only check for bits that could possibly be set to true.
    setHasBoxDecorations(style()->hasBorder() || style()->hasBackground() || style()->hasAppearance() || style()->boxShadow());
    setInline(style()->isDisplayInlineType());
    setRelPositioned(style()->position() == RelativePosition);
}

int RenderBoxModelObject::relativePositionOffsetX() const
{
    if (!style()->left().isAuto()) {
        if (!style()->right().isAuto() && containingBlock()->style()->direction() == RTL)
            return -style()->right().calcValue(containingBlockWidthForContent());
        return style()->left().calcValue(containingBlockWidthForContent());
    }
    if (!style()->right().isAuto())
        return -style()->right().calcValue(containingBlockWidthForContent());
    return 0;
}

int RenderBoxModelObject::relativePositionOffsetY() const
{
    if (!style()->top().isAuto()) {
        if (!style()->top().isPercent() || containingBlock()->style()->height().isFixed())
            return style()->top().calcValue(containingBlock()->availableHeight());
    } else if (!style()->bottom().isAuto()) {
        if (!style()->bottom().isPercent() || containingBlock()->style()->height().isFixed())
            return -style()->bottom().calcValue(containingBlock()->availableHeight());
    }
    return 0;
}

int RenderBoxModelObject::offsetLeft() const
{
    RenderBoxModelObject* offsetPar = offsetParent();
    if (!offsetPar)
        return 0;
    int xPos = (isBox() ? toRenderBox(this)->x() : 0);
    if (offsetPar->isBox())
        xPos -= toRenderBox(offsetPar)->borderLeft();
    if (!isPositioned()) {
        if (isRelPositioned())
            xPos += relativePositionOffsetX();
        RenderObject* curr = parent();
        while (curr && curr != offsetPar) {
            // FIXME: What are we supposed to do inside SVG content?
            if (curr->isBox() && !curr->isTableRow())
                xPos += toRenderBox(curr)->x();
            curr = curr->parent();
        }
        if (offsetPar->isBox() && offsetPar->isBody() && !offsetPar->isRelPositioned() && !offsetPar->isPositioned())
            xPos += toRenderBox(offsetPar)->x();
    }
    return xPos;
}

int RenderBoxModelObject::offsetTop() const
{
    RenderBoxModelObject* offsetPar = offsetParent();
    if (!offsetPar)
        return 0;
    int yPos = (isBox() ? toRenderBox(this)->y() : 0);
    if (offsetPar->isBox())
        yPos -= toRenderBox(offsetPar)->borderTop();
    if (!isPositioned()) {
        if (isRelPositioned())
            yPos += relativePositionOffsetY();
        RenderObject* curr = parent();
        while (curr && curr != offsetPar) {
            // FIXME: What are we supposed to do inside SVG content?
            if (curr->isBox() && !curr->isTableRow())
                yPos += toRenderBox(curr)->y();
            curr = curr->parent();
        }
        if (offsetPar->isBox() && offsetPar->isBody() && !offsetPar->isRelPositioned() && !offsetPar->isPositioned())
            yPos += toRenderBox(offsetPar)->y();
    }
    return yPos;
}

int RenderBoxModelObject::paddingTop(bool) const
{
    int w = 0;
    Length padding = style()->paddingTop();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderBoxModelObject::paddingBottom(bool) const
{
    int w = 0;
    Length padding = style()->paddingBottom();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderBoxModelObject::paddingLeft(bool) const
{
    int w = 0;
    Length padding = style()->paddingLeft();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderBoxModelObject::paddingRight(bool) const
{
    int w = 0;
    Length padding = style()->paddingRight();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}


void RenderBoxModelObject::paintFillLayerExtended(const PaintInfo& paintInfo, const Color& c, const FillLayer* bgLayer, int clipY, int clipH,
                                                  int tx, int ty, int w, int h, InlineFlowBox* box, CompositeOperator op)
{
    GraphicsContext* context = paintInfo.context;
    bool includeLeftEdge = box ? box->includeLeftEdge() : true;
    bool includeRightEdge = box ? box->includeRightEdge() : true;
    int bLeft = includeLeftEdge ? borderLeft() : 0;
    int bRight = includeRightEdge ? borderRight() : 0;
    int pLeft = includeLeftEdge ? paddingLeft() : 0;
    int pRight = includeRightEdge ? paddingRight() : 0;

    bool clippedToBorderRadius = false;
    if (style()->hasBorderRadius() && (includeLeftEdge || includeRightEdge)) {
        context->save();
        context->addRoundedRectClip(IntRect(tx, ty, w, h),
            includeLeftEdge ? style()->borderTopLeftRadius() : IntSize(),
            includeRightEdge ? style()->borderTopRightRadius() : IntSize(),
            includeLeftEdge ? style()->borderBottomLeftRadius() : IntSize(),
            includeRightEdge ? style()->borderBottomRightRadius() : IntSize());
        clippedToBorderRadius = true;
    }

    if (bgLayer->clip() == PaddingFillBox || bgLayer->clip() == ContentFillBox) {
        // Clip to the padding or content boxes as necessary.
        bool includePadding = bgLayer->clip() == ContentFillBox;
        int x = tx + bLeft + (includePadding ? pLeft : 0);
        int y = ty + borderTop() + (includePadding ? paddingTop() : 0);
        int width = w - bLeft - bRight - (includePadding ? pLeft + pRight : 0);
        int height = h - borderTop() - borderBottom() - (includePadding ? paddingTop() + paddingBottom() : 0);
        context->save();
        context->clip(IntRect(x, y, width, height));
    } else if (bgLayer->clip() == TextFillBox) {
        // We have to draw our text into a mask that can then be used to clip background drawing.
        // First figure out how big the mask has to be.  It should be no bigger than what we need
        // to actually render, so we should intersect the dirty rect with the border box of the background.
        IntRect maskRect(tx, ty, w, h);
        maskRect.intersect(paintInfo.rect);
        
        // Now create the mask.
        auto_ptr<ImageBuffer> maskImage = ImageBuffer::create(maskRect.size(), false);
        if (!maskImage.get())
            return;
        
        GraphicsContext* maskImageContext = maskImage->context();
        maskImageContext->translate(-maskRect.x(), -maskRect.y());
        
        // Now add the text to the clip.  We do this by painting using a special paint phase that signals to
        // InlineTextBoxes that they should just add their contents to the clip.
        PaintInfo info(maskImageContext, maskRect, PaintPhaseTextClip, true, 0, 0);
        if (box)
            box->paint(info, tx - box->x(), ty - box->y());
        else
            paint(info, tx, ty);
            
        // The mask has been created.  Now we just need to clip to it.
        context->save();
        context->clipToImageBuffer(maskRect, maskImage.get());
    }
    
    StyleImage* bg = bgLayer->image();
    bool shouldPaintBackgroundImage = bg && bg->canRender(style()->effectiveZoom());
    Color bgColor = c;

    // When this style flag is set, change existing background colors and images to a solid white background.
    // If there's no bg color or image, leave it untouched to avoid affecting transparency.
    // We don't try to avoid loading the background images, because this style flag is only set
    // when printing, and at that point we've already loaded the background images anyway. (To avoid
    // loading the background images we'd have to do this check when applying styles rather than
    // while rendering.)
    if (style()->forceBackgroundsToWhite()) {
        // Note that we can't reuse this variable below because the bgColor might be changed
        bool shouldPaintBackgroundColor = !bgLayer->next() && bgColor.isValid() && bgColor.alpha() > 0;
        if (shouldPaintBackgroundImage || shouldPaintBackgroundColor) {
            bgColor = Color::white;
            shouldPaintBackgroundImage = false;
        }
    }

    // Only fill with a base color (e.g., white) if we're the root document, since iframes/frames with
    // no background in the child document should show the parent's background.
    bool isTransparent = false;
    if (!bgLayer->next() && isRoot() && !(bgColor.isValid() && bgColor.alpha() > 0) && view()->frameView()) {
        Node* elt = document()->ownerElement();
        if (elt) {
            if (!elt->hasTagName(frameTag)) {
                // Locate the <body> element using the DOM.  This is easier than trying
                // to crawl around a render tree with potential :before/:after content and
                // anonymous blocks created by inline <body> tags etc.  We can locate the <body>
                // render object very easily via the DOM.
                HTMLElement* body = document()->body();
                isTransparent = !body || !body->hasLocalName(framesetTag); // Can't scroll a frameset document anyway.
            }
        } else
            isTransparent = view()->frameView()->isTransparent();

        // FIXME: This needs to be dynamic.  We should be able to go back to blitting if we ever stop being transparent.
        if (isTransparent)
            view()->frameView()->setUseSlowRepaints(); // The parent must show behind the child.
    }

    // Paint the color first underneath all images.
    if (!bgLayer->next()) {
        IntRect rect(tx, clipY, w, clipH);
        // If we have an alpha and we are painting the root element, go ahead and blend with the base background color.
        if (isRoot() && (!bgColor.isValid() || bgColor.alpha() < 0xFF) && !isTransparent) {
            Color baseColor = view()->frameView()->baseBackgroundColor();
            if (baseColor.alpha() > 0) {
                context->save();
                context->setCompositeOperation(CompositeCopy);
                context->fillRect(rect, baseColor);
                context->restore();
            } else
                context->clearRect(rect);
        }

        if (bgColor.isValid() && bgColor.alpha() > 0)
            context->fillRect(rect, bgColor);
    }

    // no progressive loading of the background image
    if (shouldPaintBackgroundImage) {
        IntRect destRect;
        IntPoint phase;
        IntSize tileSize;

        calculateBackgroundImageGeometry(bgLayer, tx, ty, w, h, destRect, phase, tileSize);
        if (!destRect.isEmpty()) {
            CompositeOperator compositeOp = op == CompositeSourceOver ? bgLayer->composite() : op;
            context->drawTiledImage(bg->image(this, tileSize), destRect, phase, tileSize, compositeOp);
        }
    }

    if (bgLayer->clip() != BorderFillBox)
        // Undo the background clip
        context->restore();

    if (clippedToBorderRadius)
        // Undo the border radius clip
        context->restore();
}

IntSize RenderBoxModelObject::calculateBackgroundSize(const FillLayer* bgLayer, int scaledWidth, int scaledHeight) const
{
    StyleImage* bg = bgLayer->image();
    bg->setImageContainerSize(IntSize(scaledWidth, scaledHeight)); // Use the box established by background-origin.

    if (bgLayer->isSizeSet()) {
        int w = scaledWidth;
        int h = scaledHeight;
        Length bgWidth = bgLayer->size().width();
        Length bgHeight = bgLayer->size().height();

        if (bgWidth.isFixed())
            w = bgWidth.value();
        else if (bgWidth.isPercent())
            w = bgWidth.calcValue(scaledWidth);
        
        if (bgHeight.isFixed())
            h = bgHeight.value();
        else if (bgHeight.isPercent())
            h = bgHeight.calcValue(scaledHeight);
        
        // If one of the values is auto we have to use the appropriate
        // scale to maintain our aspect ratio.
        if (bgWidth.isAuto() && !bgHeight.isAuto())
            w = bg->imageSize(this, style()->effectiveZoom()).width() * h / bg->imageSize(this, style()->effectiveZoom()).height();        
        else if (!bgWidth.isAuto() && bgHeight.isAuto())
            h = bg->imageSize(this, style()->effectiveZoom()).height() * w / bg->imageSize(this, style()->effectiveZoom()).width();
        else if (bgWidth.isAuto() && bgHeight.isAuto()) {
            // If both width and height are auto, we just want to use the image's
            // intrinsic size.
            w = bg->imageSize(this, style()->effectiveZoom()).width();
            h = bg->imageSize(this, style()->effectiveZoom()).height();
        }
        
        return IntSize(max(1, w), max(1, h));
    } else
        return bg->imageSize(this, style()->effectiveZoom());
}

void RenderBoxModelObject::calculateBackgroundImageGeometry(const FillLayer* bgLayer, int tx, int ty, int w, int h, 
                                                            IntRect& destRect, IntPoint& phase, IntSize& tileSize)
{
    int pw;
    int ph;
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
    int cx;
    int cy;
    int rw = 0;
    int rh = 0;

    // CSS2 chapter 14.2.1

    if (bgLayer->attachment()) {
        // Scroll
        if (bgLayer->origin() != BorderFillBox) {
            left = borderLeft();
            right = borderRight();
            top = borderTop();
            bottom = borderBottom();
            if (bgLayer->origin() == ContentFillBox) {
                left += paddingLeft();
                right += paddingRight();
                top += paddingTop();
                bottom += paddingBottom();
            }
        }
        
        // The background of the box generated by the root element covers the entire canvas including
        // its margins.  Since those were added in already, we have to factor them out when computing the
        // box used by background-origin/size/position.
        if (isRoot()) {
            rw = toRenderBox(this)->width() - left - right;
            rh = toRenderBox(this)->height() - top - bottom; 
            left += marginLeft();
            right += marginRight();
            top += marginTop();
            bottom += marginBottom();
        }
        cx = tx;
        cy = ty;
        pw = w - left - right;
        ph = h - top - bottom;
    } else {
        // Fixed
        IntRect vr = viewRect();
        cx = vr.x();
        cy = vr.y();
        pw = vr.width();
        ph = vr.height();
    }

    int sx = 0;
    int sy = 0;
    int cw;
    int ch;

    IntSize scaledImageSize;
    if (isRoot() && bgLayer->attachment())
        scaledImageSize = calculateBackgroundSize(bgLayer, rw, rh);
    else
        scaledImageSize = calculateBackgroundSize(bgLayer, pw, ph);
        
    int scaledImageWidth = scaledImageSize.width();
    int scaledImageHeight = scaledImageSize.height();

    EFillRepeat backgroundRepeat = bgLayer->repeat();
    
    int xPosition;
    if (isRoot() && bgLayer->attachment())
        xPosition = bgLayer->xPosition().calcMinValue(rw - scaledImageWidth, true);
    else
        xPosition = bgLayer->xPosition().calcMinValue(pw - scaledImageWidth, true);
    if (backgroundRepeat == RepeatFill || backgroundRepeat == RepeatXFill) {
        cw = pw + left + right;
        sx = scaledImageWidth ? scaledImageWidth - (xPosition + left) % scaledImageWidth : 0;
    } else {
        cx += max(xPosition + left, 0);
        sx = -min(xPosition + left, 0);
        cw = scaledImageWidth + min(xPosition + left, 0);
    }
    
    int yPosition;
    if (isRoot() && bgLayer->attachment())
        yPosition = bgLayer->yPosition().calcMinValue(rh - scaledImageHeight, true);
    else 
        yPosition = bgLayer->yPosition().calcMinValue(ph - scaledImageHeight, true);
    if (backgroundRepeat == RepeatFill || backgroundRepeat == RepeatYFill) {
        ch = ph + top + bottom;
        sy = scaledImageHeight ? scaledImageHeight - (yPosition + top) % scaledImageHeight : 0;
    } else {
        cy += max(yPosition + top, 0);
        sy = -min(yPosition + top, 0);
        ch = scaledImageHeight + min(yPosition + top, 0);
    }

    if (!bgLayer->attachment()) {
        sx += max(tx - cx, 0);
        sy += max(ty - cy, 0);
    }

    destRect = IntRect(cx, cy, cw, ch);
    destRect.intersect(IntRect(tx, ty, w, h));
    phase = IntPoint(sx, sy);
    tileSize = IntSize(scaledImageWidth, scaledImageHeight);
}

int RenderBoxModelObject::verticalPosition(bool firstLine) const
{
    // This method determines the vertical position for inline elements.
    ASSERT(isInline());
    if (!isInline())
        return 0;

    int vpos = 0;
    EVerticalAlign va = style()->verticalAlign();
    if (va == TOP)
        vpos = PositionTop;
    else if (va == BOTTOM)
        vpos = PositionBottom;
    else {
        bool checkParent = parent()->isRenderInline() && parent()->style()->verticalAlign() != TOP && parent()->style()->verticalAlign() != BOTTOM;
        vpos = checkParent ? toRenderInline(parent())->verticalPositionFromCache(firstLine) : 0;
        // don't allow elements nested inside text-top to have a different valignment.
        if (va == BASELINE)
            return vpos;

        const Font& f = parent()->style(firstLine)->font();
        int fontsize = f.pixelSize();

        if (va == SUB)
            vpos += fontsize / 5 + 1;
        else if (va == SUPER)
            vpos -= fontsize / 3 + 1;
        else if (va == TEXT_TOP)
            vpos += baselinePosition(firstLine) - f.ascent();
        else if (va == MIDDLE)
            vpos += -static_cast<int>(f.xHeight() / 2) - lineHeight(firstLine) / 2 + baselinePosition(firstLine);
        else if (va == TEXT_BOTTOM) {
            vpos += f.descent();
            if (!isReplaced())
                vpos -= style(firstLine)->font().descent();
        } else if (va == BASELINE_MIDDLE)
            vpos += -lineHeight(firstLine) / 2 + baselinePosition(firstLine);
        else if (va == LENGTH)
            vpos -= style()->verticalAlignLength().calcValue(lineHeight(firstLine));
    }

    return vpos;
}

bool RenderBoxModelObject::paintNinePieceImage(GraphicsContext* graphicsContext, int tx, int ty, int w, int h, const RenderStyle* style,
                                               const NinePieceImage& ninePieceImage, CompositeOperator op)
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage || !styleImage->canRender(style->effectiveZoom()))
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    // If we have a border radius, the image gets clipped to the rounded rect.
    bool clipped = false;
    if (style->hasBorderRadius()) {
        IntRect clipRect(tx, ty, w, h);
        graphicsContext->save();
        graphicsContext->addRoundedRectClip(clipRect, style->borderTopLeftRadius(), style->borderTopRightRadius(),
                                            style->borderBottomLeftRadius(), style->borderBottomRightRadius());
        clipped = true;
    }

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    styleImage->setImageContainerSize(IntSize(w, h));
    IntSize imageSize = styleImage->imageSize(this, 1.0f);
    int imageWidth = imageSize.width();
    int imageHeight = imageSize.height();

    int topSlice = min(imageHeight, ninePieceImage.m_slices.top().calcValue(imageHeight));
    int bottomSlice = min(imageHeight, ninePieceImage.m_slices.bottom().calcValue(imageHeight));
    int leftSlice = min(imageWidth, ninePieceImage.m_slices.left().calcValue(imageWidth));
    int rightSlice = min(imageWidth, ninePieceImage.m_slices.right().calcValue(imageWidth));

    ENinePieceImageRule hRule = ninePieceImage.horizontalRule();
    ENinePieceImageRule vRule = ninePieceImage.verticalRule();

    bool fitToBorder = style->borderImage() == ninePieceImage;
    
    int leftWidth = fitToBorder ? style->borderLeftWidth() : leftSlice;
    int topWidth = fitToBorder ? style->borderTopWidth() : topSlice;
    int rightWidth = fitToBorder ? style->borderRightWidth() : rightSlice;
    int bottomWidth = fitToBorder ? style->borderBottomWidth() : bottomSlice;

    bool drawLeft = leftSlice > 0 && leftWidth > 0;
    bool drawTop = topSlice > 0 && topWidth > 0;
    bool drawRight = rightSlice > 0 && rightWidth > 0;
    bool drawBottom = bottomSlice > 0 && bottomWidth > 0;
    bool drawMiddle = (imageWidth - leftSlice - rightSlice) > 0 && (w - leftWidth - rightWidth) > 0 &&
                      (imageHeight - topSlice - bottomSlice) > 0 && (h - topWidth - bottomWidth) > 0;

    Image* image = styleImage->image(this, imageSize);

    if (drawLeft) {
        // Paint the top and bottom left corners.

        // The top left corner rect is (tx, ty, leftWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (0, 0, leftSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image, IntRect(tx, ty, leftWidth, topWidth),
                                       IntRect(0, 0, leftSlice, topSlice), op);

        // The bottom left corner rect is (tx, ty + h - bottomWidth, leftWidth, bottomWidth)
        // The rect to use from within the image is (0, imageHeight - bottomSlice, leftSlice, botomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image, IntRect(tx, ty + h - bottomWidth, leftWidth, bottomWidth),
                                       IntRect(0, imageHeight - bottomSlice, leftSlice, bottomSlice), op);

        // Paint the left edge.
        // Have to scale and tile into the border rect.
        graphicsContext->drawTiledImage(image, IntRect(tx, ty + topWidth, leftWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(0, topSlice, leftSlice, imageHeight - topSlice - bottomSlice),
                                        Image::StretchTile, (Image::TileRule)vRule, op);
    }

    if (drawRight) {
        // Paint the top and bottom right corners
        // The top right corner rect is (tx + w - rightWidth, ty, rightWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (imageWidth - rightSlice, 0, rightSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image, IntRect(tx + w - rightWidth, ty, rightWidth, topWidth),
                                       IntRect(imageWidth - rightSlice, 0, rightSlice, topSlice), op);

        // The bottom right corner rect is (tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth)
        // The rect to use from within the image is (imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image, IntRect(tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth),
                                       IntRect(imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice), op);

        // Paint the right edge.
        graphicsContext->drawTiledImage(image, IntRect(tx + w - rightWidth, ty + topWidth, rightWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(imageWidth - rightSlice, topSlice, rightSlice, imageHeight - topSlice - bottomSlice),
                                        Image::StretchTile, (Image::TileRule)vRule, op);
    }

    // Paint the top edge.
    if (drawTop)
        graphicsContext->drawTiledImage(image, IntRect(tx + leftWidth, ty, w - leftWidth - rightWidth, topWidth),
                                        IntRect(leftSlice, 0, imageWidth - rightSlice - leftSlice, topSlice),
                                        (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the bottom edge.
    if (drawBottom)
        graphicsContext->drawTiledImage(image, IntRect(tx + leftWidth, ty + h - bottomWidth,
                                        w - leftWidth - rightWidth, bottomWidth),
                                        IntRect(leftSlice, imageHeight - bottomSlice, imageWidth - rightSlice - leftSlice, bottomSlice),
                                        (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the middle.
    if (drawMiddle)
        graphicsContext->drawTiledImage(image, IntRect(tx + leftWidth, ty + topWidth, w - leftWidth - rightWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(leftSlice, topSlice, imageWidth - rightSlice - leftSlice, imageHeight - topSlice - bottomSlice),
                                        (Image::TileRule)hRule, (Image::TileRule)vRule, op);

    // Clear the clip for the border radius.
    if (clipped)
        graphicsContext->restore();

    return true;
}

void RenderBoxModelObject::paintBorder(GraphicsContext* graphicsContext, int tx, int ty, int w, int h,
                               const RenderStyle* style, bool begin, bool end)
{
    if (paintNinePieceImage(graphicsContext, tx, ty, w, h, style, style->borderImage()))
        return;

    const Color& tc = style->borderTopColor();
    const Color& bc = style->borderBottomColor();
    const Color& lc = style->borderLeftColor();
    const Color& rc = style->borderRightColor();

    bool tt = style->borderTopIsTransparent();
    bool bt = style->borderBottomIsTransparent();
    bool rt = style->borderRightIsTransparent();
    bool lt = style->borderLeftIsTransparent();

    EBorderStyle ts = style->borderTopStyle();
    EBorderStyle bs = style->borderBottomStyle();
    EBorderStyle ls = style->borderLeftStyle();
    EBorderStyle rs = style->borderRightStyle();

    bool renderTop = ts > BHIDDEN && !tt;
    bool renderLeft = ls > BHIDDEN && begin && !lt;
    bool renderRight = rs > BHIDDEN && end && !rt;
    bool renderBottom = bs > BHIDDEN && !bt;

    // Need sufficient width and height to contain border radius curves.  Sanity check our border radii
    // and our width/height values to make sure the curves can all fit. If not, then we won't paint
    // any border radii.
    bool renderRadii = false;
    IntSize topLeft = style->borderTopLeftRadius();
    IntSize topRight = style->borderTopRightRadius();
    IntSize bottomLeft = style->borderBottomLeftRadius();
    IntSize bottomRight = style->borderBottomRightRadius();

    if (style->hasBorderRadius() &&
        static_cast<unsigned>(w) >= static_cast<unsigned>(topLeft.width()) + static_cast<unsigned>(topRight.width()) &&
        static_cast<unsigned>(w) >= static_cast<unsigned>(bottomLeft.width()) + static_cast<unsigned>(bottomRight.width()) &&
        static_cast<unsigned>(h) >= static_cast<unsigned>(topLeft.height()) + static_cast<unsigned>(bottomLeft.height()) &&
        static_cast<unsigned>(h) >= static_cast<unsigned>(topRight.height()) + static_cast<unsigned>(bottomRight.height()))
        renderRadii = true;

    // Clip to the rounded rectangle.
    if (renderRadii) {
        graphicsContext->save();
        graphicsContext->addRoundedRectClip(IntRect(tx, ty, w, h), topLeft, topRight, bottomLeft, bottomRight);
    }

    int firstAngleStart, secondAngleStart, firstAngleSpan, secondAngleSpan;
    float thickness;
    bool upperLeftBorderStylesMatch = renderLeft && (ts == ls) && (tc == lc);
    bool upperRightBorderStylesMatch = renderRight && (ts == rs) && (tc == rc) && (ts != OUTSET) && (ts != RIDGE) && (ts != INSET) && (ts != GROOVE);
    bool lowerLeftBorderStylesMatch = renderLeft && (bs == ls) && (bc == lc) && (bs != OUTSET) && (bs != RIDGE) && (bs != INSET) && (bs != GROOVE);
    bool lowerRightBorderStylesMatch = renderRight && (bs == rs) && (bc == rc);

    if (renderTop) {
        bool ignore_left = (renderRadii && topLeft.width() > 0) ||
            (tc == lc && tt == lt && ts >= OUTSET &&
             (ls == DOTTED || ls == DASHED || ls == SOLID || ls == OUTSET));

        bool ignore_right = (renderRadii && topRight.width() > 0) ||
            (tc == rc && tt == rt && ts >= OUTSET &&
             (rs == DOTTED || rs == DASHED || rs == SOLID || rs == INSET));

        int x = tx;
        int x2 = tx + w;
        if (renderRadii) {
            x += topLeft.width();
            x2 -= topRight.width();
        }

        drawLineForBoxSide(graphicsContext, x, ty, x2, ty + style->borderTopWidth(), BSTop, tc, style->color(), ts,
                   ignore_left ? 0 : style->borderLeftWidth(), ignore_right ? 0 : style->borderRightWidth());

        if (renderRadii) {
            int leftY = ty;

            // We make the arc double thick and let the clip rect take care of clipping the extra off.
            // We're doing this because it doesn't seem possible to match the curve of the clip exactly
            // with the arc-drawing function.
            thickness = style->borderTopWidth() * 2;

            if (topLeft.width()) {
                int leftX = tx;
                // The inner clip clips inside the arc. This is especially important for 1px borders.
                bool applyLeftInnerClip = (style->borderLeftWidth() < topLeft.width())
                    && (style->borderTopWidth() < topLeft.height())
                    && (ts != DOUBLE || style->borderTopWidth() > 6);
                if (applyLeftInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(leftX, leftY, topLeft.width() * 2, topLeft.height() * 2),
                                                             style->borderTopWidth());
                }

                firstAngleStart = 90;
                firstAngleSpan = upperLeftBorderStylesMatch ? 90 : 45;

                // Draw upper left arc
                drawArcForBoxSide(graphicsContext, leftX, leftY, thickness, topLeft, firstAngleStart, firstAngleSpan,
                              BSTop, tc, style->color(), ts, true);
                if (applyLeftInnerClip)
                    graphicsContext->restore();
            }

            if (topRight.width()) {
                int rightX = tx + w - topRight.width() * 2;
                bool applyRightInnerClip = (style->borderRightWidth() < topRight.width())
                    && (style->borderTopWidth() < topRight.height())
                    && (ts != DOUBLE || style->borderTopWidth() > 6);
                if (applyRightInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(rightX, leftY, topRight.width() * 2, topRight.height() * 2),
                                                             style->borderTopWidth());
                }

                if (upperRightBorderStylesMatch) {
                    secondAngleStart = 0;
                    secondAngleSpan = 90;
                } else {
                    secondAngleStart = 45;
                    secondAngleSpan = 45;
                }

                // Draw upper right arc
                drawArcForBoxSide(graphicsContext, rightX, leftY, thickness, topRight, secondAngleStart, secondAngleSpan,
                              BSTop, tc, style->color(), ts, false);
                if (applyRightInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderBottom) {
        bool ignore_left = (renderRadii && bottomLeft.width() > 0) ||
            (bc == lc && bt == lt && bs >= OUTSET &&
             (ls == DOTTED || ls == DASHED || ls == SOLID || ls == OUTSET));

        bool ignore_right = (renderRadii && bottomRight.width() > 0) ||
            (bc == rc && bt == rt && bs >= OUTSET &&
             (rs == DOTTED || rs == DASHED || rs == SOLID || rs == INSET));

        int x = tx;
        int x2 = tx + w;
        if (renderRadii) {
            x += bottomLeft.width();
            x2 -= bottomRight.width();
        }

        drawLineForBoxSide(graphicsContext, x, ty + h - style->borderBottomWidth(), x2, ty + h, BSBottom, bc, style->color(), bs,
                   ignore_left ? 0 : style->borderLeftWidth(), ignore_right ? 0 : style->borderRightWidth());

        if (renderRadii) {
            thickness = style->borderBottomWidth() * 2;

            if (bottomLeft.width()) {
                int leftX = tx;
                int leftY = ty + h - bottomLeft.height() * 2;
                bool applyLeftInnerClip = (style->borderLeftWidth() < bottomLeft.width())
                    && (style->borderBottomWidth() < bottomLeft.height())
                    && (bs != DOUBLE || style->borderBottomWidth() > 6);
                if (applyLeftInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(leftX, leftY, bottomLeft.width() * 2, bottomLeft.height() * 2),
                                                             style->borderBottomWidth());
                }

                if (lowerLeftBorderStylesMatch) {
                    firstAngleStart = 180;
                    firstAngleSpan = 90;
                } else {
                    firstAngleStart = 225;
                    firstAngleSpan = 45;
                }

                // Draw lower left arc
                drawArcForBoxSide(graphicsContext, leftX, leftY, thickness, bottomLeft, firstAngleStart, firstAngleSpan,
                              BSBottom, bc, style->color(), bs, true);
                if (applyLeftInnerClip)
                    graphicsContext->restore();
            }

            if (bottomRight.width()) {
                int rightY = ty + h - bottomRight.height() * 2;
                int rightX = tx + w - bottomRight.width() * 2;
                bool applyRightInnerClip = (style->borderRightWidth() < bottomRight.width())
                    && (style->borderBottomWidth() < bottomRight.height())
                    && (bs != DOUBLE || style->borderBottomWidth() > 6);
                if (applyRightInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(rightX, rightY, bottomRight.width() * 2, bottomRight.height() * 2),
                                                             style->borderBottomWidth());
                }

                secondAngleStart = 270;
                secondAngleSpan = lowerRightBorderStylesMatch ? 90 : 45;

                // Draw lower right arc
                drawArcForBoxSide(graphicsContext, rightX, rightY, thickness, bottomRight, secondAngleStart, secondAngleSpan,
                              BSBottom, bc, style->color(), bs, false);
                if (applyRightInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderLeft) {
        bool ignore_top = (renderRadii && topLeft.height() > 0) ||
            (tc == lc && tt == lt && ls >= OUTSET &&
             (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET));

        bool ignore_bottom = (renderRadii && bottomLeft.height() > 0) ||
            (bc == lc && bt == lt && ls >= OUTSET &&
             (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET));

        int y = ty;
        int y2 = ty + h;
        if (renderRadii) {
            y += topLeft.height();
            y2 -= bottomLeft.height();
        }

        drawLineForBoxSide(graphicsContext, tx, y, tx + style->borderLeftWidth(), y2, BSLeft, lc, style->color(), ls,
                   ignore_top ? 0 : style->borderTopWidth(), ignore_bottom ? 0 : style->borderBottomWidth());

        if (renderRadii && (!upperLeftBorderStylesMatch || !lowerLeftBorderStylesMatch)) {
            int topX = tx;
            thickness = style->borderLeftWidth() * 2;

            if (!upperLeftBorderStylesMatch && topLeft.width()) {
                int topY = ty;
                bool applyTopInnerClip = (style->borderLeftWidth() < topLeft.width())
                    && (style->borderTopWidth() < topLeft.height())
                    && (ls != DOUBLE || style->borderLeftWidth() > 6);
                if (applyTopInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, topY, topLeft.width() * 2, topLeft.height() * 2),
                                                             style->borderLeftWidth());
                }

                firstAngleStart = 135;
                firstAngleSpan = 45;

                // Draw top left arc
                drawArcForBoxSide(graphicsContext, topX, topY, thickness, topLeft, firstAngleStart, firstAngleSpan,
                              BSLeft, lc, style->color(), ls, true);
                if (applyTopInnerClip)
                    graphicsContext->restore();
            }

            if (!lowerLeftBorderStylesMatch && bottomLeft.width()) {
                int bottomY = ty + h - bottomLeft.height() * 2;
                bool applyBottomInnerClip = (style->borderLeftWidth() < bottomLeft.width())
                    && (style->borderBottomWidth() < bottomLeft.height())
                    && (ls != DOUBLE || style->borderLeftWidth() > 6);
                if (applyBottomInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, bottomY, bottomLeft.width() * 2, bottomLeft.height() * 2),
                                                             style->borderLeftWidth());
                }

                secondAngleStart = 180;
                secondAngleSpan = 45;

                // Draw bottom left arc
                drawArcForBoxSide(graphicsContext, topX, bottomY, thickness, bottomLeft, secondAngleStart, secondAngleSpan,
                              BSLeft, lc, style->color(), ls, false);
                if (applyBottomInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderRight) {
        bool ignore_top = (renderRadii && topRight.height() > 0) ||
            ((tc == rc) && (tt == rt) &&
            (rs >= DOTTED || rs == INSET) &&
            (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET));

        bool ignore_bottom = (renderRadii && bottomRight.height() > 0) ||
            ((bc == rc) && (bt == rt) &&
            (rs >= DOTTED || rs == INSET) &&
            (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET));

        int y = ty;
        int y2 = ty + h;
        if (renderRadii) {
            y += topRight.height();
            y2 -= bottomRight.height();
        }

        drawLineForBoxSide(graphicsContext, tx + w - style->borderRightWidth(), y, tx + w, y2, BSRight, rc, style->color(), rs,
                   ignore_top ? 0 : style->borderTopWidth(), ignore_bottom ? 0 : style->borderBottomWidth());

        if (renderRadii && (!upperRightBorderStylesMatch || !lowerRightBorderStylesMatch)) {
            thickness = style->borderRightWidth() * 2;

            if (!upperRightBorderStylesMatch && topRight.width()) {
                int topX = tx + w - topRight.width() * 2;
                int topY = ty;
                bool applyTopInnerClip = (style->borderRightWidth() < topRight.width())
                    && (style->borderTopWidth() < topRight.height())
                    && (rs != DOUBLE || style->borderRightWidth() > 6);
                if (applyTopInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, topY, topRight.width() * 2, topRight.height() * 2),
                                                             style->borderRightWidth());
                }

                firstAngleStart = 0;
                firstAngleSpan = 45;

                // Draw top right arc
                drawArcForBoxSide(graphicsContext, topX, topY, thickness, topRight, firstAngleStart, firstAngleSpan,
                              BSRight, rc, style->color(), rs, true);
                if (applyTopInnerClip)
                    graphicsContext->restore();
            }

            if (!lowerRightBorderStylesMatch && bottomRight.width()) {
                int bottomX = tx + w - bottomRight.width() * 2;
                int bottomY = ty + h - bottomRight.height() * 2;
                bool applyBottomInnerClip = (style->borderRightWidth() < bottomRight.width())
                    && (style->borderBottomWidth() < bottomRight.height())
                    && (rs != DOUBLE || style->borderRightWidth() > 6);
                if (applyBottomInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(bottomX, bottomY, bottomRight.width() * 2, bottomRight.height() * 2),
                                                             style->borderRightWidth());
                }

                secondAngleStart = 315;
                secondAngleSpan = 45;

                // Draw bottom right arc
                drawArcForBoxSide(graphicsContext, bottomX, bottomY, thickness, bottomRight, secondAngleStart, secondAngleSpan,
                              BSRight, rc, style->color(), rs, false);
                if (applyBottomInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderRadii)
        graphicsContext->restore();
}

void RenderBoxModelObject::paintBoxShadow(GraphicsContext* context, int tx, int ty, int w, int h, const RenderStyle* s, bool begin, bool end)
{
    // FIXME: Deal with border-image.  Would be great to use border-image as a mask.

    IntRect rect(tx, ty, w, h);
    bool hasBorderRadius = s->hasBorderRadius();
    bool hasOpaqueBackground = s->backgroundColor().isValid() && s->backgroundColor().alpha() == 255;
    for (ShadowData* shadow = s->boxShadow(); shadow; shadow = shadow->next) {
        context->save();

        IntSize shadowOffset(shadow->x, shadow->y);
        int shadowBlur = shadow->blur;
        IntRect fillRect(rect);

        if (hasBorderRadius) {
            IntRect shadowRect(rect);
            shadowRect.inflate(shadowBlur);
            shadowRect.move(shadowOffset);
            context->clip(shadowRect);

            // Move the fill just outside the clip, adding 1 pixel separation so that the fill does not
            // bleed in (due to antialiasing) if the context is transformed.
            IntSize extraOffset(w + max(0, shadowOffset.width()) + shadowBlur + 1, 0);
            shadowOffset -= extraOffset;
            fillRect.move(extraOffset);
        }

        context->setShadow(shadowOffset, shadowBlur, shadow->color);
        if (hasBorderRadius) {
            IntSize topLeft = begin ? s->borderTopLeftRadius() : IntSize();
            IntSize topRight = end ? s->borderTopRightRadius() : IntSize();
            IntSize bottomLeft = begin ? s->borderBottomLeftRadius() : IntSize();
            IntSize bottomRight = end ? s->borderBottomRightRadius() : IntSize();
            if (!hasOpaqueBackground)
                context->clipOutRoundedRect(rect, topLeft, topRight, bottomLeft, bottomRight);
            context->fillRoundedRect(fillRect, topLeft, topRight, bottomLeft, bottomRight, Color::black);
        } else {
            if (!hasOpaqueBackground)
                context->clipOut(rect);
            context->fillRect(fillRect, Color::black);
        }
        context->restore();
    }
}

int RenderBoxModelObject::containingBlockWidthForContent() const
{
    return containingBlock()->availableWidth();
}

} // namespace WebCore
