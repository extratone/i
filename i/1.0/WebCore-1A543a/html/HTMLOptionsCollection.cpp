/*
 * This file is part of the DOM implementation for KDE.
 *
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

#include "config.h"
#include "HTMLOptionsCollection.h"

#include "ExceptionCode.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"

namespace WebCore {

HTMLOptionsCollection::HTMLOptionsCollection(HTMLSelectElement* select)
    : HTMLCollection(select, SelectOptions)
{
    ASSERT(!info);
    info = select->collectionInfo();
}

void HTMLOptionsCollection::add(PassRefPtr<HTMLOptionElement> element, ExceptionCode &ec)
{
    add(element, length(), ec);
}

void HTMLOptionsCollection::add(PassRefPtr<HTMLOptionElement> element, int index, ExceptionCode &ec)
{
    HTMLOptionElement* newOption = element.get();

    if (!newOption) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    if (index < -1) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    ec = 0;
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(m_base.get());

    if (index == -1 || unsigned(index) >= length())
        select->add(newOption, 0, ec);
    else
        select->add(newOption, static_cast<HTMLOptionElement*>(item(index)), ec);

    ASSERT(ec == 0);
}

int HTMLOptionsCollection::selectedIndex() const
{
    return static_cast<HTMLSelectElement*>(m_base.get())->selectedIndex();
}

void HTMLOptionsCollection::setSelectedIndex(int index)
{
    static_cast<HTMLSelectElement*>(m_base.get())->setSelectedIndex(index);
}

void HTMLOptionsCollection::setLength(unsigned length, ExceptionCode& ec)
{
    static_cast<HTMLSelectElement*>(m_base.get())->setLength(length, ec);
}

} //namespace
