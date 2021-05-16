/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#if ENABLE(XSLT)

#include "JSXSLTProcessorConstructor.h"

#include "JSXSLTProcessor.h"
#include "XSLTProcessor.h"
#include <wtf/RefPtr.h>

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSXSLTProcessorConstructor)

const ClassInfo JSXSLTProcessorConstructor::s_info = { "XSLTProcessorConsructor", 0, 0, 0 };

JSXSLTProcessorConstructor::JSXSLTProcessorConstructor(ExecState* exec)
    : DOMObject(JSXSLTProcessorConstructor::createStructure(exec->lexicalGlobalObject()->objectPrototype()))
{
    putDirect(exec->propertyNames().prototype, JSXSLTProcessorPrototype::self(exec, exec->lexicalGlobalObject()), None);
}

static JSObject* constructXSLTProcessor(ExecState* exec, JSObject*, const ArgList&)
{
    return CREATE_DOM_OBJECT_WRAPPER(exec, XSLTProcessor, XSLTProcessor::create().get());
}

ConstructType JSXSLTProcessorConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructXSLTProcessor;
    return ConstructTypeHost;
}

} // namespace WebCore

#endif // ENABLE(XSLT)
