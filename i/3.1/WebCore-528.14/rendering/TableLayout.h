/*
 * This file is part of the HTML rendering engine for KDE.
 *
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
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
 */

#ifndef TableLayout_h
#define TableLayout_h

namespace WebCore {

class RenderTable;

class TableLayout {
public:
    TableLayout(RenderTable* table)
        : m_table(table)
    {
    }

    virtual ~TableLayout() { }

    virtual void calcPrefWidths(int& minWidth, int& maxWidth) = 0;
    virtual void layout() = 0;

protected:
    RenderTable* m_table;
};

} // namespace WebCore

#endif // TableLayout_h
