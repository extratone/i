/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef WebSoupCustomProtocolRequestManager_h
#define WebSoupCustomProtocolRequestManager_h

#include "APIObject.h"
#include "WebContextSupplement.h"
#include "WebSoupCustomProtocolRequestManagerClient.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace API {
class Data;
}

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {

class WebProcessPool;

class WebSoupCustomProtocolRequestManager : public API::ObjectImpl<API::Object::Type::SoupCustomProtocolRequestManager>, public WebContextSupplement {
public:
    static const char* supplementName();

    static Ref<WebSoupCustomProtocolRequestManager> create(WebProcessPool*);
    virtual ~WebSoupCustomProtocolRequestManager();

    void initializeClient(const WKSoupCustomProtocolRequestManagerClientBase*);

    void registerSchemeForCustomProtocol(const String& scheme);
    void unregisterSchemeForCustomProtocol(const String& scheme);

    void startLoading(uint64_t customProtocolID, const WebCore::ResourceRequest&);
    void stopLoading(uint64_t customProtocolID);

    void didReceiveResponse(uint64_t customProtocolID, const WebCore::ResourceResponse&);
    void didLoadData(uint64_t customProtocolID, const API::Data*);
    void didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError&);
    void didFinishLoading(uint64_t customProtocolID);

    const Vector<String>& registeredSchemesForCustomProtocols() const { return m_registeredSchemes; }

    using API::Object::ref;
    using API::Object::deref;

private:
    WebSoupCustomProtocolRequestManager(WebProcessPool*);

    // WebContextSupplement
    virtual void processPoolDestroyed() override;
    virtual void processDidClose(WebProcessProxy*) override;
    virtual void refWebContextSupplement() override;
    virtual void derefWebContextSupplement() override;

    WebSoupCustomProtocolRequestManagerClient m_client;
    Vector<String> m_registeredSchemes;
};

} // namespace WebKit

#endif // WebSoupCustomProtocolRequestManager_h
