/*
 * Copyright (C) 2012 Samsung Electronics
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

#ifndef ewk_context_private_h
#define ewk_context_private_h

#include "ewk_context.h"
#include "ewk_object_private.h"
#include <JavaScriptCore/JSContextRef.h>
#include <WebKit/WKBase.h>
#include <WebKit/WKRetainPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

using namespace WebKit;

class EwkApplicationCacheManager;
class EwkCookieManager;
class EwkFaviconDatabase;

namespace WebKit {
class ContextHistoryClientEfl;
class DownloadManagerEfl;
class RequestManagerClientEfl;
#if ENABLE(BATTERY_STATUS)
class BatteryProvider;
#endif
}

class EwkContext : public EwkObject {
public:
    EWK_OBJECT_DECLARE(EwkContext)

    static PassRefPtr<EwkContext> findOrCreateWrapper(WKContextRef context);
    static Ref<EwkContext> create(const String& extensionsPath = String());

    static EwkContext* defaultContext();

    ~EwkContext();

    EwkApplicationCacheManager* applicationCacheManager();
    EwkCookieManager* cookieManager();
    EwkDatabaseManager* databaseManager();

    bool setFaviconDatabaseDirectoryPath(const String& databaseDirectory);
    EwkFaviconDatabase* faviconDatabase();

    EwkStorageManager* storageManager() const;

    WebKit::RequestManagerClientEfl* requestManager();

    void addVisitedLink(const String& visitedURL);

    void setCacheModel(Ewk_Cache_Model);

    Ewk_Cache_Model cacheModel() const;

    void setProcessModel(Ewk_Process_Model);

    Ewk_Process_Model processModel() const;

    WKContextRef wkContext() const { return m_context.get(); }

    WebKit::DownloadManagerEfl* downloadManager() const;

    WebKit::ContextHistoryClientEfl* historyClient();

    Ewk_TLS_Error_Policy ignoreTLSErrors() const;

    void setIgnoreTLSErrors(Ewk_TLS_Error_Policy TLSErrorPolicy) const;

    const String& extensionsPath() const { return m_extensionsPath; }

    void allowSpecificHTTPSCertificateForHost(const String& pem, const String& host) const;

#if ENABLE(NETSCAPE_PLUGIN_API)
    void setAdditionalPluginPath(const String&);
#endif

    void clearResourceCache();

    JSGlobalContextRef jsGlobalContext();

    static void didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo);
    static WKTypeRef getInjectedBundleInitializationUserData(WKContextRef, const void* clientInfo);
    void setMessageFromExtensionCallback(Ewk_Context_Message_From_Extension_Cb, void*);
    void processReceivedMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);

private:
    explicit EwkContext(WKContextRef, const String& extensionsPath = String());

    void ensureFaviconDatabase();
    bool isDefaultBundle() const;

    WKRetainPtr<WKContextRef> m_context;

    std::unique_ptr<EwkApplicationCacheManager> m_applicationCacheManager;
    std::unique_ptr<EwkCookieManager> m_cookieManager;
    std::unique_ptr<EwkDatabaseManager> m_databaseManager;
    std::unique_ptr<EwkFaviconDatabase> m_faviconDatabase;
    std::unique_ptr<EwkStorageManager> m_storageManager;
#if ENABLE(BATTERY_STATUS)
    RefPtr<WebKit::BatteryProvider> m_batteryProvider;
#endif
    std::unique_ptr<WebKit::DownloadManagerEfl> m_downloadManager;
    std::unique_ptr<WebKit::RequestManagerClientEfl> m_requestManagerClient;

    std::unique_ptr<WebKit::ContextHistoryClientEfl> m_historyClient;

    JSGlobalContextRef m_jsGlobalContext;

    String m_extensionsPath;

    struct {
        Ewk_Context_Message_From_Extension_Cb callback;
        void* userData;
    } m_callbackForMessageFromExtension;
};

#endif // ewk_context_private_h
