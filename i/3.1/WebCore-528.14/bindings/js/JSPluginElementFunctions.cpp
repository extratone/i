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
#include "JSPluginElementFunctions.h"

#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSHTMLElement.h"
#include "ScriptController.h"
#include "runtime.h"
#include "runtime_object.h"

using namespace JSC;

namespace WebCore {

using namespace Bindings;
using namespace HTMLNames;

// Runtime object support code for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.

static Instance* pluginInstance(Node* node)
{
    if (!node)
        return 0;
    if (!(node->hasTagName(objectTag) || node->hasTagName(embedTag) || node->hasTagName(appletTag)))
        return 0;
    HTMLPlugInElement* plugInElement = static_cast<HTMLPlugInElement*>(node);
    // The plugin element holds an owning reference, so we don't have to.
    Instance* instance = plugInElement->getInstance().get();
    if (!instance || !instance->rootObject())
        return 0;
    return instance;
}

static RuntimeObjectImp* getRuntimeObject(ExecState* exec, Node* node)
{
    Instance* instance = pluginInstance(node);
    if (!instance)
        return 0;
    return instance->createRuntimeObject(exec);
}

JSValuePtr runtimeObjectGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(asObject(slot.slotBase()));
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());
    RuntimeObjectImp* runtimeObject = getRuntimeObject(exec, element);
    return runtimeObject ? runtimeObject : jsUndefined();
}

JSValuePtr runtimeObjectPropertyGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(asObject(slot.slotBase()));
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());
    RuntimeObjectImp* runtimeObject = getRuntimeObject(exec, element);
    if (!runtimeObject)
        return jsUndefined();
    return runtimeObject->get(exec, propertyName);
}

bool runtimeObjectCustomGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, JSHTMLElement* element)
{
    RuntimeObjectImp* runtimeObject = getRuntimeObject(exec, element->impl());
    if (!runtimeObject)
        return false;
    if (!runtimeObject->hasProperty(exec, propertyName))
        return false;
    slot.setCustom(element, runtimeObjectPropertyGetter);
    return true;
}

bool runtimeObjectCustomPut(ExecState* exec, const Identifier& propertyName, JSValuePtr value, HTMLElement* element, PutPropertySlot& slot)
{
    RuntimeObjectImp* runtimeObject = getRuntimeObject(exec, element);
    if (!runtimeObject)
        return 0;
    if (!runtimeObject->hasProperty(exec, propertyName))
        return false;
    runtimeObject->put(exec, propertyName, value, slot);
    return true;
}

static JSValuePtr callPlugin(ExecState* exec, JSObject* function, JSValuePtr, const ArgList& args)
{
    Instance* instance = pluginInstance(static_cast<JSHTMLElement*>(function)->impl());
    instance->begin();
    JSValuePtr result = instance->invokeDefaultMethod(exec, args);
    instance->end();
    return result;
}

CallType runtimeObjectGetCallData(HTMLElement* element, CallData& callData)
{
    Instance* instance = pluginInstance(element);
    if (!instance || !instance->supportsInvokeDefaultMethod())
        return CallTypeNone;
    callData.native.function = callPlugin;
    return CallTypeHost;
}

} // namespace WebCore
