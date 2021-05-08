/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSHTMLDocument.h"

#include "Frame.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowCustom.h"
#include "JSDOMWindowShell.h"
#include "JSHTMLCollection.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

using namespace HTMLNames;

bool JSHTMLDocument::canGetItemsForName(ExecState*, HTMLDocument* document, const Identifier& propertyName)
{
    AtomicStringImpl* atomicPropertyName = AtomicString::find(propertyName);
    return atomicPropertyName && (document->hasNamedItem(atomicPropertyName) || document->hasExtraNamedItem(atomicPropertyName));
}

JSValuePtr JSHTMLDocument::nameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLDocument* thisObj = static_cast<JSHTMLDocument*>(asObject(slot.slotBase()));
    HTMLDocument* document = static_cast<HTMLDocument*>(thisObj->impl());

    String name = propertyName;
    RefPtr<HTMLCollection> collection = document->documentNamedItems(name);

    unsigned length = collection->length();
    if (!length)
        return jsUndefined();

    if (length == 1) {
        Node* node = collection->firstItem();

        Frame* frame;
        if (node->hasTagName(iframeTag) && (frame = static_cast<HTMLIFrameElement*>(node)->contentFrame()))
            return toJS(exec, frame);

        return toJS(exec, node);
    } 

    return toJS(exec, collection.get());
}

// Custom attributes

JSValuePtr JSHTMLDocument::all(ExecState* exec) const
{
    // If "all" has been overwritten, return the overwritten value
    JSValuePtr v = getDirect(Identifier(exec, "all"));
    if (v)
        return v;

    return toJS(exec, static_cast<HTMLDocument*>(impl())->all().get());
}

void JSHTMLDocument::setAll(ExecState* exec, JSValuePtr value)
{
    // Add "all" to the property map.
    putDirect(Identifier(exec, "all"), value);
}

// Custom functions

JSValuePtr JSHTMLDocument::open(ExecState* exec, const ArgList& args)
{
    // For compatibility with other browsers, pass open calls with more than 2 parameters to the window.
    if (args.size() > 2) {
        Frame* frame = static_cast<HTMLDocument*>(impl())->frame();
        if (frame) {
            JSDOMWindowShell* wrapper = toJSDOMWindowShell(frame);
            if (wrapper) {
                JSValuePtr function = wrapper->get(exec, Identifier(exec, "open"));
                CallData callData;
                CallType callType = function.getCallData(callData);
                if (callType == CallTypeNone)
                    return throwError(exec, TypeError);
                return call(exec, function, callType, callData, wrapper, args);
            }
        }
        return jsUndefined();
    }

    // document.open clobbers the security context of the document and
    // aliases it with the active security context.
    Document* activeDocument = asJSDOMWindow(exec->lexicalGlobalObject())->impl()->document();

    // In the case of two parameters or fewer, do a normal document open.
    static_cast<HTMLDocument*>(impl())->open(activeDocument);
    return this;
}

static String writeHelper(ExecState* exec, const ArgList& args)
{
    // DOM only specifies single string argument, but NS & IE allow multiple
    // or no arguments.

    unsigned size = args.size();
    if (size == 1)
        return args.at(exec, 0).toString(exec);

    Vector<UChar> result;
    for (unsigned i = 0; i < size; ++i)
        append(result, args.at(exec, i).toString(exec));
    return String::adopt(result);
}

JSValuePtr JSHTMLDocument::write(ExecState* exec, const ArgList& args)
{
    Document* activeDocument = asJSDOMWindow(exec->lexicalGlobalObject())->impl()->document();
    static_cast<HTMLDocument*>(impl())->write(writeHelper(exec, args), activeDocument);
    return jsUndefined();
}

JSValuePtr JSHTMLDocument::writeln(ExecState* exec, const ArgList& args)
{
    Document* activeDocument = asJSDOMWindow(exec->lexicalGlobalObject())->impl()->document();
    static_cast<HTMLDocument*>(impl())->write(writeHelper(exec, args) + "\n", activeDocument);
    return jsUndefined();
}

} // namespace WebCore
