/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SecItemShim.h"

#if ENABLE(SEC_ITEM_SHIM)

#include "BlockingResponseMap.h"
#include "ChildProcess.h"
#include "SecItemRequestData.h"
#include "SecItemResponseData.h"
#include "SecItemShimLibrary.h"
#include "SecItemShimMessages.h"
#include "SecItemShimProxyMessages.h"
#include <Security/Security.h>
#include <atomic>
#include <dlfcn.h>
#include <mutex>
#include <wtf/NeverDestroyed.h>

#if __has_include(<CFNetwork/CFURLConnectionPriv.h>)
#include <CFNetwork/CFURLConnectionPriv.h>
#else
struct _CFNFrameworksStubs {
    CFIndex version;

    OSStatus (*SecItem_stub_CopyMatching)(CFDictionaryRef query, CFTypeRef *result);
    OSStatus (*SecItem_stub_Add)(CFDictionaryRef attributes, CFTypeRef *result);
    OSStatus (*SecItem_stub_Update)(CFDictionaryRef query, CFDictionaryRef attributesToUpdate);
    OSStatus (*SecItem_stub_Delete)(CFDictionaryRef query);
};
#endif

extern "C" void _CFURLConnectionSetFrameworkStubs(const struct _CFNFrameworksStubs* stubs);

namespace WebKit {

static BlockingResponseMap<SecItemResponseData>& responseMap()
{
    static std::once_flag onceFlag;
    static LazyNeverDestroyed<BlockingResponseMap<SecItemResponseData>> responseMap;

    std::call_once(onceFlag, [] {
        responseMap.construct();
    });

    return responseMap;
}

static ChildProcess* sharedProcess;

SecItemShim& SecItemShim::singleton()
{
    static SecItemShim* shim;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        shim = adoptRef(new SecItemShim).leakRef();
    });

    return *shim;
}

SecItemShim::SecItemShim()
    : m_queue(WorkQueue::create("com.apple.WebKit.SecItemShim"))
{
}

static uint64_t generateSecItemRequestID()
{
    static std::atomic<int64_t> uniqueSecItemRequestID;
    return ++uniqueSecItemRequestID;
}

static std::unique_ptr<SecItemResponseData> sendSecItemRequest(SecItemRequestData::Type requestType, CFDictionaryRef query, CFDictionaryRef attributesToMatch = 0)
{
    uint64_t requestID = generateSecItemRequestID();
    if (!sharedProcess->parentProcessConnection()->send(Messages::SecItemShimProxy::SecItemRequest(requestID, SecItemRequestData(requestType, query, attributesToMatch)), 0))
        return nullptr;

    return responseMap().waitForResponse(requestID);
}

static OSStatus webSecItemCopyMatching(CFDictionaryRef query, CFTypeRef* result)
{
    std::unique_ptr<SecItemResponseData> response = sendSecItemRequest(SecItemRequestData::CopyMatching, query);
    if (!response)
        return errSecInteractionNotAllowed;

    *result = response->resultObject().leakRef();
    return response->resultCode();
}

static OSStatus webSecItemAdd(CFDictionaryRef query, CFTypeRef* result)
{
    std::unique_ptr<SecItemResponseData> response = sendSecItemRequest(SecItemRequestData::Add, query);
    if (!response)
        return errSecInteractionNotAllowed;

    if (result)
        *result = response->resultObject().leakRef();
    return response->resultCode();
}

static OSStatus webSecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
{
    std::unique_ptr<SecItemResponseData> response = sendSecItemRequest(SecItemRequestData::Update, query, attributesToUpdate);
    if (!response)
        return errSecInteractionNotAllowed;
    
    return response->resultCode();
}

static OSStatus webSecItemDelete(CFDictionaryRef query)
{
    std::unique_ptr<SecItemResponseData> response = sendSecItemRequest(SecItemRequestData::Delete, query);
    if (!response)
        return errSecInteractionNotAllowed;
    
    return response->resultCode();
}

void SecItemShim::secItemResponse(uint64_t requestID, const SecItemResponseData& response)
{
    responseMap().didReceiveResponse(requestID, std::make_unique<SecItemResponseData>(response));
}

void SecItemShim::initialize(ChildProcess* process)
{
    sharedProcess = process;

#if PLATFORM(IOS)
    struct _CFNFrameworksStubs stubs = {
        .version = 0,
        .SecItem_stub_CopyMatching = webSecItemCopyMatching,
        .SecItem_stub_Add = webSecItemAdd,
        .SecItem_stub_Update = webSecItemUpdate,
        .SecItem_stub_Delete = webSecItemDelete,
    };

    _CFURLConnectionSetFrameworkStubs(&stubs);
#endif

#if PLATFORM(MAC)
    const SecItemShimCallbacks callbacks = {
        webSecItemCopyMatching,
        webSecItemAdd,
        webSecItemUpdate,
        webSecItemDelete
    };
    
    SecItemShimInitializeFunc func = reinterpret_cast<SecItemShimInitializeFunc>(dlsym(RTLD_DEFAULT, "WebKitSecItemShimInitialize"));
    func(callbacks);
#endif
}

void SecItemShim::initializeConnection(IPC::Connection* connection)
{
    connection->addWorkQueueMessageReceiver(Messages::SecItemShim::messageReceiverName(), &m_queue.get(), this);
}

} // namespace WebKit

#endif // ENABLE(SEC_ITEM_SHIM)
