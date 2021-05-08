/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSImageConstructor.h"

#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "JSHTMLImageElement.h"
#include "JSNode.h"
#include "ScriptExecutionContext.h"

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSImageConstructor)

const ClassInfo JSImageConstructor::s_info = { "ImageConstructor", 0, 0, 0 };

JSImageConstructor::JSImageConstructor(ExecState* exec, ScriptExecutionContext* context)
    : DOMObject(JSImageConstructor::createStructure(exec->lexicalGlobalObject()->objectPrototype()))
{
    ASSERT(context->isDocument());
    m_document = static_cast<JSDocument*>(asObject(toJS(exec, static_cast<Document*>(context))));
    putDirect(exec->propertyNames().prototype, JSHTMLImageElementPrototype::self(exec, exec->lexicalGlobalObject()), None);
}

static JSObject* constructImage(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    bool widthSet = false;
    bool heightSet = false;
    int width = 0;
    int height = 0;
    if (args.size() > 0) {
        widthSet = true;
        width = args.at(exec, 0).toInt32(exec);
    }
    if (args.size() > 1) {
        heightSet = true;
        height = args.at(exec, 1).toInt32(exec);
    }

    Document* document = static_cast<JSImageConstructor*>(constructor)->document();

    // Calling toJS on the document causes the JS document wrapper to be
    // added to the window object. This is done to ensure that JSDocument::mark
    // will be called (which will cause the image element to be marked if necessary).
    toJS(exec, document);

    RefPtr<HTMLImageElement> image = new HTMLImageElement(HTMLNames::imgTag, document);
    if (widthSet)
        image->setWidth(width);
    if (heightSet)
        image->setHeight(height);
    return asObject(toJS(exec, image.release()));
}

ConstructType JSImageConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructImage;
    return ConstructTypeHost;
}

void JSImageConstructor::mark()
{
    DOMObject::mark();
    if (!m_document->marked())
        m_document->mark();
}

} // namespace WebCore
