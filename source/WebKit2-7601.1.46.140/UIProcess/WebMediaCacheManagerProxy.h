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

#ifndef WebMediaCacheManagerProxy_h
#define WebMediaCacheManagerProxy_h

#include "APIObject.h"
#include "GenericCallback.h"
#include "MessageReceiver.h"
#include "WebContextSupplement.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebKit {

class WebProcessPool;
class WebProcessProxy;

typedef GenericCallback<API::Array*> ArrayCallback;

class WebMediaCacheManagerProxy : public API::ObjectImpl<API::Object::Type::MediaCacheManager>, public WebContextSupplement, private IPC::MessageReceiver {
public:
    static const char* supplementName();

    static PassRefPtr<WebMediaCacheManagerProxy> create(WebProcessPool*);
    virtual ~WebMediaCacheManagerProxy();
    
    void getHostnamesWithMediaCache(std::function<void (API::Array*, CallbackBase::Error)>);
    void clearCacheForHostname(const String&);
    void clearCacheForAllHostnames();

    using API::Object::ref;
    using API::Object::deref;

private:
    explicit WebMediaCacheManagerProxy(WebProcessPool*);

    void didGetHostnamesWithMediaCache(const Vector<String>&, uint64_t callbackID);

    // WebContextSupplement
    virtual void processPoolDestroyed() override;
    virtual void processDidClose(WebProcessProxy*) override;
    virtual bool shouldTerminate(WebProcessProxy*) const override;
    virtual void refWebContextSupplement() override;
    virtual void derefWebContextSupplement() override;

    // IPC::MessageReceiver
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    HashMap<uint64_t, RefPtr<ArrayCallback>> m_arrayCallbacks;
};

} // namespace WebKit

#endif // WebMediaCacheManagerProxy_h
