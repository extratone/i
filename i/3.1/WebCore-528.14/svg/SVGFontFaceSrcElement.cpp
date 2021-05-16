/*
   Copyright (C) 2007 Eric Seidel <eric@webkit.org>

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

#include "config.h"

#if ENABLE(SVG_FONTS)
#include "SVGFontFaceSrcElement.h"

#include "CSSValueList.h"
#include "CSSFontFaceSrcValue.h"
#include "SVGFontFaceElement.h"
#include "SVGFontFaceNameElement.h"
#include "SVGFontFaceUriElement.h"
#include "SVGNames.h"

namespace WebCore {
    
using namespace SVGNames;
    
SVGFontFaceSrcElement::SVGFontFaceSrcElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
{
}

PassRefPtr<CSSValueList> SVGFontFaceSrcElement::srcValue() const
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(font_face_uriTag))
            list->append(static_cast<SVGFontFaceUriElement*>(child)->srcValue());
        else if (child->hasTagName(font_face_nameTag))
            list->append(static_cast<SVGFontFaceNameElement*>(child)->srcValue());
    }
    return list;
}

void SVGFontFaceSrcElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (parentNode() && parentNode()->hasTagName(font_faceTag))
        static_cast<SVGFontFaceElement*>(parentNode())->rebuildFontFace();
}

}

#endif // ENABLE(SVG_FONTS)
