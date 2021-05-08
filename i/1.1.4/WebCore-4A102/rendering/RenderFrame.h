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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef RenderFrame_H
#define RenderFrame_H

#include "RenderPart.h"
#include "HTMLFrameElement.h"

namespace WebCore {

class RenderFrame : public RenderPart
{
public:
    RenderFrame(HTMLFrameElement*);

    virtual const char* renderName() const { return "RenderFrame"; }
    virtual bool isFrame() const { return true; }

    HTMLFrameElement* element() const
        { return static_cast<HTMLFrameElement*>(RenderPart::element()); }

    virtual void viewCleared();
    void layoutWithFlattening(bool flexibleWidth, bool flexibleHeight);
};

}

#endif
