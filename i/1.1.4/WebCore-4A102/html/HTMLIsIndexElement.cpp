/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "HTMLIsIndexElement.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLIsIndexElement::HTMLIsIndexElement(Document *doc, HTMLFormElement *f)
    : HTMLInputElement(isindexTag, doc, f)
{
    m_name = "isindex";
}

void HTMLIsIndexElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == promptAttr)
        setValue(attr->value());
    else
        // don't call HTMLInputElement::parseMappedAttribute here, as it would
        // accept attributes this element does not support
        HTMLGenericFormElement::parseMappedAttribute(attr);
}

String HTMLIsIndexElement::prompt() const
{
    return getAttribute(promptAttr);
}

void HTMLIsIndexElement::setPrompt(const String &value)
{
    setAttribute(promptAttr, value);
}

} // namespace
