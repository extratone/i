/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebFrame_h
#define WebFrame_h

#include "APIObject.h"
#include "ShareableBitmap.h"
#include "WKBase.h"
#include "WebFrameLoaderClient.h"
#include <JavaScriptCore/JSBase.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/PolicyChecker.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

namespace API {
class Array;
}

namespace WebCore {
class CertificateInfo;
class Frame;
class HTMLFrameOwnerElement;
class IntPoint;
class IntRect;
class URL;
}

namespace WebKit {

class InjectedBundleFileHandle;
class InjectedBundleHitTestResult;
class InjectedBundleNodeHandle;
class InjectedBundleRangeHandle;
class InjectedBundleScriptWorld;
class WebPage;

class WebFrame : public API::ObjectImpl<API::Object::Type::BundleFrame> {
public:
    static PassRefPtr<WebFrame> createWithCoreMainFrame(WebPage*, WebCore::Frame*);
    static PassRefPtr<WebFrame> createSubframe(WebPage*, const String& frameName, WebCore::HTMLFrameOwnerElement*);
    ~WebFrame();

    // Called when the FrameLoaderClient (and therefore the WebCore::Frame) is being torn down.
    void invalidate();

    WebPage* page() const;

    static WebFrame* fromCoreFrame(WebCore::Frame&);
    WebCore::Frame* coreFrame() const { return m_coreFrame; }

    uint64_t frameID() const { return m_frameID; }

    uint64_t setUpPolicyListener(WebCore::FramePolicyFunction);
    void invalidatePolicyListener();
    void didReceivePolicyDecision(uint64_t listenerID, WebCore::PolicyAction, uint64_t navigationID, uint64_t downloadID);

    void startDownload(const WebCore::ResourceRequest&);
    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    String source() const;
    String contentsAsString() const;
    String selectionAsString() const;

    WebCore::IntSize size() const;

    // WKBundleFrame API and SPI functions
    bool isMainFrame() const;
    String name() const;
    String url() const;
    WebCore::CertificateInfo certificateInfo() const;
    String innerText() const;
    bool isFrameSet() const;
    WebFrame* parentFrame() const;
    Ref<API::Array> childFrames();
    JSGlobalContextRef jsContext();
    JSGlobalContextRef jsContextForWorld(InjectedBundleScriptWorld*);
    WebCore::IntRect contentBounds() const;
    WebCore::IntRect visibleContentBounds() const;
    WebCore::IntRect visibleContentBoundsExcludingScrollbars() const;
    WebCore::IntSize scrollOffset() const;
    bool hasHorizontalScrollbar() const;
    bool hasVerticalScrollbar() const;
    PassRefPtr<InjectedBundleHitTestResult> hitTest(const WebCore::IntPoint) const;
    bool getDocumentBackgroundColor(double* red, double* green, double* blue, double* alpha);
    bool containsAnyFormElements() const;
    bool containsAnyFormControls() const;
    void stopLoading();
    bool handlesPageScaleGesture() const;
    void setAccessibleName(const String&);

    static WebFrame* frameForContext(JSContextRef);

    JSValueRef jsWrapperForWorld(InjectedBundleNodeHandle*, InjectedBundleScriptWorld*);
    JSValueRef jsWrapperForWorld(InjectedBundleRangeHandle*, InjectedBundleScriptWorld*);
    JSValueRef jsWrapperForWorld(InjectedBundleFileHandle*, InjectedBundleScriptWorld*);

    static String counterValue(JSObjectRef element);

    String layerTreeAsText() const;
    
    unsigned pendingUnloadCount() const;
    
    bool allowsFollowingLink(const WebCore::URL&) const;

    String provisionalURL() const;
    String suggestedFilenameForResourceWithURL(const WebCore::URL&) const;
    String mimeTypeForResourceWithURL(const WebCore::URL&) const;

    void setTextDirection(const String&);

    void documentLoaderDetached(uint64_t navigationID);

    // Simple listener class used by plug-ins to know when frames finish or fail loading.
    class LoadListener {
    public:
        virtual ~LoadListener() { }

        virtual void didFinishLoad(WebFrame*) = 0;
        virtual void didFailLoad(WebFrame*, bool wasCancelled) = 0;
    };
    void setLoadListener(LoadListener* loadListener) { m_loadListener = loadListener; }
    LoadListener* loadListener() const { return m_loadListener; }
    
#if PLATFORM(COCOA)
    typedef bool (*FrameFilterFunction)(WKBundleFrameRef, WKBundleFrameRef subframe, void* context);
    RetainPtr<CFDataRef> webArchiveData(FrameFilterFunction, void* context);
#endif

    PassRefPtr<ShareableBitmap> createSelectionSnapshot() const;

#if PLATFORM(IOS)
    uint64_t firstLayerTreeTransactionIDAfterDidCommitLoad() const { return m_firstLayerTreeTransactionIDAfterDidCommitLoad; }
    void setFirstLayerTreeTransactionIDAfterDidCommitLoad(uint64_t transactionID) { m_firstLayerTreeTransactionIDAfterDidCommitLoad = transactionID; }
#endif

private:
    static PassRefPtr<WebFrame> create(std::unique_ptr<WebFrameLoaderClient>);
    WebFrame(std::unique_ptr<WebFrameLoaderClient>);

    WebCore::Frame* m_coreFrame;

    uint64_t m_policyListenerID;
    WebCore::FramePolicyFunction m_policyFunction;
    uint64_t m_policyDownloadID;

    std::unique_ptr<WebFrameLoaderClient> m_frameLoaderClient;
    LoadListener* m_loadListener;
    
    uint64_t m_frameID;

#if PLATFORM(IOS)
    uint64_t m_firstLayerTreeTransactionIDAfterDidCommitLoad;
#endif
};

} // namespace WebKit

#endif // WebFrame_h
