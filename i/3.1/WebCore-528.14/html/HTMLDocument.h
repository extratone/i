/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef HTMLDocument_h
#define HTMLDocument_h

#include "CachedResourceClient.h"
#include "Document.h"

namespace WebCore {

class FrameView;
class HTMLElement;

class HTMLDocument : public Document, public CachedResourceClient {
public:
    static PassRefPtr<HTMLDocument> create(Frame* frame)
    {
        return new HTMLDocument(frame);
    }
    virtual ~HTMLDocument();

    int width();
    int height();

    String dir();
    void setDir(const String&);

    String designMode() const;
    void setDesignMode(const String&);

    String compatMode() const;

    Element* activeElement();
    bool hasFocus();

    String bgColor();
    void setBgColor(const String&);
    String fgColor();
    void setFgColor(const String&);
    String alinkColor();
    void setAlinkColor(const String&);
    String linkColor();
    void setLinkColor(const String&);
    String vlinkColor();
    void setVlinkColor(const String&);

    void clear();

    void captureEvents();
    void releaseEvents();

    virtual bool childAllowed(Node*);

    virtual PassRefPtr<Element> createElement(const AtomicString& tagName, ExceptionCode&);

    void addNamedItem(const AtomicString& name);
    void removeNamedItem(const AtomicString& name);
    bool hasNamedItem(AtomicStringImpl* name);

    void addExtraNamedItem(const AtomicString& name);
    void removeExtraNamedItem(const AtomicString& name);
    bool hasExtraNamedItem(AtomicStringImpl* name);

    typedef HashMap<AtomicStringImpl*, int> NameCountMap;

protected:
    HTMLDocument(Frame*);

private:
    virtual bool isHTMLDocument() const { return true; }
    virtual Tokenizer* createTokenizer();
    virtual void determineParseMode();

    NameCountMap m_namedItemCounts;
    NameCountMap m_extraNamedItemCounts;
};

inline bool HTMLDocument::hasNamedItem(AtomicStringImpl* name)
{
    ASSERT(name);
    return m_namedItemCounts.contains(name);
}

inline bool HTMLDocument::hasExtraNamedItem(AtomicStringImpl* name)
{
    ASSERT(name);
    return m_extraNamedItemCounts.contains(name);
}

} // namespace

#endif
