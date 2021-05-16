/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "FrameLoaderTypes.h"
#include "ResourceLoader.h"
#include "SubstituteData.h"
#include "Timer.h"
#include <wtf/Forward.h>

namespace WebCore {

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    class ApplicationCache;
#endif
    class FormState;
    class ResourceRequest;

    class MainResourceLoader : public ResourceLoader {
    public:
        static PassRefPtr<MainResourceLoader> create(Frame*);
        virtual ~MainResourceLoader();

        virtual bool load(const ResourceRequest&, const SubstituteData&);
        virtual void addData(const char*, int, bool allAtOnce);

        virtual void setDefersLoading(bool);

        virtual void willSendRequest(ResourceRequest&, const ResourceResponse& redirectResponse);
        virtual void didReceiveResponse(const ResourceResponse&);
        virtual void didReceiveData(const char*, int, long long lengthReceived, bool allAtOnce);
        virtual void didFinishLoading();
        virtual void didFail(const ResourceError&);

        void handleDataLoadNow(Timer<MainResourceLoader>*);

        bool isLoadingMultipartContent() const { return m_loadingMultipartContent; }
        
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        ApplicationCache* applicationCache() const { return m_applicationCache.get(); }
#endif

    private:
        MainResourceLoader(Frame*);

        virtual void didCancel(const ResourceError&);

        bool loadNow(ResourceRequest&);

        void handleEmptyLoad(const KURL&, bool forURLScheme);
        void handleDataLoadSoon(ResourceRequest& r);

        void handleDataLoad(ResourceRequest&);

        void receivedError(const ResourceError&);
        ResourceError interruptionForPolicyChangeError() const;
        void stopLoadingForPolicyChange();
        bool isPostOrRedirectAfterPost(const ResourceRequest& newRequest, const ResourceResponse& redirectResponse);

        static void callContinueAfterNavigationPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
        void continueAfterNavigationPolicy(const ResourceRequest&, bool shouldContinue);

        static void callContinueAfterContentPolicy(void*, PolicyAction);
        void continueAfterContentPolicy(PolicyAction);
        void continueAfterContentPolicy(PolicyAction, const ResourceResponse&);

        ResourceRequest m_initialRequest;
        SubstituteData m_substituteData;
        Timer<MainResourceLoader> m_dataLoadTimer;

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        // The application cache that the main resource was loaded from (if any).
        RefPtr<ApplicationCache> m_applicationCache;
#endif

        bool m_loadingMultipartContent;
        bool m_waitingForContentPolicy;
    };

}
