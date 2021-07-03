/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef APILoaderClient_h
#define APILoaderClient_h

#include "APIData.h"
#include "PluginModuleInfo.h"
#include "SameDocumentNavigationType.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/LayoutMilestones.h>
#include <wtf/Forward.h>

namespace WebCore {
class ResourceError;
}

namespace WebKit {
class AuthenticationChallengeProxy;
class QuickLookDocumentData;
class WebBackForwardListItem;
class WebFrameProxy;
class WebPageProxy;
class WebProtectionSpace;
struct WebNavigationDataStore;
}

namespace API {

class Dictionary;
class Navigation;
class Object;

class LoaderClient {
public:
    virtual ~LoaderClient() { }

    virtual void didStartProvisionalLoadForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) { }
    virtual void didReceiveServerRedirectForProvisionalLoadForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) { }
    virtual void didFailProvisionalLoadWithErrorForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, const WebCore::ResourceError&, API::Object*) { }
    virtual void didCommitLoadForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) { }
    virtual void didFinishDocumentLoadForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) { }
    virtual void didFinishLoadForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) { }
    virtual void didFailLoadWithErrorForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, const WebCore::ResourceError&, API::Object*) { }
    virtual void didSameDocumentNavigationForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, WebKit::SameDocumentNavigationType, API::Object*) { }
    virtual void didReceiveTitleForFrame(WebKit::WebPageProxy&, const WTF::String&, WebKit::WebFrameProxy&, API::Object*) { }
    virtual void didFirstLayoutForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Object*) { }

    // FIXME: We should consider removing didFirstVisuallyNonEmptyLayoutForFrame since it is replaced by didLayout.
    virtual void didFirstVisuallyNonEmptyLayoutForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Object*) { }

    virtual void didRemoveFrameFromHierarchy(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Object*) { }
    virtual void didDisplayInsecureContentForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Object*) { }
    virtual void didRunInsecureContentForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Object*) { }
    virtual void didDetectXSSForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Object*) { }

    virtual void didLayout(WebKit::WebPageProxy&, WebCore::LayoutMilestones, API::Object*) { }
    
    virtual bool canAuthenticateAgainstProtectionSpaceInFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, WebKit::WebProtectionSpace*) { return false; }
    virtual void didReceiveAuthenticationChallengeInFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, WebKit::AuthenticationChallengeProxy*) { }

    virtual void didStartProgress(WebKit::WebPageProxy&) { }
    virtual void didChangeProgress(WebKit::WebPageProxy&) { }
    virtual void didFinishProgress(WebKit::WebPageProxy&) { }

    // FIXME: These four functions should not be part of this client.
    virtual void processDidBecomeUnresponsive(WebKit::WebPageProxy&) { }
    virtual void interactionOccurredWhileProcessUnresponsive(WebKit::WebPageProxy&) { }
    virtual void processDidBecomeResponsive(WebKit::WebPageProxy&) { }
    virtual void processDidCrash(WebKit::WebPageProxy&) { }

    virtual void didChangeBackForwardList(WebKit::WebPageProxy&, WebKit::WebBackForwardListItem*, Vector<RefPtr<WebKit::WebBackForwardListItem>>) { }
    virtual bool shouldKeepCurrentBackForwardListItemInList(WebKit::WebPageProxy&, WebKit::WebBackForwardListItem*) { return true; }
    virtual void willGoToBackForwardListItem(WebKit::WebPageProxy&, WebKit::WebBackForwardListItem*, API::Object*) { }

    virtual PassRefPtr<Data> webCryptoMasterKey(WebKit::WebPageProxy&) { return nullptr; }

    virtual void didNavigateWithNavigationData(WebKit::WebPageProxy&, const WebKit::WebNavigationDataStore&, WebKit::WebFrameProxy&) { }
    virtual void didPerformClientRedirect(WebKit::WebPageProxy&, const WTF::String&, const WTF::String&, WebKit::WebFrameProxy&) { }
    virtual void didPerformServerRedirect(WebKit::WebPageProxy&, const WTF::String&, const WTF::String&, WebKit::WebFrameProxy&) { }
    virtual void didUpdateHistoryTitle(WebKit::WebPageProxy&, const WTF::String&, const WTF::String&, WebKit::WebFrameProxy&) { }

    virtual void navigationGestureDidBegin(WebKit::WebPageProxy&) { }
    virtual void navigationGestureWillEnd(WebKit::WebPageProxy&, bool willNavigate, WebKit::WebBackForwardListItem&) { }
    virtual void navigationGestureDidEnd(WebKit::WebPageProxy&, bool willNavigate, WebKit::WebBackForwardListItem&) { }

#if ENABLE(NETSCAPE_PLUGIN_API)
    virtual WebKit::PluginModuleLoadPolicy pluginLoadPolicy(WebKit::WebPageProxy&, WebKit::PluginModuleLoadPolicy currentPluginLoadPolicy, API::Dictionary*, WTF::String& /* unavailabilityDescription */) { return currentPluginLoadPolicy; }
    virtual void didFailToInitializePlugin(WebKit::WebPageProxy&, API::Dictionary*) { }
    virtual void didBlockInsecurePluginVersion(WebKit::WebPageProxy&, API::Dictionary*) { }
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(WEBGL)
    virtual WebCore::WebGLLoadPolicy webGLLoadPolicy(WebKit::WebPageProxy&, const WTF::String&) const { return WebCore::WebGLLoadPolicy::WebGLAllowCreation; }
    virtual WebCore::WebGLLoadPolicy resolveWebGLLoadPolicy(WebKit::WebPageProxy&, const WTF::String&) const { return WebCore::WebGLLoadPolicy::WebGLAllowCreation; }
#endif // ENABLE(WEBGL)

#if USE(QUICK_LOOK)
    virtual void didStartLoadForQuickLookDocumentInMainFrame(const WTF::String& fileName, const WTF::String& uti) { }
    virtual void didFinishLoadForQuickLookDocumentInMainFrame(const WebKit::QuickLookDocumentData&) { }
#endif
};

} // namespace API

#endif // APILoaderClient_h
