/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 * Permission is granted by Apple to use this file to the extent
 * necessary to relink with LGPL WebKit files.
 *
 * No license or rights are granted by Apple expressly or by
 * implication, estoppel, or otherwise, to Apple patents and
 * trademarks. For the sake of clarity, no license or rights are
 * granted by Apple expressly or by implication, estoppel, or otherwise,
 * under any Apple patents, copyrights and trademarks to underlying
 * implementations of any application programming interfaces (APIs)
 * or to any functionality that is invoked by calling any API.
 */

#ifndef JSTouchConstructor_h
#define JSTouchConstructor_h

#include <wtf/Platform.h>

#if ENABLE(TOUCH_EVENTS)

#include "JSDOMBinding.h"
#include "JSDocument.h"

namespace WebCore {

class JSTouchConstructor : public DOMConstructorWithDocument {
public:
    typedef DOMConstructorWithDocument Base;

    static JSTouchConstructor* create(JSC::ExecState* exec, JSC::Structure* structure, JSDOMGlobalObject* globalObject)
    {
        JSTouchConstructor* constructor = new (JSC::allocateCell<JSTouchConstructor>(*exec->heap())) JSTouchConstructor(structure, globalObject);
        constructor->finishCreation(exec, globalObject);
        return constructor;
    }

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), &s_info);
    }

    static const JSC::ClassInfo s_info;

private:
    JSTouchConstructor(JSC::Structure*, JSDOMGlobalObject*);
    void finishCreation(JSC::ExecState*, JSDOMGlobalObject*);    
    static JSC::ConstructType getConstructData(JSC::JSCell*, JSC::ConstructData&);
};

}

#endif // ENABLE(TOUCH_EVENTS)

#endif // JSTouchConstructor_h
