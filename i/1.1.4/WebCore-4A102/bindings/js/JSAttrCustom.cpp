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
#include "JSAttr.h"

#include "Attr.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "HTMLFrameElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "PlatformString.h"
#include "kjs_binding.h"
#include "kjs_dom.h"

namespace WebCore {

using namespace HTMLNames;

void JSAttr::setValue(KJS::ExecState* exec, KJS::JSValue* value)
{
    Attr* imp = static_cast<Attr*>(impl());
    String attrValue = valueToStringWithNullCheck(exec, value);

    Element* ownerElement = imp->ownerElement();
    if (ownerElement && ownerElement->hasTagName(iframeTag) && equalIgnoringCase(imp->name(), "src") && attrValue.startsWith("javascript:", false)) {
        HTMLIFrameElement* frame = static_cast<HTMLIFrameElement*>(ownerElement);
        if (!checkNodeSecurity(exec, frame->contentDocument()))
            return;
    }
    if (ownerElement && ownerElement->hasTagName(frameTag) && equalIgnoringCase(imp->name(), "src") && attrValue.startsWith("javascript:", false)) {
        HTMLFrameElement* frame = static_cast<HTMLFrameElement*>(ownerElement);
        if (!checkNodeSecurity(exec, frame->contentDocument()))
            return;
    }

    ExceptionCode ec = 0;
    imp->setValue(attrValue, ec);
    KJS::setDOMException(exec, ec);
}

} // namespace WebCore
