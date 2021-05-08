/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006 Apple Computer, Inc.
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
#include "RegisteredEventListener.h"

#include "EventListener.h"

namespace WebCore {

RegisteredEventListener::RegisteredEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
    : m_eventType(eventType)
    , m_listener(listener)
    , m_useCapture(useCapture)
    , m_removed(false)
{
}

bool operator==(const RegisteredEventListener& a, const RegisteredEventListener& b)
{
    return a.eventType() == b.eventType() && a.listener() == b.listener() && a.useCapture() == b.useCapture();
}

} // namespace WebCore
