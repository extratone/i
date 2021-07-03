/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef NetworkProcessConnection_h
#define NetworkProcessConnection_h

#include "Connection.h"
#include "ShareableResource.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

#if ENABLE(NETWORK_PROCESS)

namespace IPC {
class DataReference;
}

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SessionID;
}

namespace WebKit {

typedef uint64_t ResourceLoadIdentifier;

class NetworkProcessConnection : public RefCounted<NetworkProcessConnection>, IPC::Connection::Client {
public:
    static Ref<NetworkProcessConnection> create(IPC::Connection::Identifier connectionIdentifier)
    {
        return adoptRef(*new NetworkProcessConnection(connectionIdentifier));
    }
    ~NetworkProcessConnection();
    
    IPC::Connection* connection() const { return m_connection.get(); }

    void didReceiveNetworkProcessConnectionMessage(IPC::Connection&, IPC::MessageDecoder&);

private:
    NetworkProcessConnection(IPC::Connection::Identifier);

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;
    virtual void didClose(IPC::Connection&) override;
    virtual void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;
    virtual IPC::ProcessType localProcessType() override { return IPC::ProcessType::Web; }
    virtual IPC::ProcessType remoteProcessType() override { return IPC::ProcessType::Network; }

#if ENABLE(SHAREABLE_RESOURCE)
    // Message handlers.
    void didCacheResource(const WebCore::ResourceRequest&, const ShareableResource::Handle&, WebCore::SessionID);
#endif

    // The connection from the web process to the network process.
    RefPtr<IPC::Connection> m_connection;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)


#endif // NetworkProcessConnection_h
