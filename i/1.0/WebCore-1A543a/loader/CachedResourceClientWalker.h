/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef CachedResourceClientWalker_h
#define CachedResourceClientWalker_h

#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

    class CachedResourceClient;

    // Call this "walker" instead of iterator so people won't expect Qt or STL-style iterator interface.
    // Just keep calling next() on this. It's safe from deletions of items.
    class CachedResourceClientWalker {
    public:
        CachedResourceClientWalker(const HashSet<CachedResourceClient*>&);
        CachedResourceClient* next();
    private:
        const HashSet<CachedResourceClient*>& m_clientSet;
        Vector<CachedResourceClient*> m_clientVector;
        size_t m_index;
    };

}

#endif
