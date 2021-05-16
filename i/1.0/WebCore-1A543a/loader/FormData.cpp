/* This file is part of the KDE project
 *
 * Copyright (C) 2004 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "FormData.h"

#include <wtf/Vector.h>

namespace WebCore {

FormData::FormData()
{
}

FormData::FormData(const DeprecatedCString &s)
{
    appendData(s.data(), s.length());
}

void FormData::appendData(const void *data, size_t size)
{
    if (m_elements.isEmpty() || m_elements.last().m_type != FormDataElement::data) {
        m_elements.append(FormDataElement());
    }
    FormDataElement &e = m_elements.last();
    size_t oldSize = e.m_data.size();
    e.m_data.resize(oldSize + size);
    memcpy(e.m_data.data() + oldSize, data, size);
}

void FormData::appendFile(const DeprecatedString &filename)
{
    m_elements.append(filename);
}

Vector<char> FormData::flatten() const
{
    // Concatenate all the byte arrays, but omit any files.
    Vector<char> a;
    for (DeprecatedValueListConstIterator<FormDataElement> it = m_elements.begin(); it != m_elements.end(); ++it) {
        const FormDataElement& e = *it;
        if (e.m_type == FormDataElement::data) {
            if (a.isEmpty())
                a = e.m_data;
            else {
                size_t oldSize = a.size();
                size_t delta = e.m_data.size();
                a.resize(oldSize + delta);
                memcpy(a.data() + oldSize, e.m_data.data(), delta);
            }
        }
    }
    return a;
}

DeprecatedString FormData::flattenToString() const
{
    Vector<char> bytes = flatten();
    return DeprecatedString::fromLatin1(bytes.data(), bytes.size());
}

} // namespace WebCore
