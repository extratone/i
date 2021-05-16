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
#include "CachedXSLStyleSheet.h"

#include "Cache.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "Decoder.h"
#include "loader.h"
#include <wtf/Vector.h>

namespace WebCore {

#ifdef KHTML_XSLT

CachedXSLStyleSheet::CachedXSLStyleSheet(DocLoader* dl, const String &url, CachePolicy cachePolicy)
    : CachedResource(url, XSLStyleSheet, cachePolicy)
{
    // It's XML we want.
    // FIXME: This should accept more general xml formats */*+xml, image/svg+xml for example.
    setAccept("text/xml, application/xml, application/xhtml+xml, text/xsl, application/rss+xml, application/atom+xml");
    
    // load the file
    cache()->loader()->load(dl, this, false);
    m_loading = true;
    m_decoder = new Decoder;
}

void CachedXSLStyleSheet::ref(CachedResourceClient *c)
{
    CachedResource::ref(c);
    
    if (!m_loading)
        c->setStyleSheet(m_url, m_sheet);
}

void CachedXSLStyleSheet::setCharset( const DeprecatedString &chs )
{
    if (!chs.isEmpty())
        m_decoder->setEncodingName(chs.latin1(), Decoder::EncodingFromHTTPHeader);
}

void CachedXSLStyleSheet::data(Vector<char>& data, bool allDataReceived)
{
    if (!allDataReceived)
        return;

    setEncodedSize(data.size());
    m_sheet = String(m_decoder->decode(data.data(), encodedSize()));
    m_loading = false;
    checkNotify();
}

void CachedXSLStyleSheet::checkNotify()
{
    if (m_loading)
        return;
    
    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient *c = w.next())
        c->setStyleSheet(m_url, m_sheet);
}


void CachedXSLStyleSheet::error()
{
    m_loading = false;
    checkNotify();
}

#endif

}
