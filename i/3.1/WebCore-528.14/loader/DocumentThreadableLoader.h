/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DocumentThreadableLoader_h
#define DocumentThreadableLoader_h

#include "SubresourceLoaderClient.h"
#include "ThreadableLoader.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
    class Document;
    class ResourceRequest;
    class ThreadableLoaderClient;

    class DocumentThreadableLoader : public RefCounted<DocumentThreadableLoader>, public ThreadableLoader, private SubresourceLoaderClient  {
    public:
        static PassRefPtr<DocumentThreadableLoader> create(Document*, ThreadableLoaderClient*, const ResourceRequest&, LoadCallbacks, ContentSniff);
        virtual ~DocumentThreadableLoader();

        virtual void cancel();

        using RefCounted<DocumentThreadableLoader>::ref;
        using RefCounted<DocumentThreadableLoader>::deref;

    protected:
        virtual void refThreadableLoader() { ref(); }
        virtual void derefThreadableLoader() { deref(); }

    private:
        DocumentThreadableLoader(Document*, ThreadableLoaderClient*, const ResourceRequest&, LoadCallbacks, ContentSniff);
        virtual void willSendRequest(SubresourceLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
        virtual void didSendData(SubresourceLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent);

        virtual void didReceiveResponse(SubresourceLoader*, const ResourceResponse&);
        virtual void didReceiveData(SubresourceLoader*, const char*, int lengthReceived);
        virtual void didFinishLoading(SubresourceLoader*);
        virtual void didFail(SubresourceLoader*, const ResourceError&);

        virtual void receivedCancellation(SubresourceLoader*, const AuthenticationChallenge&);

        RefPtr<SubresourceLoader> m_loader;
        ThreadableLoaderClient* m_client;
        Document* m_document;
    };

} // namespace WebCore

#endif // DocumentThreadableLoader_h
