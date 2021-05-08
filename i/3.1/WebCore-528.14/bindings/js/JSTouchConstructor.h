/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 * No license or rights are granted by Apple expressly or by implication,
 * estoppel, or otherwise, to Apple copyrights, patents, trademarks, trade
 * secrets or other rights.
 */

#ifndef JSTouchConstructor_h
#define JSTouchConstructor_h

#include <wtf/Platform.h>

#if ENABLE(TOUCH_EVENTS)

#include "JSDOMBinding.h"
#include "JSDocument.h"

namespace WebCore {

class JSTouchConstructor : public DOMObject {
public:
    JSTouchConstructor(JSC::ExecState*, ScriptExecutionContext*);
    static const JSC::ClassInfo s_info;

private:
    virtual JSC::ConstructType getConstructData(JSC::ConstructData&);
    virtual const JSC::ClassInfo* classInfo() const { return &s_info; }
};

}

#endif // ENABLE(TOUCH_EVENTS)

#endif // JSTouchConstructor_h
