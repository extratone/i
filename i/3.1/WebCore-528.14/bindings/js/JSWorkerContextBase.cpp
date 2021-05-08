/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"

#if ENABLE(WORKERS)

#include "JSWorkerContextBase.h"

#include "Event.h"
#include "JSDOMBinding.h"
#include "JSEventListener.h"
#include "JSMessageChannelConstructor.h"
#include "JSMessageEvent.h"
#include "JSMessagePort.h"
#include "JSWorkerLocation.h"
#include "JSWorkerNavigator.h"
#include "WorkerContext.h"
#include "WorkerLocation.h"

using namespace JSC;

/*
@begin JSWorkerContextBaseTable
@end
*/

#include "JSWorkerContextBase.lut.h"

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSWorkerContextBase)

JSWorkerContextBase::JSWorkerContextBase(PassRefPtr<JSC::Structure> structure, PassRefPtr<WorkerContext> impl)
    : JSDOMGlobalObject(structure, new JSDOMGlobalObjectData, this)
    , m_impl(impl)
{
}

JSWorkerContextBase::~JSWorkerContextBase()
{
}

ScriptExecutionContext* JSWorkerContextBase::scriptExecutionContext() const
{
    return m_impl.get();
}

static const HashTable* getJSWorkerContextBaseTable(ExecState* exec)
{
    return getHashTableForGlobalData(exec->globalData(), &JSWorkerContextBaseTable);
}

const ClassInfo JSWorkerContextBase::s_info = { "WorkerContext", 0, 0, getJSWorkerContextBaseTable };

void JSWorkerContextBase::put(ExecState* exec, const Identifier& propertyName, JSValuePtr value, PutPropertySlot& slot)
{
    lookupPut<JSWorkerContextBase, Base>(exec, propertyName, value, getJSWorkerContextBaseTable(exec), this, slot);
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
