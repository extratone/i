/*
 * This file is part of the HTML rendering engine for KDE.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
*/
#ifndef HitTestRequest_h
#define HitTestRequest_h

namespace WebCore {

struct HitTestRequest {
    HitTestRequest(bool r, bool a, bool m = false, bool u = false)
        : readonly(r)
        , active(a)
        , mouseMove(m)
        , mouseUp(u)
    { 
    }

    bool readonly;
    bool active;
    bool mouseMove;
    bool mouseUp;
};

} // namespace WebCore

#endif // HitTestRequest_h
