/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
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

#ifndef WebDatabaseManagerProxy_h
#define WebDatabaseManagerProxy_h

#include "APIObject.h"
#include "Arguments.h"
#include "GenericCallback.h"
#include "MessageReceiver.h"
#include "OriginAndDatabases.h"
#include "WebContextSupplement.h"
#include "WebDatabaseManagerProxyClient.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>

namespace WebKit {

class WebProcessPool;
class WebProcessProxy;

typedef GenericCallback<API::Array*> ArrayCallback;

class WebDatabaseManagerProxy : public API::ObjectImpl<API::Object::Type::DatabaseManager>, public WebContextSupplement, private IPC::MessageReceiver {
public:
    static const char* supplementName();

    static PassRefPtr<WebDatabaseManagerProxy> create(WebProcessPool*);
    virtual ~WebDatabaseManagerProxy();

    void initializeClient(const WKDatabaseManagerClientBase*);

    void getDatabasesByOrigin(std::function<void (API::Array*, CallbackBase::Error)>);
    void getDatabaseOrigins(std::function<void (API::Array*, CallbackBase::Error)>);
    void deleteDatabaseWithNameForOrigin(const String& databaseIdentifier, API::SecurityOrigin*);
    void deleteDatabasesForOrigin(API::SecurityOrigin*);
    void deleteAllDatabases();
    void setQuotaForOrigin(API::SecurityOrigin*, uint64_t quota);
    
    static String originKey();
    static String originQuotaKey();
    static String originUsageKey();
    static String databaseDetailsKey();
    static String databaseDetailsNameKey();
    static String databaseDetailsDisplayNameKey();
    static String databaseDetailsExpectedUsageKey();
    static String databaseDetailsCurrentUsageKey();
    static String databaseDetailsCreationTimeKey();
    static String databaseDetailsModificationTimeKey();

    using API::Object::ref;
    using API::Object::deref;

private:
    explicit WebDatabaseManagerProxy(WebProcessPool*);

    // WebContextSupplement
    virtual void processPoolDestroyed() override;
    virtual void processDidClose(WebProcessProxy*) override;
    virtual bool shouldTerminate(WebProcessProxy*) const override;
    virtual void refWebContextSupplement() override;
    virtual void derefWebContextSupplement() override;

    // IPC::MessageReceiver
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    // Message handlers.
    void didGetDatabasesByOrigin(const Vector<OriginAndDatabases>& originAndDatabases, uint64_t callbackID);
    void didGetDatabaseOrigins(const Vector<String>& originIdentifiers, uint64_t callbackID);
    void didModifyOrigin(const String& originIdentifier);
    void didModifyDatabase(const String& originIdentifier, const String& databaseIdentifier);

    HashMap<uint64_t, RefPtr<ArrayCallback>> m_arrayCallbacks;
    WebDatabaseManagerProxyClient m_client;
};

} // namespace WebKit

#endif // DatabaseManagerProxy_h
