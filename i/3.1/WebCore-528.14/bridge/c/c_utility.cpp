/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "c_utility.h"

#include "JSDOMWindow.h"
#include "NP_jsobject.h"
#include "c_instance.h"
#include <runtime/JSGlobalObject.h>
#include <runtime/JSLock.h>
#include "PlatformString.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"
#include "runtime_object.h"
#include "runtime_root.h"
#include <wtf/Assertions.h>

using WebCore::String;

namespace JSC { namespace Bindings {

static String convertUTF8ToUTF16WithLatin1Fallback(const NPUTF8* UTF8Chars, int UTF8Length)
{
    ASSERT(UTF8Chars || UTF8Length == 0);
    
    if (UTF8Length == -1)
        UTF8Length = static_cast<int>(strlen(UTF8Chars));

    String result = String::fromUTF8(UTF8Chars, UTF8Length);
    
    // If we got back a null string indicating an unsuccessful conversion, fall back to latin 1.
    // Some plugins return invalid UTF-8 in NPVariantType_String, see <http://bugs.webkit.org/show_bug.cgi?id=5163>
    // There is no "bad data" for latin1. It is unlikely that the plugin was really sending text in this encoding,
    // but it should have used UTF-8, and now we are simply avoiding a crash.
    if (result.isNull())
        result = String(UTF8Chars, UTF8Length);
    
    return result;
}

// Variant value must be released with NPReleaseVariantValue()
void convertValueToNPVariant(ExecState* exec, JSValuePtr value, NPVariant* result)
{
    JSLock lock(false);

    VOID_TO_NPVARIANT(*result);

    if (value.isString()) {
        UString ustring = value.toString(exec);
        CString cstring = ustring.UTF8String();
        NPString string = { (const NPUTF8*)cstring.c_str(), static_cast<uint32_t>(cstring.size()) };
        NPN_InitializeVariantWithStringCopy(result, &string);
    } else if (value.isNumber()) {
        DOUBLE_TO_NPVARIANT(value.toNumber(exec), *result);
    } else if (value.isBoolean()) {
        BOOLEAN_TO_NPVARIANT(value.toBoolean(exec), *result);
    } else if (value.isNull()) {
        NULL_TO_NPVARIANT(*result);
    } else if (value.isObject()) {
        JSObject* object = asObject(value);
        if (object->classInfo() == &RuntimeObjectImp::s_info) {
            RuntimeObjectImp* imp = static_cast<RuntimeObjectImp*>(object);
            CInstance* instance = static_cast<CInstance*>(imp->getInternalInstance());
            if (instance) {
                NPObject* obj = instance->getObject();
                _NPN_RetainObject(obj);
                OBJECT_TO_NPVARIANT(obj, *result);
            }
        } else {
            JSGlobalObject* globalObject = exec->dynamicGlobalObject();

            RootObject* rootObject = findRootObject(globalObject);
            if (rootObject) {
                NPObject* npObject = _NPN_CreateScriptObject(0, object, rootObject);
                OBJECT_TO_NPVARIANT(npObject, *result);
            }
        }
    }
}

JSValuePtr convertNPVariantToValue(ExecState* exec, const NPVariant* variant, RootObject* rootObject)
{
    JSLock lock(false);
    
    NPVariantType type = variant->type;

    if (type == NPVariantType_Bool)
        return jsBoolean(NPVARIANT_TO_BOOLEAN(*variant));
    if (type == NPVariantType_Null)
        return jsNull();
    if (type == NPVariantType_Void)
        return jsUndefined();
    if (type == NPVariantType_Int32)
        return jsNumber(exec, NPVARIANT_TO_INT32(*variant));
    if (type == NPVariantType_Double)
        return jsNumber(exec, NPVARIANT_TO_DOUBLE(*variant));
    if (type == NPVariantType_String)
        return jsString(exec, convertNPStringToUTF16(&variant->value.stringValue));
    if (type == NPVariantType_Object) {
        NPObject* obj = variant->value.objectValue;
        
        if (obj->_class == NPScriptObjectClass)
            // Get JSObject from NP_JavaScriptObject.
            return ((JavaScriptObject*)obj)->imp;

        // Wrap NPObject in a CInstance.
        return CInstance::create(obj, rootObject)->createRuntimeObject(exec);
    }
    
    return jsUndefined();
}

String convertNPStringToUTF16(const NPString* string)
{
    return convertUTF8ToUTF16WithLatin1Fallback(string->UTF8Characters, string->UTF8Length);
}

Identifier identifierFromNPIdentifier(const NPUTF8* name)
{
    return Identifier(WebCore::JSDOMWindow::commonJSGlobalData(), convertUTF8ToUTF16WithLatin1Fallback(name, -1));
}

} }

#endif // ENABLE(NETSCAPE_PLUGIN_API)
