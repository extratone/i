/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#include "c_runtime.h"

#include "c_instance.h"
#include "c_utility.h"
#include "npruntime_impl.h"

namespace KJS {
namespace Bindings {

// ---------------------- CMethod ----------------------

const char* CMethod::name() const
{
    PrivateIdentifier *i = (PrivateIdentifier *)_methodIdentifier;
    return i->isString ? i->value.string : 0;
}

// ---------------------- CField ----------------------

const char* CField::name() const
{
    PrivateIdentifier *i = (PrivateIdentifier *)_fieldIdentifier;
    return i->isString ? i->value.string : 0;
}

JSValue* CField::valueFromInstance(ExecState* exec, const Instance* inst) const
{
    const CInstance* instance = static_cast<const CInstance*>(inst);
    NPObject* obj = instance->getObject();
    if (obj->_class->getProperty) {
        NPVariant property;
        VOID_TO_NPVARIANT(property);
        if (obj->_class->getProperty(obj, _fieldIdentifier, &property)) {
            JSValue* result = convertNPVariantToValue(exec, &property);
            _NPN_ReleaseVariantValue(&property);
            return result;
        }
    }
    return jsUndefined();
}

void CField::setValueToInstance(ExecState *exec, const Instance *inst, JSValue *aValue) const
{
    const CInstance* instance = static_cast<const CInstance*>(inst);
    NPObject* obj = instance->getObject();
    if (obj->_class->setProperty) {
        NPVariant variant;
        convertValueToNPVariant(exec, aValue, &variant);
        obj->_class->setProperty(obj, _fieldIdentifier, &variant);
        _NPN_ReleaseVariantValue(&variant);
    }
}

} }
