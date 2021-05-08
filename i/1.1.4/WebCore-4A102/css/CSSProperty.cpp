/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "CSSProperty.h"
#include "PlatformString.h"

// Not in any header, so just declare it here for now.
WebCore::String getPropertyName(unsigned short id);

namespace WebCore {

String CSSProperty::cssText() const
{
    return getPropertyName(id()) + ": " + m_value->cssText() + (isImportant() ? " !important" : "") + "; ";
}

bool operator==(const CSSProperty &a, const CSSProperty &b)
{
    return a.m_id == b.m_id && a.m_important == b.m_important && a.m_value == b.m_value;
}

}
