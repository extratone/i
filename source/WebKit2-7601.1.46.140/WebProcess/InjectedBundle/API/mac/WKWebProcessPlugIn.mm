/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKWebProcessPlugInInternal.h"

#if WK_API_ENABLED

#import "WKConnectionInternal.h"
#import "WKBundle.h"
#import "WKBundleAPICast.h"
#import "WKRetainPtr.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import <wtf/RetainPtr.h>

using namespace WebKit;

@interface WKWebProcessPlugInController () {
    API::ObjectStorage<InjectedBundle> _bundle;
    RetainPtr<id <WKWebProcessPlugIn>> _principalClassInstance;
}
@end

@implementation WKWebProcessPlugInController

- (void)dealloc
{
    _bundle->~InjectedBundle();

    [super dealloc];
}

static void didCreatePage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    WKWebProcessPlugInController *plugInController = (WKWebProcessPlugInController *)clientInfo;
    id <WKWebProcessPlugIn> principalClassInstance = plugInController->_principalClassInstance.get();

    if ([principalClassInstance respondsToSelector:@selector(webProcessPlugIn:didCreateBrowserContextController:)])
        [principalClassInstance webProcessPlugIn:plugInController didCreateBrowserContextController:wrapper(*toImpl(page))];
}

static void willDestroyPage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    WKWebProcessPlugInController *plugInController = (WKWebProcessPlugInController *)clientInfo;
    id <WKWebProcessPlugIn> principalClassInstance = plugInController->_principalClassInstance.get();

    if ([principalClassInstance respondsToSelector:@selector(webProcessPlugIn:willDestroyBrowserContextController:)])
        [principalClassInstance webProcessPlugIn:plugInController willDestroyBrowserContextController:wrapper(*toImpl(page))];
}

static void setUpBundleClient(WKWebProcessPlugInController *plugInController, InjectedBundle& bundle)
{
    WKBundleClientV1 bundleClient;
    memset(&bundleClient, 0, sizeof(bundleClient));

    bundleClient.base.version = 1;
    bundleClient.base.clientInfo = plugInController;
    bundleClient.didCreatePage = didCreatePage;
    bundleClient.willDestroyPage = willDestroyPage;

    bundle.initializeClient(&bundleClient.base);
}

- (void)_setPrincipalClassInstance:(id <WKWebProcessPlugIn>)principalClassInstance
{
    ASSERT(!_principalClassInstance);
    _principalClassInstance = principalClassInstance;

    setUpBundleClient(self, *_bundle);
}

- (WKConnection *)connection
{
    return wrapper(*_bundle->webConnectionToUIProcess());
}

- (id)parameters
{
    return _bundle->bundleParameters();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_bundle;
}

@end

@implementation WKWebProcessPlugInController (Private)

- (WKBundleRef)_bundleRef
{
    return toAPI(_bundle.get());
}

@end

#endif // WK_API_ENABLED
