/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGStyleElement_h
#define SVGStyleElement_h
#if ENABLE(SVG)

#include <SVGElement.h>
#include "StyleElement.h"

namespace WebCore {

    class SVGStyleElement : public SVGElement, public StyleElement {
    public:
        SVGStyleElement(const QualifiedName&, Document*, bool createdByParser);

        // Derived from: 'Element'
        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void insertedIntoDocument();
        virtual void removedFromDocument();
        virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

        virtual void finishParsingChildren();

        // 'SVGStyleElement' functions
        const AtomicString& xmlspace() const;
        void setXmlspace(const AtomicString&, ExceptionCode&);

        virtual bool sheetLoaded();

        virtual const AtomicString& type() const;
        void setType(const AtomicString&, ExceptionCode&);

        virtual const AtomicString& media() const;
        void setMedia(const AtomicString&, ExceptionCode&);

        virtual String title() const;
        void setTitle(const AtomicString&, ExceptionCode&);

        StyleSheet* sheet();
        
    protected:
        bool m_createdByParser;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGStyleElement_h

// vim:ts=4:noet
