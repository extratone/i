/*
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSPluginElementFunctions_h
#define JSPluginElementFunctions_h

#include "JSDOMBinding.h"

namespace WebCore {

    class HTMLElement;
    class JSHTMLElement;
    class Node;

    // Runtime object support code for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.

    JSC::JSValuePtr runtimeObjectGetter(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
    JSC::JSValuePtr runtimeObjectPropertyGetter(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
    bool runtimeObjectCustomGetOwnPropertySlot(JSC::ExecState*, const JSC::Identifier&, JSC::PropertySlot&, JSHTMLElement*);
    bool runtimeObjectCustomPut(JSC::ExecState*, const JSC::Identifier&, JSC::JSValuePtr, HTMLElement*, JSC::PutPropertySlot&);
    JSC::CallType runtimeObjectGetCallData(HTMLElement*, JSC::CallData&);

} // namespace WebCore

#endif // JSPluginElementFunctions_h
