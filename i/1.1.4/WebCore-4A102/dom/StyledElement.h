/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef StyledElement_h
#define StyledElement_h

#include "NamedMappedAttrMap.h"

namespace WebCore {

class CSSMappedAttributeDeclaration;
class MappedAttribute;

class StyledElement : public Element
{
public:
    StyledElement(const QualifiedName&, Document*);
    virtual ~StyledElement();

    virtual bool isStyledElement() const { return true; }

    NamedMappedAttrMap* mappedAttributes() { return static_cast<NamedMappedAttrMap*>(namedAttrMap.get()); }
    const NamedMappedAttrMap* mappedAttributes() const { return static_cast<NamedMappedAttrMap*>(namedAttrMap.get()); }
    bool hasMappedAttributes() const { return namedAttrMap && mappedAttributes()->hasMappedAttributes(); }
    bool isMappedAttribute(const QualifiedName& name) const { MappedAttributeEntry res = eNone; mapToEntry(name, res); return res != eNone; }

    void addCSSLength(MappedAttribute* attr, int id, const String &value);
    void addCSSProperty(MappedAttribute* attr, int id, const String &value);
    void addCSSProperty(MappedAttribute* attr, int id, int value);
    void addCSSStringProperty(MappedAttribute* attr, int id, const String &value, CSSPrimitiveValue::UnitTypes = CSSPrimitiveValue::CSS_STRING);
    void addCSSImageProperty(MappedAttribute* attr, int id, const String &URL);
    void addCSSColor(MappedAttribute* attr, int id, const String &c);
    void createMappedDecl(MappedAttribute* attr);
    
    static CSSMappedAttributeDeclaration* getMappedAttributeDecl(MappedAttributeEntry type, Attribute* attr);
    static void setMappedAttributeDecl(MappedAttributeEntry type, Attribute* attr, CSSMappedAttributeDeclaration* decl);
    static void removeMappedAttributeDecl(MappedAttributeEntry type, const QualifiedName& attrName, const AtomicString& attrValue);
    
    CSSMutableStyleDeclaration* inlineStyleDecl() const { return m_inlineStyleDecl.get(); }
    virtual CSSMutableStyleDeclaration* additionalAttributeStyleDecl();
    CSSMutableStyleDeclaration* getInlineStyleDecl();
    CSSStyleDeclaration* style();
    void createInlineStyleDecl();
    void destroyInlineStyleDecl();
    void invalidateStyleAttribute();
    virtual void updateStyleAttributeIfNeeded() const;
    
    virtual const AtomicStringList* getClassList() const;
    virtual void attributeChanged(Attribute* attr, bool preserveDecls = false);
    virtual void parseMappedAttribute(MappedAttribute* attr);
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void createAttributeMap() const;
    virtual Attribute* createAttribute(const QualifiedName& name, StringImpl* value);

protected:
    RefPtr<CSSMutableStyleDeclaration> m_inlineStyleDecl;
    mutable bool m_isStyleAttributeValid : 1;
    mutable bool m_synchronizingStyleAttribute : 1;
};

} //namespace

#endif
