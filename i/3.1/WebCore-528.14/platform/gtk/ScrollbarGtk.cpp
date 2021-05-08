/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther zecke@selfish.org
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ScrollbarGtk.h"

#include "IntRect.h"
#include "GraphicsContext.h"
#include "FrameView.h"
#include "NotImplemented.h"
#include "ScrollbarTheme.h"
#include "gtkdrawing.h"

#include <gtk/gtk.h>

using namespace WebCore;

PassRefPtr<Scrollbar> Scrollbar::createNativeScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
{
    return adoptRef(new ScrollbarGtk(client, orientation, size));
}

static gboolean gtkScrollEventCallback(GtkWidget* widget, GdkEventScroll* event, ScrollbarGtk*)
{
    /* Scroll only if our parent rejects the scroll event. The rationale for
     * this is that we want the main frame to scroll when we move the mouse
     * wheel over a child scrollbar in most cases. */
    return gtk_widget_event(gtk_widget_get_parent(widget), reinterpret_cast<GdkEvent*>(event));
}

ScrollbarGtk::ScrollbarGtk(ScrollbarClient* client, ScrollbarOrientation orientation,
                                     ScrollbarControlSize controlSize)
    : Scrollbar(client, orientation, controlSize)
    , m_adjustment(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)))
{
    GtkScrollbar* scrollBar = orientation == HorizontalScrollbar ?
                              GTK_SCROLLBAR(::gtk_hscrollbar_new(m_adjustment)) :
                              GTK_SCROLLBAR(::gtk_vscrollbar_new(m_adjustment));
    gtk_widget_show(GTK_WIDGET(scrollBar));
    g_object_ref(G_OBJECT(scrollBar));
    g_signal_connect(G_OBJECT(scrollBar), "value-changed", G_CALLBACK(ScrollbarGtk::gtkValueChanged), this);
    g_signal_connect(G_OBJECT(scrollBar), "scroll-event", G_CALLBACK(gtkScrollEventCallback), this);

    setPlatformWidget(GTK_WIDGET(scrollBar));

    /*
     * assign a sane default width and height to the Scrollbar, otherwise
     * we will end up with a 0 width scrollbar.
     */
    resize(ScrollbarTheme::nativeTheme()->scrollbarThickness(),
           ScrollbarTheme::nativeTheme()->scrollbarThickness());
}

ScrollbarGtk::~ScrollbarGtk()
{
    /*
     * the Widget does not take over ownership.
     */
    g_signal_handlers_disconnect_by_func(G_OBJECT(platformWidget()), (gpointer)ScrollbarGtk::gtkValueChanged, this);
    g_signal_handlers_disconnect_by_func(G_OBJECT(platformWidget()), (gpointer)gtkScrollEventCallback, this);
    g_object_unref(G_OBJECT(platformWidget()));
}

void ScrollbarGtk::frameRectsChanged()
{
    if (!parent() || !parent()->isScrollViewScrollbar(this))
        return;

    IntPoint loc = parent()->convertToContainingWindow(frameRect().location());

    // Don't allow the allocation size to be negative
    IntSize sz = frameRect().size();
    sz.clampNegativeToZero();

    GtkAllocation allocation = { loc.x(), loc.y(), sz.width(), sz.height() };
    gtk_widget_size_allocate(platformWidget(), &allocation);
}

void ScrollbarGtk::updateThumbPosition()
{
    if (m_adjustment->value != m_currentPos) {
        m_adjustment->value = m_currentPos;
        gtk_adjustment_value_changed(m_adjustment);
    }
}

void ScrollbarGtk::updateThumbProportion()
{
    m_adjustment->step_increment = m_lineStep;
    m_adjustment->page_increment = m_pageStep;
    m_adjustment->page_size = m_visibleSize;
    m_adjustment->upper = m_totalSize;
    gtk_adjustment_changed(m_adjustment);
}

void ScrollbarGtk::setFrameRect(const IntRect& rect)
{
    Widget::setFrameRect(rect);
    frameRectsChanged();
}

void ScrollbarGtk::gtkValueChanged(GtkAdjustment*, ScrollbarGtk* that)
{
    that->setValue(static_cast<int>(gtk_adjustment_get_value(that->m_adjustment)));
}

void ScrollbarGtk::setEnabled(bool shouldEnable)
{
    if (enabled() == shouldEnable)
        return;
        
    Scrollbar::setEnabled(shouldEnable);
    if (platformWidget()) 
        gtk_widget_set_sensitive(platformWidget(), shouldEnable);
}



