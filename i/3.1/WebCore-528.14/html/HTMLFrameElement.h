/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#ifndef HTMLFrameElement_h
#define HTMLFrameElement_h

#include "HTMLFrameElementBase.h"

namespace WebCore {

class Document;
class RenderObject;
class RenderArena;
class RenderStyle;

class HTMLFrameElement : public HTMLFrameElementBase
{
public:
    HTMLFrameElement(const QualifiedName&, Document*, bool createdByParser);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }
  
    virtual void attach();

    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    
    virtual void parseMappedAttribute(MappedAttribute*);

    bool hasFrameBorder() const { return m_frameBorder; }

private:
    bool m_frameBorder;
    bool m_frameBorderSet;
};

} // namespace WebCore

#endif // HTMLFrameElement_h
