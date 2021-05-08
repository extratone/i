/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef HTMLPlugInElement_H
#define HTMLPlugInElement_H

#include "HTMLElement.h"
#if PLATFORM(MAC)
#include <JavaScriptCore/runtime.h>
#include <JavaScriptCore/npruntime.h>
#endif

namespace WebCore {

class HTMLPlugInElement : public HTMLElement
{
public:
    HTMLPlugInElement(const QualifiedName& tagName, Document*);
    HTMLPlugInElement::~HTMLPlugInElement();

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void detach();
    
    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual bool checkDTD(const Node* newChild);

    String align() const;
    void setAlign(const String&);
    
    String height() const;
    void setHeight(const String&);
    
    String name() const;
    void setName(const String&);
    
    String width() const;
    void setWidth(const String&);

    virtual bool willRespondToMouseMoveEvents() { return false; }
    virtual bool willRespondToMouseDragEvents() { return false; }
    virtual bool willRespondToMouseClickEvents() { return true; }
    
#if PLATFORM(MAC)
    virtual KJS::Bindings::Instance* getInstance() const = 0;
#endif

    void setFrameName(const AtomicString& frameName) { m_frameName = frameName; }
private:
#if PLATFORM(MAC)
#endif

protected:
    String oldNameAttr;
#if PLATFORM(MAC)
    mutable RefPtr<KJS::Bindings::Instance> m_instance;
#endif
private:
    AtomicString m_frameName;
};

}

#endif
