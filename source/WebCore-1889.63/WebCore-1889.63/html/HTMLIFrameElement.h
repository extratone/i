/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLIFrameElement_h
#define HTMLIFrameElement_h

#include "HTMLFrameElementBase.h"

namespace WebCore {

class HTMLIFrameElement FINAL : public HTMLFrameElementBase {
public:
    static PassRefPtr<HTMLIFrameElement> create(const QualifiedName&, Document*);

    bool shouldDisplaySeamlessly() const;

private:
    HTMLIFrameElement(const QualifiedName&, Document*);

#if PLATFORM(IOS)
    virtual bool isKeyboardFocusable(KeyboardEvent*) const OVERRIDE { return false; }
#endif

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;

    virtual bool rendererIsNeeded(const NodeRenderingContext&);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    virtual void didRecalcStyle(StyleChange) OVERRIDE;

#if ENABLE(MICRODATA)
    virtual String itemValueText() const OVERRIDE;
    virtual void setItemValueText(const String&, ExceptionCode&) OVERRIDE;
#endif
};

} // namespace WebCore

#endif // HTMLIFrameElement_h
