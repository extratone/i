/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "WebCoreObjCExtras.h"

#include <objc/objc-auto.h>
#include <objc/objc-runtime.h>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>
#include <wtf/UnusedParam.h>

void WebCoreObjCFinalizeOnMainThread(Class cls)
{
    // This method relies on threading being initialized by the caller, otherwise
    // WebCoreObjCScheduleDeallocateOnMainThread will crash.
#if !defined(BUILDING_ON_TIGER) && !defined(DONT_FINALIZE_ON_MAIN_THREAD)
    objc_finalizeOnMainThread(cls);
#else
    UNUSED_PARAM(cls);
#endif
}

#ifdef BUILDING_ON_TIGER
static inline IMP method_getImplementation(Method method) 
{
    return method->method_imp;
}
#endif

typedef std::pair<Class, id> ClassAndIdPair;


bool WebCoreObjCScheduleDeallocateOnMainThread(Class cls, id object)
{
    UNUSED_PARAM(cls);
    UNUSED_PARAM(object);
    return false;
}

