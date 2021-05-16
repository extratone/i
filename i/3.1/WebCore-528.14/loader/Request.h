/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef Request_h
#define Request_h

#include <wtf/Vector.h>

namespace WebCore {

    class CachedResource;
    class DocLoader;

    class Request {
    public:
        Request(DocLoader*, CachedResource*, bool incremental, bool skipCanLoadCheck, bool sendResourceLoadCallbacks);
        ~Request();
        
        Vector<char>& buffer() { return m_buffer; }
        CachedResource* cachedResource() { return m_object; }
        DocLoader* docLoader() { return m_docLoader; }

        bool isIncremental() { return m_incremental; }
        void setIsIncremental(bool b = true) { m_incremental = b; }

        bool isMultipart() { return m_multipart; }
        void setIsMultipart(bool b = true) { m_multipart = b; }

        bool shouldSkipCanLoadCheck() const { return m_shouldSkipCanLoadCheck; }
        bool sendResourceLoadCallbacks() const { return m_sendResourceLoadCallbacks; }
        
    private:
        Vector<char> m_buffer;
        CachedResource* m_object;
        DocLoader* m_docLoader;
        bool m_incremental;
        bool m_multipart;
        bool m_shouldSkipCanLoadCheck;
        bool m_sendResourceLoadCallbacks;
    };

} //namespace WebCore

#endif // Request_h
