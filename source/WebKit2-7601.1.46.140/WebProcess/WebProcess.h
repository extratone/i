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

#ifndef WebProcess_h
#define WebProcess_h

#include "CacheModel.h"
#include "ChildProcess.h"
#include "DownloadManager.h"
#include "DrawingArea.h"
#include "PluginProcessConnectionManager.h"
#include "ResourceCachesToClear.h"
#include "SandboxExtension.h"
#include "SharedMemory.h"
#include "TextCheckerState.h"
#include "ViewUpdateDispatcher.h"
#include "VisitedLinkTable.h"
#include <WebCore/SessionID.h>
#include <WebCore/Timer.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

#if PLATFORM(COCOA)
#include <dispatch/dispatch.h>
#include <WebCore/MachSendRight.h>
#endif

#if PLATFORM(IOS)
#include "WebSQLiteDatabaseTracker.h"
#endif

namespace API {
class Object;
}

namespace WebCore {
class CertificateInfo;
class PageGroup;
class ResourceRequest;
struct PluginInfo;
}

namespace WebKit {

class DownloadManager;
class EventDispatcher;
class InjectedBundle;
class ObjCObjectGraph;
class UserData;
class WebConnectionToUIProcess;
class WebFrame;
class WebIconDatabaseProxy;
class WebPage;
class WebPageGroupProxy;
class WebProcessSupplement;
struct SecurityOriginData;
struct WebPageCreationParameters;
struct WebPageGroupData;
struct WebPreferencesStore;
struct WebProcessCreationParameters;

#if ENABLE(NETWORK_PROCESS)
class NetworkProcessConnection;
class WebResourceLoadScheduler;
#endif

#if ENABLE(DATABASE_PROCESS)
class WebToDatabaseProcessConnection;
#endif

class WebProcess : public ChildProcess, private DownloadManager::Client {
    friend class NeverDestroyed<DownloadManager>;
public:
    static WebProcess& singleton();

    template <typename T>
    T* supplement()
    {
        return static_cast<T*>(m_supplements.get(T::supplementName()));
    }

    template <typename T>
    void addSupplement()
    {
        m_supplements.add(T::supplementName(), std::make_unique<T>(this));
    }

    WebConnectionToUIProcess* webConnectionToUIProcess() const { return m_webConnection.get(); }

    WebPage* webPage(uint64_t pageID) const;
    void createWebPage(uint64_t pageID, const WebPageCreationParameters&);
    void removeWebPage(uint64_t pageID);
    WebPage* focusedWebPage() const;

    InjectedBundle* injectedBundle() const { return m_injectedBundle.get(); }

#if PLATFORM(COCOA)
    const WebCore::MachSendRight& compositingRenderServerPort() const { return m_compositingRenderServerPort; }
#endif

    bool shouldPlugInAutoStartFromOrigin(WebPage&, const String& pageOrigin, const String& pluginOrigin, const String& mimeType);
    void plugInDidStartFromOrigin(const String& pageOrigin, const String& pluginOrigin, const String& mimeType, WebCore::SessionID);
    void plugInDidReceiveUserInteraction(const String& pageOrigin, const String& pluginOrigin, const String& mimeType, WebCore::SessionID);
    void setPluginLoadClientPolicy(uint8_t policy, const String& host, const String& bundleIdentifier, const String& versionString);
    void clearPluginClientPolicies();

    bool fullKeyboardAccessEnabled() const { return m_fullKeyboardAccessEnabled; }

    WebFrame* webFrame(uint64_t) const;
    void addWebFrame(uint64_t, WebFrame*);
    void removeWebFrame(uint64_t);

    WebPageGroupProxy* webPageGroup(WebCore::PageGroup*);
    WebPageGroupProxy* webPageGroup(uint64_t pageGroupID);
    WebPageGroupProxy* webPageGroup(const WebPageGroupData&);

#if PLATFORM(COCOA)
    pid_t presenterApplicationPid() const { return m_presenterApplicationPid; }
#endif
    
    const TextCheckerState& textCheckerState() const { return m_textCheckerState; }
    DownloadManager& downloadManager();

    void clearResourceCaches(ResourceCachesToClear = AllResourceCaches);
    
#if ENABLE(NETSCAPE_PLUGIN_API)
    PluginProcessConnectionManager& pluginProcessConnectionManager();
#endif

    EventDispatcher& eventDispatcher() { return *m_eventDispatcher; }

    bool usesNetworkProcess() const;

#if ENABLE(NETWORK_PROCESS)
    NetworkProcessConnection* networkConnection();
    void networkProcessConnectionClosed(NetworkProcessConnection*);
    WebResourceLoadScheduler& webResourceLoadScheduler();
#endif

#if ENABLE(DATABASE_PROCESS)
    void webToDatabaseProcessConnectionClosed(WebToDatabaseProcessConnection*);
    WebToDatabaseProcessConnection* webToDatabaseProcessConnection();
#endif

