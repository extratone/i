/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the KDE project

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
*/

#include "config.h"
#if SVG_SUPPORT
#include "SVGStyleElement.h"

#include "CSSStyleSheet.h"
#include "DeprecatedString.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "PlatformString.h"

namespace WebCore {

SVGStyleElement::SVGStyleElement(const QualifiedName& tagName, Document *doc) : SVGElement(tagName, doc)
{
    m_loading = false;
}

SVGStyleElement::~SVGStyleElement()
{
}

AtomicString SVGStyleElement::xmlspace() const
{
    return tryGetAttribute("xml:space");
}

void SVGStyleElement::setXmlspace(const AtomicString&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

AtomicString SVGStyleElement::type() const
{
    return tryGetAttribute("type", "text/css");
}

void SVGStyleElement::setType(const AtomicString&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

AtomicString SVGStyleElement::media() const
{
    return tryGetAttribute("media", "all");
}

void SVGStyleElement::setMedia(const AtomicString&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

AtomicString SVGStyleElement::title() const
{
    return tryGetAttribute("title");
}

void SVGStyleElement::setTitle(const AtomicString&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

CSSStyleSheet *SVGStyleElement::sheet()
{
    return m_sheet.get();
}

void SVGStyleElement::childrenChanged()
{
    SVGElement::childrenChanged();

    if(m_sheet)
        m_sheet = 0;

    m_loading = false;
    MediaQueryEvaluator screenEval("screen", true);
    MediaQueryEvaluator printEval("print", true);   
    RefPtr<MediaList> mediaList = new MediaList((CSSStyleSheet*)0, media());
    if ((type().isEmpty() || type() == "text/css") && (screenEval.eval(mediaList.get()) || printEval.eval(mediaList.get()))) {
        ownerDocument()->addPendingSheet();

        m_loading = true;
 
        m_sheet = new CSSStyleSheet(this);
        m_sheet->parseString(textContent()); // SVG css is always parsed in strict mode
        
        m_sheet->setMedia(mediaList.get());
        m_loading = false;
    }

    if(!isLoading() && m_sheet)
        document()->stylesheetLoaded();
}

bool SVGStyleElement::isLoading() const
{
    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT
