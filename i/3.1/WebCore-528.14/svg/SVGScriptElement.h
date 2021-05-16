/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>

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

#ifndef SVGScriptElement_h
#define SVGScriptElement_h

#if ENABLE(SVG)
#include "ScriptElement.h"
#include "SVGElement.h"
#include "SVGURIReference.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore {

    class SVGScriptElement : public SVGElement
                           , public SVGURIReference
                           , public SVGExternalResourcesRequired
                           , public ScriptElement {
    public:
        SVGScriptElement(const QualifiedName&, Document*, bool createdByParser);
        virtual ~SVGScriptElement();

        virtual String scriptContent() const;

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void insertedIntoDocument();
        virtual void removedFromDocument();
        virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

        virtual void svgAttributeChanged(const QualifiedName&);
        virtual bool isURLAttribute(Attribute*) const;
        virtual void finishParsingChildren();

        String type() const;
        void setType(const String&);

        virtual String scriptCharset() const;

        virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }
        virtual bool haveLoadedRequiredResources();

        virtual String sourceAttributeValue() const;
        virtual String charsetAttributeValue() const;
        virtual String typeAttributeValue() const;
        virtual String languageAttributeValue() const;

        virtual void dispatchLoadEvent();
        virtual void dispatchErrorEvent();

    private:
        ScriptElementData m_data;
        String m_type;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
