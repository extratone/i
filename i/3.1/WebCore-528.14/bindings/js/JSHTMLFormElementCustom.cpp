/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSHTMLFormElement.h"

#include "Frame.h"
#include "HTMLCollection.h"
#include "HTMLFormElement.h"
#include "JSDOMWindowCustom.h"
#include "JSNamedNodesCollection.h"

using namespace JSC;

namespace WebCore {

bool JSHTMLFormElement::canGetItemsForName(ExecState*, HTMLFormElement* form, const Identifier& propertyName)
{
    Vector<RefPtr<Node> > namedItems;
    form->getNamedElements(propertyName, namedItems);
    return namedItems.size();
}

JSValuePtr JSHTMLFormElement::nameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    HTMLFormElement* form = static_cast<HTMLFormElement*>(static_cast<JSHTMLElement*>(asObject(slot.slotBase()))->impl());
    
    Vector<RefPtr<Node> > namedItems;
    form->getNamedElements(propertyName, namedItems);
    
    if (namedItems.size() == 1)
        return toJS(exec, namedItems[0].get());
    if (namedItems.size() > 1) 
        return new (exec) JSNamedNodesCollection(exec, namedItems);
    return jsUndefined();
}

JSValuePtr JSHTMLFormElement::submit(ExecState* exec, const ArgList&)
{
    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!activeFrame)
        return jsUndefined();
    static_cast<HTMLFormElement*>(impl())->submit(0, false, !activeFrame->script()->anyPageIsProcessingUserGesture(), false);
    return jsUndefined();
}

}