    void setCacheModel(uint32_t);

    void ensurePrivateBrowsingSession(WebCore::SessionID);
    void destroyPrivateBrowsingSession(WebCore::SessionID);

    void pageDidEnterWindow(uint64_t pageID);
    void pageWillLeaveWindow(uint64_t pageID);

    void nonVisibleProcessCleanupTimerFired();

#if PLATFORM(COCOA)
    void destroyRenderingResources();
#endif

    void updateActivePages();

#if USE(SOUP)
    void allowSpecificHTTPSCertificateForHost(const WebCore::CertificateInfo&, const String& host);
#endif

    void processWillSuspendImminently(bool& handled);
    void prepareToSuspend();
    void cancelPrepareToSuspend();
    bool markAllLayersVolatileIfPossible();
    void setAllLayerTreeStatesFrozen(bool);
    void processSuspensionCleanupTimerFired();
    void processDidResume();

#if PLATFORM(IOS)
    void resetAllGeolocationPermissions();
#endif

    RefPtr<API::Object> transformHandlesToObjects(API::Object*);
    static RefPtr<API::Object> transformObjectsToHandles(API::Object*);

#if PLATFORM(COCOA)
    RefPtr<ObjCObjectGraph> transformHandlesToObjects(ObjCObjectGraph&);
    static RefPtr<ObjCObjectGraph> transformObjectsToHandles(ObjCObjectGraph&);
#endif

#if ENABLE(SERVICE_CONTROLS)
    bool hasImageServices() const { return m_hasImageServices; }
    bool hasSelectionServices() const { return m_hasSelectionServices; }
    bool hasRichContentServices() const { return m_hasRichContentServices; }
#endif

private:
    WebProcess();
    ~WebProcess();

    // DownloadManager::Client.
    virtual void didCreateDownload() override;
    virtual void didDestroyDownload() override;
    virtual IPC::Connection* downloadProxyConnection() override;
    virtual AuthenticationManager& downloadsAuthenticationManager() override;

    void initializeWebProcess(WebProcessCreationParameters&&);
    void platformInitializeWebProcess(WebProcessCreationParameters&&);

    void platformTerminate();
    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&) const;
    void registerURLSchemeAsBypassingContentSecurityPolicy(const String&) const;
    void setDomainRelaxationForbiddenForURLScheme(const String&) const;
    void registerURLSchemeAsLocal(const String&) const;
    void registerURLSchemeAsNoAccess(const String&) const;
    void registerURLSchemeAsDisplayIsolated(const String&) const;
    void registerURLSchemeAsCORSEnabled(const String&) const;
#if ENABLE(CACHE_PARTITIONING)
    void registerURLSchemeAsCachePartitioned(const String&) const;
#endif
    void setDefaultRequestTimeoutInterval(double);
    void setAlwaysUsesComplexTextCodePath(bool);
    void setShouldUseFontSmoothing(bool);
    void userPreferredLanguagesChanged(const Vector<String>&) const;
    void fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled);

    bool isPlugInAutoStartOriginHash(unsigned plugInOriginHash, WebCore::SessionID);
    void didAddPlugInAutoStartOriginHash(unsigned plugInOriginHash, double expirationTime, WebCore::SessionID);
    void resetPlugInAutoStartOriginDefaultHashes(const HashMap<unsigned, double>& hashes);
    void resetPlugInAutoStartOriginHashes(const HashMap<WebCore::SessionID, HashMap<unsigned, double>>& hashes);

    void platformSetCacheModel(CacheModel);
    void platformClearResourceCaches(ResourceCachesToClear);
    void clearApplicationCache();

    void setEnhancedAccessibility(bool);
    
    void startMemorySampler(const SandboxExtension::Handle&, const String&, const double);
    void stopMemorySampler();

    void downloadRequest(uint64_t downloadID, uint64_t initiatingPageID, const WebCore::ResourceRequest&);
    void resumeDownload(uint64_t downloadID, const IPC::DataReference& resumeData, const String& path, const SandboxExtension::Handle&);
    void cancelDownload(uint64_t downloadID);

    void setTextCheckerState(const TextCheckerState&);
    
    void getWebCoreStatistics(uint64_t callbackID);
    void garbageCollectJavaScriptObjects();
    void setJavaScriptGarbageCollectorTimerEnabled(bool flag);

    void releasePageCache();

    void fetchWebsiteData(WebCore::SessionID, uint64_t websiteDataTypes, uint64_t callbackID);
    void deleteWebsiteData(WebCore::SessionID, uint64_t websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID);
    void deleteWebsiteDataForOrigins(WebCore::SessionID, uint64_t websiteDataTypes, const Vector<WebKit::SecurityOriginData>& origins, uint64_t callbackID);

