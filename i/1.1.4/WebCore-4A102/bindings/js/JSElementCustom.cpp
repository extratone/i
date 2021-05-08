/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "JSElement.h"

#include "Attr.h"
#include "Document.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "HTMLFrameElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "PlatformString.h"
#include "kjs_binding.h"
#include "kjs_dom.h"

namespace WebCore {

using namespace HTMLNames;

static inline bool allowSettingSrcToJavascriptURL(KJS::ExecState* exec, Element* element, String name, String value)
{
    if (element->hasTagName(iframeTag) && equalIgnoringCase(name, "src") && value.startsWith("javascript:", false)) {
        HTMLIFrameElement* frame = static_cast<HTMLIFrameElement*>(element);
        if (!checkNodeSecurity(exec, frame->contentDocument()))
            return false;
    }
    if (element->hasTagName(frameTag) && equalIgnoringCase(name, "src") && value.startsWith("javascript:", false)) {
        HTMLFrameElement* frame = static_cast<HTMLFrameElement*>(element);
        if (!checkNodeSecurity(exec, frame->contentDocument()))
            return false;
    }
    return true;
} 

KJS::JSValue* JSElement::setAttribute(KJS::ExecState* exec, const KJS::List& args)
{
    ExceptionCode ec = 0;
    String name = args[0]->toString(exec);
    String value = args[1]->toString(exec);

    Element* imp = static_cast<Element*>(impl());
    if (!allowSettingSrcToJavascriptURL(exec, imp, name, value))
        return KJS::jsUndefined();

    imp->setAttribute(name, value, ec);
    KJS::setDOMException(exec, ec);
    return KJS::jsUndefined();
}

KJS::JSValue* JSElement::setAttributeNode(KJS::ExecState* exec, const KJS::List& args)
{
    ExceptionCode ec = 0;
    bool newAttrOk;
    Attr* newAttr = toAttr(args[0], newAttrOk);
    if (!newAttrOk) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return KJS::jsUndefined();
    }

    Element* imp = static_cast<Element*>(impl());
    if (!allowSettingSrcToJavascriptURL(exec, imp, newAttr->name(), newAttr->value()))
        return KJS::jsUndefined();

    KJS::JSValue* result = toJS(exec, WTF::getPtr(imp->setAttributeNode(newAttr, ec)));
    KJS::setDOMException(exec, ec);
    return result;
}

KJS::JSValue* JSElement::setAttributeNS(KJS::ExecState* exec, const KJS::List& args)
{
    ExceptionCode ec = 0;
    String namespaceURI = valueToStringWithNullCheck(exec, args[0]);
    String qualifiedName = args[1]->toString(exec);
    String value = args[2]->toString(exec);

    Element* imp = static_cast<Element*>(impl());
    if (!allowSettingSrcToJavascriptURL(exec, imp, qualifiedName, value))
        return KJS::jsUndefined();

    imp->setAttributeNS(namespaceURI, qualifiedName, value, ec);
    KJS::setDOMException(exec, ec);
    return KJS::jsUndefined();
}

KJS::JSValue* JSElement::setAttributeNodeNS(KJS::ExecState* exec, const KJS::List& args)
{
    ExceptionCode ec = 0;
    bool newAttrOk;
    Attr* newAttr = toAttr(args[0], newAttrOk);
    if (!newAttrOk) {
        KJS::setDOMException(exec, TYPE_MISMATCH_ERR);
        return KJS::jsUndefined();
    }

    Element* imp = static_cast<Element*>(impl());
    if (!allowSettingSrcToJavascriptURL(exec, imp, newAttr->name(), newAttr->value()))
        return KJS::jsUndefined();

    KJS::JSValue* result = toJS(exec, WTF::getPtr(imp->setAttributeNodeNS(newAttr, ec)));
    KJS::setDOMException(exec, ec);
    return result;
}

} // namespace WebCore
