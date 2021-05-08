/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
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


#ifndef RENDER_THEME_PURPLE_H
#define RENDER_THEME_PURPLE_H

#import "RenderTheme.h"
#import "GraphicsContext.h"


namespace WebCore {
    
class RenderStyle;
    
class RenderThemePurple : public RenderTheme {
public:
    
    virtual int auxiliaryMenuListRightPadding(RenderMenuList *) const;
                
protected:

    virtual short baselinePosition(const RenderObject* o) const;

    virtual bool isControlStyled(const RenderStyle* style, const BorderData& border, const BackgroundLayer& background, const Color& backgroundColor) const;
    
    // Methods for each appearance value.
    virtual void adjustCheckboxStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintCheckboxDecorations(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustRadioStyle(CSSStyleSelector*, RenderStyle*, Element*) const;    
    virtual bool paintRadioDecorations(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    
    virtual bool paintPushButtonDecorations(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    virtual void setButtonSize(RenderStyle*) const;
    
    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintTextFieldDecorations(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool paintTextAreaDecorations(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintMenuListButtonDecorations(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual RenderPopupMenu* createPopupMenu(RenderArena *aRenderArena, Document *aDocument, RenderMenuList* menuList);
    
private:

    typedef enum
    {
        InsetGradient,
        ShineGradient,
        ShadeGradient,
        ConvexGradient,
        ConcaveGradient
    } GradientName;
    
    Color * shadowColor() const;
    GradientRef gradientWithName(GradientName aGradientName) const;
    FloatRect addRoundedBorderClip(FloatRect aRect, RenderObject * aRenderer, GraphicsContext * aContext);
};
    
}

#endif // RENDER_THEME_PURPLE_H

