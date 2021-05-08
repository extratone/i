/*
 * This file is part of the KDE project.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef RenderPart_h
#define RenderPart_h

#include "RenderWidget.h"

namespace WebCore {

class Frame;
class HTMLFrameOwnerElement;

class RenderPart : public RenderWidget {
public:
    RenderPart(Element*);
    virtual ~RenderPart();
    
    virtual bool isRenderPart() const { return true; }
    virtual const char* renderName() const { return "RenderPart"; }

    virtual void setWidget(Widget*);

    // FIXME: This should not be necessary.
    // Remove this once WebKit knows to properly schedule layouts using WebCore when objects resize.
    virtual void updateWidgetPosition();

    bool hasFallbackContent() const { return m_hasFallbackContent; }

    virtual void viewCleared();

protected:
    bool m_hasFallbackContent;

private:
    virtual void deleteWidget();
};

}

#endif
