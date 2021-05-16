/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#include "config.h"
#include "JSHTMLInputElement.h"

#include "HTMLInputElement.h"

using namespace JSC;

namespace WebCore {

bool JSHTMLInputElement::customGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(impl());
    if (input->canHaveSelection())
        return false;

    const HashEntry* entry = JSHTMLInputElementPrototype::s_info.propHashTable(exec)->entry(exec, propertyName);
    if (entry) {
        if (entry->attributes() & Function) {
            if (entry->function() == jsHTMLInputElementPrototypeFunctionSetSelectionRange) {
                slot.setUndefined();
                return true;
            }
        }
    }

    return false;
}

JSValuePtr JSHTMLInputElement::selectionStart(ExecState* exec) const
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(impl());
    if (!input->canHaveSelection())
        return jsUndefined();

    return jsNumber(exec, input->selectionStart());
}

JSValuePtr JSHTMLInputElement::selectionEnd(ExecState* exec) const
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(impl());
    if (!input->canHaveSelection())
        return jsUndefined();

    return jsNumber(exec, input->selectionEnd());
}

} // namespace WebCore
