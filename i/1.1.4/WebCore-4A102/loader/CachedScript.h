/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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

#ifndef CachedScript_h
#define CachedScript_h

#include "CachedResource.h"
#include "TextEncoding.h"
#include <wtf/Vector.h>

namespace WebCore {
    class DocLoader;

    class CachedScript : public CachedResource {
    public:
        CachedScript(DocLoader*, const String& URL, CachePolicy, const DeprecatedString& charset);
        virtual ~CachedScript();

        const String& script() const { return m_script; }

        virtual void ref(CachedResourceClient*);

        virtual void setCharset(const DeprecatedString&);
        virtual void data(Vector<char>&, bool allDataReceived);
        virtual void error();

        virtual bool schedule() const { return false; }
        
        bool errorOccurred() const { return m_errorOccurred; }

        void checkNotify();

    private:
        String m_script;
        TextEncoding m_encoding;
        bool m_errorOccurred;
    };
}

#endif
