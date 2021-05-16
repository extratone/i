/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
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

#include "config.h"
#include "CachedScript.h"

#include "Cache.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "loader.h"
#include <wtf/Vector.h>

namespace WebCore {

CachedScript::CachedScript(DocLoader* dl, const String &url, CachePolicy cachePolicy, const DeprecatedString& charset)
    : CachedResource(url, Script, cachePolicy)
    , m_encoding(charset.latin1())
{
    // It's javascript we want.
    // But some websites think their scripts are <some wrong mimetype here>
    // and refuse to serve them if we only accept application/x-javascript.
    setAccept("*/*");
    m_errorOccurred = false;
    // load the file
    cache()->loader()->load(dl, this, false);
    m_loading = true;
    if (!m_encoding.isValid())
        m_encoding = TextEncoding(Latin1Encoding);
}

CachedScript::~CachedScript()
{
}

void CachedScript::ref(CachedResourceClient *c)
{
    CachedResource::ref(c);

    if(!m_loading) c->notifyFinished(this);
}

void CachedScript::setCharset(const DeprecatedString &chs)
{
    TextEncoding encoding = TextEncoding(chs.latin1());
    if (encoding.isValid())
        m_encoding = encoding;
}

void CachedScript::data(Vector<char>& data, bool allDataReceived)
{
    if (!allDataReceived)
        return;

    setEncodedSize(data.size());
    m_script = String(m_encoding.toUnicode(data.data(), encodedSize()));
    m_loading = false;
    checkNotify();
}

void CachedScript::checkNotify()
{
    if (m_loading)
        return;

    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient *c = w.next())
        c->notifyFinished(this);
}

void CachedScript::error()
{
    m_loading = false;
    m_errorOccurred = true;
    checkNotify();
}

}
