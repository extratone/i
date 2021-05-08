/**
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
#include "RenderApplet.h"

#include "Document.h"
#include "HTMLAppletElement.h"
#include "HTMLNames.h"
#include "HTMLParamElement.h"
#include "JavaAppletWidget.h"

namespace WebCore {

using namespace HTMLNames;

RenderApplet::RenderApplet(HTMLAppletElement* applet, const HashMap<String, String>& args)
    : RenderWidget(applet), m_args(args)
{
    setInline(true);
}

RenderApplet::~RenderApplet()
{
}

int RenderApplet::intrinsicWidth() const
{
    if (!m_widget)
        return 150;
    int w = m_widget->sizeHint().width();
    return w > 10 ? w : 50;
}

int RenderApplet::intrinsicHeight() const
{
    if (!m_widget)
        return 150;
    int h = m_widget->sizeHint().height();
    return h > 10 ? h : 50;
}

void RenderApplet::createWidgetIfNecessary()
{
}

void RenderApplet::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    calcWidth();
    calcHeight();

    // The applet's widget gets created lazily upon first layout.
    createWidgetIfNecessary();
    setNeedsLayout(false);
}

}
