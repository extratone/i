/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef HTMLKeygenElement_h
#define HTMLKeygenElement_h

#include "HTMLSelectElement.h"

namespace WebCore {

class HTMLKeygenElement : public HTMLSelectElement {
public:
    HTMLKeygenElement(const QualifiedName&, Document*, HTMLFormElement* = 0);

    virtual int tagPriority() const { return 0; }
    virtual const AtomicString& type() const;
    virtual bool isEnumeratable() const { return false; }
    virtual void parseMappedAttribute(MappedAttribute*);
    virtual bool appendFormData(FormDataList&, bool);

private:
    AtomicString m_challenge;
    AtomicString m_keyType;
};

} //namespace

#endif
