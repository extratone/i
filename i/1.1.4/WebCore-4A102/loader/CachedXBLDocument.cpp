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

#ifndef KHTML_NO_XBL

#include "CachedXBLDocument.h"

#include "Cache.h"
#include "CachedResourceClientWalker.h"
#include "Decoder.h"
#include "loader.h"
#include <wtf/Vector.h>

namespace WebCore {

CachedXBLDocument::CachedXBLDocument(DocLoader* dl, const String &url, CachePolicy cachePolicy)
: CachedResource(url, XBL, cachePolicy), m_document(0)
{
    // It's XML we want.
    setAccept("text/xml, application/xml, application/xhtml+xml, text/xsl, application/rss+xml, application/atom+xml");
    
    // Load the file
    Cache::loader()->load(dl, this, false);
    m_loading = true;
    m_decoder = new Decoder;
}

CachedXBLDocument::~CachedXBLDocument()
{
    if (m_document)
        m_document->deref();
}

void CachedXBLDocument::ref(CachedResourceClient *c)
{
    CachedResource::ref(c);
    if (!m_loading)
        c->setXBLDocument(m_url, m_document);
}

void CachedXBLDocument::setCharset( const DeprecatedString &chs )
{
    if (!chs.isEmpty())
        m_decoder->setEncoding(chs.latin1(), Decoder::EncodingFromHTTPHeader);
}

void CachedXBLDocument::data(Vector<char>& data, bool )
{
    if (!allDataReceived)
        return;
    
    assert(!m_document);
    
    m_document = new XBL::XBLDocument();
    m_document->ref();
    m_document->open();
    
    m_document->write(m_decoder->decode(data.data(), data.size()));
    setSize(data.size());
    
    m_document->finishParsing();
    m_document->close();
    m_loading = false;
    checkNotify();
}

void CachedXBLDocument::checkNotify()
{
    if (m_loading)
        return;
    
    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient *c = w.next())
        c->setXBLDocument(m_url, m_document);
}

void CachedXBLDocument::error()
{
    m_loading = false;
    checkNotify();
}

}

#endif
