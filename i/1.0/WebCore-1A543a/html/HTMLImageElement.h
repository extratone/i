/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#ifndef HTMLImageElement_H
#define HTMLImageElement_H

#include "HTMLElement.h"
#include "GraphicsTypes.h"
#include "HTMLImageLoader.h"

namespace WebCore {
    class HTMLFormElement;

class HTMLImageElement : public HTMLElement {
    friend class HTMLFormElement;
public:
    HTMLImageElement(Document*, HTMLFormElement* = 0);
    HTMLImageElement(const QualifiedName&, Document*);
    ~HTMLImageElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void attach();
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    int width(bool ignorePendingStylesheets = false) const;
    int height(bool ignorePendingStylesheets = false) const;

    bool isServerMap() const { return ismap && usemap.isEmpty(); }

    String altText() const;

    String imageMap() const { return usemap; }
    
    virtual bool isURLAttribute(Attribute*) const;

    CompositeOperator compositeOperator() const { return m_compositeOperator; }

    CachedImage* cachedImage() const { return m_imageLoader.image(); }
    
    void setLoadManually (bool loadManually) { m_imageLoader.setLoadManually(loadManually); }

    String name() const;
    void setName(const String&);

    String align() const;
    void setAlign(const String&);

    String alt() const;
    void setAlt(const String&);

    int border() const;
    void setBorder(int);

    void setHeight(int);

    int hspace() const;
    void setHspace(int);

    bool isMap() const;
    void setIsMap(bool);

    String longDesc() const;
    void setLongDesc(const String&);

    String src() const;
    void setSrc(const String&);

    String useMap() const;
    void setUseMap(const String&);

    int vspace() const;
    void setVspace(int);

    void setWidth(int);

    int x() const;
    int y() const;

    bool complete() const;

protected:
    HTMLImageLoader m_imageLoader;
    String usemap;
    bool ismap;
    HTMLFormElement* m_form;
    String oldNameAttr;
    CompositeOperator m_compositeOperator;
};

} //namespace

#endif
