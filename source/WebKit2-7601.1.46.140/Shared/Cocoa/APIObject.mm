/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "APIObject.h"

#if WK_API_ENABLED

#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextGroupInternal.h"
#import "WKConnectionInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKNSArray.h"
#import "WKNSData.h"
#import "WKNSDictionary.h"
#import "WKNSError.h"
#import "WKNSString.h"
#import "WKNSURL.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKNSURLRequest.h"
#import "WKNavigationActionInternal.h"
#import "WKNavigationDataInternal.h"
#import "WKNavigationInternal.h"
#import "WKNavigationResponseInternal.h"
#import "WKPreferencesInternal.h"
#import "WKProcessPoolInternal.h"
#import "WKSecurityOriginInternal.h"
#import "WKUserContentControllerInternal.h"
#import "WKUserScriptInternal.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import "WKWebProcessPlugInFrameInternal.h"
#import "WKWebProcessPlugInHitTestResultInternal.h"
#import "WKWebProcessPlugInInternal.h"
#import "WKWebProcessPlugInNodeHandleInternal.h"
#import "WKWebProcessPlugInPageGroupInternal.h"
#import "WKWebProcessPlugInScriptWorldInternal.h"
#import "WKWebsiteDataRecordInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "_WKDownloadInternal.h"
#import "_WKFrameHandleInternal.h"
#import "_WKProcessPoolConfigurationInternal.h"
#import "_WKUserContentExtensionStoreInternal.h"
#import "_WKUserContentFilterInternal.h"
#import "_WKVisitedLinkProviderInternal.h"
#import <objc/objc-auto.h>

namespace API {

void Object::ref()
{
    CFRetain(wrapper());
}

void Object::deref()
{
    CFRelease(wrapper());
}

void* Object::newObject(size_t size, Type type)
{
    NSObject <WKObject> *wrapper;

    // Wrappers that inherit from WKObject store the API::Object in their extra bytes, so they are
    // allocated using NSAllocatedObject. The other wrapper classes contain inline storage for the
    // API::Object, so they are allocated using +alloc.

    switch (type) {
    case Type::Array:
        wrapper = [WKNSArray alloc];
        break;

    case Type::AuthenticationChallenge:
        wrapper = NSAllocateObject([WKNSURLAuthenticationChallenge self], size, nullptr);
        break;

    case Type::BackForwardList:
        wrapper = [WKBackForwardList alloc];
        break;

    case Type::BackForwardListItem:
        wrapper = [WKBackForwardListItem alloc];
        break;

    case Type::Bundle:
        wrapper = [WKWebProcessPlugInController alloc];
        break;

    case Type::BundlePage:
        wrapper = [WKWebProcessPlugInBrowserContextController alloc];
        break;

    case Type::Connection:
        wrapper = NSAllocateObject([WKConnection self], size, nullptr);
        break;

    case Type::Preferences:
        wrapper = [WKPreferences alloc];
        break;

    case Type::ProcessPool:
        wrapper = [WKProcessPool alloc];
        break;

    case Type::ProcessPoolConfiguration:
        wrapper = [_WKProcessPoolConfiguration alloc];
        break;

    case Type::Data:
        wrapper = [WKNSData alloc];
        break;

    case Type::Dictionary:
        wrapper = [WKNSDictionary alloc];
        break;

    case Type::Download:
        wrapper = [_WKDownload alloc];
        break;

    case Type::Error:
        wrapper = NSAllocateObject([WKNSError self], size, nullptr);
        break;

    case Type::FrameHandle:
        wrapper = [_WKFrameHandle alloc];
        break;

    case Type::FrameInfo:
        wrapper = [WKFrameInfo alloc];
        break;

    case Type::Navigation:
        wrapper = [WKNavigation alloc];
        break;

    case Type::NavigationAction:
        wrapper = [WKNavigationAction alloc];
        break;

    case Type::NavigationData:
        wrapper = [WKNavigationData alloc];
        break;

    case Type::NavigationResponse:
        wrapper = [WKNavigationResponse alloc];
        break;

    case Type::PageGroup:
        wrapper = [WKBrowsingContextGroup alloc];
        break;

    case Type::SecurityOrigin:
        wrapper = [WKSecurityOrigin alloc];
        break;

    case Type::String:
        wrapper = NSAllocateObject([WKNSString class], size, nullptr);
        break;

    case Type::URL:
        wrapper = NSAllocateObject([WKNSURL class], size, nullptr);
        break;

    case Type::URLRequest:
        wrapper = NSAllocateObject([WKNSURLRequest class], size, nullptr);
        break;

    case Type::UserContentController:
        wrapper = [WKUserContentController alloc];
        break;

    case Type::UserContentExtension:
        wrapper = [_WKUserContentFilter alloc];
        break;

    case Type::UserContentExtensionStore:
        wrapper = [_WKUserContentExtensionStore alloc];
        break;

    case Type::UserScript:
        wrapper = [WKUserScript alloc];
        break;

    case Type::VisitedLinkProvider:
        wrapper = [_WKVisitedLinkProvider alloc];
        break;

    case Type::WebsiteDataRecord:
        wrapper = [WKWebsiteDataRecord alloc];
        break;

    case Type::WebsiteDataStore:
        wrapper = [WKWebsiteDataStore alloc];
        break;

    case Type::BundleFrame:
        wrapper = [WKWebProcessPlugInFrame alloc];
        break;

    case Type::BundleHitTestResult:
        wrapper = [WKWebProcessPlugInHitTestResult alloc];
        break;

    case Type::BundleNodeHandle:
        wrapper = [WKWebProcessPlugInNodeHandle alloc];
        break;

    case Type::BundlePageGroup:
        wrapper = [WKWebProcessPlugInPageGroup alloc];
        break;

    case Type::BundleScriptWorld:
        wrapper = [WKWebProcessPlugInScriptWorld alloc];
        break;

    default:
        wrapper = NSAllocateObject([WKObject class], size, nullptr);
        break;
    }

    Object& object = wrapper._apiObject;
    object.m_wrapper = wrapper;

#if PLATFORM(MAC)
    if (objc_collectingEnabled())
        object.ref();
#endif

    return &object;
}

void* Object::wrap(API::Object* object)
{
    if (!object)
        return nullptr;

    return static_cast<void*>(object->wrapper());
}

API::Object* Object::unwrap(void* object)
{
    if (!object)
        return nullptr;

    ASSERT([(id)object conformsToProtocol:@protocol(WKObject)]);
    ASSERT([(id)object respondsToSelector:@selector(_apiObject)]);

    return &static_cast<id <WKObject>>(object)._apiObject;
}

} // namespace API

#endif // WK_API_ENABLED
