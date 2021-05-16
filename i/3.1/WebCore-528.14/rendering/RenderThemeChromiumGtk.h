/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Google, Inc.
 * All rights reserved.
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

#ifndef RenderThemeChromiumGtk_h
#define RenderThemeChromiumGtk_h

#include "RenderTheme.h"

#include <gtk/gtk.h>

namespace WebCore {

    class RenderThemeChromiumGtk : public RenderTheme {
    public:
        RenderThemeChromiumGtk();
        ~RenderThemeChromiumGtk() { }

        virtual String extraDefaultStyleSheet();
        virtual String extraQuirksStyleSheet();

        // A method asking if the theme's controls actually care about redrawing when hovered.
        virtual bool supportsHover(const RenderStyle*) const { return true; }

        // A method asking if the theme is able to draw the focus ring.
        virtual bool supportsFocusRing(const RenderStyle*) const;

        // The platform selection color.
        virtual Color platformActiveSelectionBackgroundColor() const;
        virtual Color platformInactiveSelectionBackgroundColor() const;
        virtual Color platformActiveSelectionForegroundColor() const;
        virtual Color platformInactiveSelectionForegroundColor() const;
        virtual Color platformTextSearchHighlightColor() const;

        virtual double caretBlinkInterval() const;

        // System fonts.
        virtual void systemFont(int propId, Document*, FontDescription&) const;

        virtual int minimumMenuListSize(RenderStyle*) const;

        virtual bool paintCheckbox(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void setCheckboxSize(RenderStyle*) const;

        virtual bool paintRadio(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void setRadioSize(RenderStyle*) const;

        virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual bool paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r) { return paintTextField(o, i, r); }

        virtual bool paintSearchField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual bool paintSearchFieldResultsDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual bool paintSearchFieldResultsButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual bool paintSearchFieldCancelButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        // MenuList refers to an unstyled menulist (meaning a menulist without
        // background-color or border set) and MenuListButton refers to a styled
        // menulist (a menulist with background-color or border set). They have
        // this distinction to support showing aqua style themes whenever they
        // possibly can, which is something we don't want to replicate.
        //
        // In short, we either go down the MenuList code path or the MenuListButton
        // codepath. We never go down both. And in both cases, they render the
        // entire menulist.
        virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintMenuList(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintMenuListButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        // These methods define the padding for the MenuList's inner block.
        virtual int popupInternalPaddingLeft(RenderStyle*) const;
        virtual int popupInternalPaddingRight(RenderStyle*) const;
        virtual int popupInternalPaddingTop(RenderStyle*) const;
        virtual int popupInternalPaddingBottom(RenderStyle*) const;

        virtual void adjustButtonInnerStyle(RenderStyle* style) const;

        // A method asking if the control changes its tint when the window has focus or not.
        virtual bool controlSupportsTints(const RenderObject*) const;

        // A general method asking if any control tinting is supported at all.
        virtual bool supportsControlTints() const { return true; }

        // List Box selection color
        virtual Color activeListBoxSelectionBackgroundColor() const;
        virtual Color activeListBoxSelectionForegroundColor() const;
        virtual Color inactiveListBoxSelectionBackgroundColor() const;
        virtual Color inactiveListBoxSelectionForegroundColor() const;

    private:
        // Hold the state
        GtkWidget* gtkEntry() const;
        GtkWidget* gtkTreeView() const;

        // Unmapped GdkWindow having a container. This is holding all our fake widgets
        GtkContainer* gtkContainer() const;

    private:
        int menuListInternalPadding(RenderStyle*, int paddingType) const;

        mutable GtkWidget* m_gtkWindow;
        mutable GtkContainer* m_gtkContainer;
        mutable GtkWidget* m_gtkEntry;
        mutable GtkWidget* m_gtkTreeView;
    };

} // namespace WebCore

#endif