#if USE(SOUP)
    void setIgnoreTLSErrors(bool);
#endif

    void setMemoryCacheDisabled(bool);

#if ENABLE(SERVICE_CONTROLS)
    void setEnabledServices(bool hasImageServices, bool hasSelectionServices, bool hasRichContentServices);
#endif

    void handleInjectedBundleMessage(const String& messageName, const UserData& messageBody);
    void setInjectedBundleParameter(const String& key, const IPC::DataReference&);

    enum class ShouldAcknowledgeWhenReadyToSuspend { No, Yes };
    void actualPrepareToSuspend(ShouldAcknowledgeWhenReadyToSuspend);

    // ChildProcess
    virtual void initializeProcess(const ChildProcessInitializationParameters&) override;
    virtual void initializeProcessName(const ChildProcessInitializationParameters&) override;
    virtual void initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&) override;
    virtual void initializeConnection(IPC::Connection*) override;
    virtual bool shouldTerminate() override;
    virtual void terminate() override;

#if USE(APPKIT)
    virtual void stopRunLoop() override;
#endif

    void platformInitializeProcess(const ChildProcessInitializationParameters&);

    // IPC::Connection::Client
    friend class WebConnectionToUIProcess;
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;
    virtual void didClose(IPC::Connection&) override;
    virtual void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;
    virtual IPC::ProcessType localProcessType() override { return IPC::ProcessType::Web; }
    virtual IPC::ProcessType remoteProcessType() override { return IPC::ProcessType::UI; }

    // Implemented in generated WebProcessMessageReceiver.cpp
    void didReceiveWebProcessMessage(IPC::Connection&, IPC::MessageDecoder&);
    void didReceiveSyncWebProcessMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);

    RefPtr<WebConnectionToUIProcess> m_webConnection;

    HashMap<uint64_t, RefPtr<WebPage>> m_pageMap;
    HashMap<uint64_t, RefPtr<WebPageGroupProxy>> m_pageGroupMap;
    RefPtr<InjectedBundle> m_injectedBundle;

    RefPtr<EventDispatcher> m_eventDispatcher;
#if PLATFORM(IOS)
    RefPtr<ViewUpdateDispatcher> m_viewUpdateDispatcher;
#endif
    WebCore::Timer m_processSuspensionCleanupTimer;

    bool m_inDidClose;

    HashMap<WebCore::SessionID, HashMap<unsigned, double>> m_plugInAutoStartOriginHashes;
    HashSet<String> m_plugInAutoStartOrigins;

    bool m_hasSetCacheModel;
    CacheModel m_cacheModel;

#if PLATFORM(COCOA)
    WebCore::MachSendRight m_compositingRenderServerPort;
    pid_t m_presenterApplicationPid;
#endif

    bool m_fullKeyboardAccessEnabled;

    HashMap<uint64_t, WebFrame*> m_frameMap;

    typedef HashMap<const char*, std::unique_ptr<WebProcessSupplement>, PtrHash<const char*>> WebProcessSupplementMap;
    WebProcessSupplementMap m_supplements;

    TextCheckerState m_textCheckerState;

    WebIconDatabaseProxy* m_iconDatabaseProxy;

#if ENABLE(NETWORK_PROCESS)
    void ensureNetworkProcessConnection();
    RefPtr<NetworkProcessConnection> m_networkProcessConnection;
    bool m_usesNetworkProcess;
    WebResourceLoadScheduler* m_webResourceLoadScheduler;
#endif

#if ENABLE(DATABASE_PROCESS)
    void ensureWebToDatabaseProcessConnection();
    RefPtr<WebToDatabaseProcessConnection> m_webToDatabaseProcessConnection;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    RefPtr<PluginProcessConnectionManager> m_pluginProcessConnectionManager;
#endif

#if ENABLE(SERVICE_CONTROLS)
    bool m_hasImageServices;
    bool m_hasSelectionServices;
    bool m_hasRichContentServices;
#endif

    HashSet<uint64_t> m_pagesInWindows;
    WebCore::Timer m_nonVisibleProcessCleanupTimer;

#if PLATFORM(IOS)
    WebSQLiteDatabaseTracker m_webSQLiteDatabaseTracker;
#endif

    ShouldAcknowledgeWhenReadyToSuspend m_shouldAcknowledgeWhenReadyToSuspend;
};

} // namespace WebKit

#endif // WebProcess_h
