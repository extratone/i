/*
 * Copyright (C) 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef WebProcessPool_h
#define WebProcessPool_h

#include "APIDictionary.h"
#include "APIObject.h"
#include "APIProcessPoolConfiguration.h"
#include "APIWebsiteDataStore.h"
#include "DownloadProxyMap.h"
#include "GenericCallback.h"
#include "MessageReceiver.h"
#include "MessageReceiverMap.h"
#include "PlugInAutoStartProvider.h"
#include "PluginInfoStore.h"
#include "ProcessModel.h"
#include "ProcessThrottler.h"
#include "StatisticsRequest.h"
#include "VisitedLinkProvider.h"
#include "WebContextClient.h"
#include "WebContextConnectionClient.h"
#include "WebContextInjectedBundleClient.h"
#include "WebProcessProxy.h"
#include <WebCore/LinkHash.h>
#include <WebCore/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounter.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if ENABLE(DATABASE_PROCESS)
#include "DatabaseProcessProxy.h"
#endif

#if ENABLE(NETWORK_PROCESS)
#include "NetworkProcessProxy.h"
#endif

#if PLATFORM(COCOA)
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS NSObject;
OBJC_CLASS NSString;
#endif

namespace API {
class DownloadClient;
class LegacyContextHistoryClient;
}

namespace WebKit {

class DownloadProxy;
class WebContextSupplement;
class WebIconDatabase;
class WebPageGroup;
class WebPageProxy;
struct StatisticsData;
struct WebPageConfiguration;
struct WebProcessCreationParameters;
    
typedef GenericCallback<API::Dictionary*> DictionaryCallback;

#if ENABLE(NETWORK_PROCESS)
struct NetworkProcessCreationParameters;
#endif

#if PLATFORM(COCOA)
int networkProcessLatencyQOS();
int networkProcessThroughputQOS();
int webProcessLatencyQOS();
int webProcessThroughputQOS();
#endif

class WebProcessPool final : public API::ObjectImpl<API::Object::Type::ProcessPool>, private IPC::MessageReceiver
#if ENABLE(NETSCAPE_PLUGIN_API)
    , private PluginInfoStoreClient
#endif
    {
public:
    static Ref<WebProcessPool> create(API::ProcessPoolConfiguration&);

    explicit WebProcessPool(API::ProcessPoolConfiguration&);        
    virtual ~WebProcessPool();

    API::ProcessPoolConfiguration& configuration() { return m_configuration.get(); }

    static const Vector<WebProcessPool*>& allProcessPools();

    template <typename T>
    T* supplement()
    {
        return static_cast<T*>(m_supplements.get(T::supplementName()));
    }

    template <typename T>
    void addSupplement()
    {
        m_supplements.add(T::supplementName(), T::create(this));
    }

    void addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver&);
    void addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID);

    bool dispatchMessage(IPC::Connection&, IPC::MessageDecoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);

    void initializeClient(const WKContextClientBase*);
    void initializeInjectedBundleClient(const WKContextInjectedBundleClientBase*);
    void initializeConnectionClient(const WKContextConnectionClientBase*);
    void setHistoryClient(std::unique_ptr<API::LegacyContextHistoryClient>);
    void setDownloadClient(std::unique_ptr<API::DownloadClient>);

    void setProcessModel(ProcessModel); // Can only be called when there are no processes running.
    ProcessModel processModel() const { return m_configuration->processModel(); }

    void setMaximumNumberOfProcesses(unsigned); // Can only be called when there are no processes running.
    unsigned maximumNumberOfProcesses() const { return !m_configuration->maximumProcessCount() ? UINT_MAX : m_configuration->maximumProcessCount(); }

    const Vector<RefPtr<WebProcessProxy>>& processes() const { return m_processes; }

    // WebProcess or NetworkProcess as approporiate for current process model. The connection must be non-null.
    IPC::Connection* networkingProcessConnection();

    template<typename T> void sendToAllProcesses(const T& message);
    template<typename T> void sendToAllProcessesRelaunchingThemIfNecessary(const T& message);
    template<typename T> void sendToOneProcess(T&& message);

    // Sends the message to WebProcess or NetworkProcess as approporiate for current process model.
    template<typename T> void sendToNetworkingProcess(T&& message);
    template<typename T> void sendToNetworkingProcessRelaunchingIfNecessary(T&& message);

    // Sends the message to WebProcess or DatabaseProcess as approporiate for current process model.
    template<typename T> void sendToDatabaseProcessRelaunchingIfNecessary(T&& message);

    void processDidFinishLaunching(WebProcessProxy*);

    // Disconnect the process from the context.
    void disconnectProcess(WebProcessProxy*);

    API::WebsiteDataStore* websiteDataStore() const { return m_websiteDataStore.get(); }

    PassRefPtr<WebPageProxy> createWebPage(PageClient&, WebPageConfiguration);

    const String& injectedBundlePath() const { return m_configuration->injectedBundlePath(); }

    DownloadProxy* download(WebPageProxy* initiatingPage, const WebCore::ResourceRequest&);
    DownloadProxy* resumeDownload(const API::Data* resumeData, const String& path);

    void setInjectedBundleInitializationUserData(PassRefPtr<API::Object> userData) { m_injectedBundleInitializationUserData = userData; }

    void postMessageToInjectedBundle(const String&, API::Object*);

    void populateVisitedLinks();

#if ENABLE(NETSCAPE_PLUGIN_API)
    void setAdditionalPluginsDirectory(const String&);

    PluginInfoStore& pluginInfoStore() { return m_pluginInfoStore; }

    void setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy, const String& host, const String& bundleIdentifier, const String& versionString);
    void clearPluginClientPolicies();
#endif

#if ENABLE(NETWORK_PROCESS)
    PlatformProcessIdentifier networkProcessIdentifier();
#endif

    void setAlwaysUsesComplexTextCodePath(bool);
    void setShouldUseFontSmoothing(bool);
    
    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&);
    void registerURLSchemeAsBypassingContentSecurityPolicy(const String&);
    void setDomainRelaxationForbiddenForURLScheme(const String&);
    void setCanHandleHTTPSServerTrustEvaluation(bool);
    void registerURLSchemeAsLocal(const String&);
    void registerURLSchemeAsNoAccess(const String&);
    void registerURLSchemeAsDisplayIsolated(const String&);
    void registerURLSchemeAsCORSEnabled(const String&);
#if ENABLE(CACHE_PARTITIONING)
    void registerURLSchemeAsCachePartitioned(const String&);
#endif

    VisitedLinkProvider& visitedLinkProvider() { return m_visitedLinkProvider.get(); }

    void setCacheModel(CacheModel);
    CacheModel cacheModel() const { return m_configuration->cacheModel(); }

    void setDefaultRequestTimeoutInterval(double);

    void startMemorySampler(const double interval);
    void stopMemorySampler();

#if USE(SOUP)
    void setInitialHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy) { m_initialHTTPCookieAcceptPolicy = policy; }
#endif
    void setEnhancedAccessibility(bool);
    
    // Downloads.
    DownloadProxy* createDownloadProxy(const WebCore::ResourceRequest&);
    API::DownloadClient& downloadClient() { return *m_downloadClient; }

    API::LegacyContextHistoryClient& historyClient() { return *m_historyClient; }
    WebContextClient& client() { return m_client; }

    WebIconDatabase* iconDatabase() const { return m_iconDatabase.get(); }

    struct Statistics {
        unsigned wkViewCount;
        unsigned wkPageCount;
        unsigned wkFrameCount;
    };
    static Statistics& statistics();    

    void setIconDatabasePath(const String&);
    String iconDatabasePath() const;
    void setCookieStorageDirectory(const String& dir) { m_overrideCookieStorageDirectory = dir; }

    void useTestingNetworkSession();
    bool isUsingTestingNetworkSession() const { return m_shouldUseTestingNetworkSession; }

    void allowSpecificHTTPSCertificateForHost(const WebCertificateInfo*, const String& host);

    WebProcessProxy& ensureSharedWebProcess();
    WebProcessProxy& createNewWebProcessRespectingProcessCountLimit(); // Will return an existing one if limit is met.
    void warmInitialProcess();

    bool shouldTerminate(WebProcessProxy*);

    void disableProcessTermination() { m_processTerminationEnabled = false; }
    void enableProcessTermination();

    // Defaults to false.
    void setHTTPPipeliningEnabled(bool);
    bool httpPipeliningEnabled() const;

    void getStatistics(uint32_t statisticsMask, std::function<void (API::Dictionary*, CallbackBase::Error)>);
    
    void garbageCollectJavaScriptObjects();
    void setJavaScriptGarbageCollectorTimerEnabled(bool flag);

#if PLATFORM(COCOA)
    static bool omitPDFSupport();
#endif

    void fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled);

    void textCheckerStateChanged();

    PassRefPtr<API::Dictionary> plugInAutoStartOriginHashes() const;
    void setPlugInAutoStartOriginHashes(API::Dictionary&);
    void setPlugInAutoStartOrigins(API::Array&);
    void setPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(API::Dictionary&, double time);

    // Network Process Management

    void setUsesNetworkProcess(bool);
    bool usesNetworkProcess() const;

#if ENABLE(NETWORK_PROCESS)
    NetworkProcessProxy& ensureNetworkProcess();
    NetworkProcessProxy* networkProcess() { return m_networkProcess.get(); }
    void networkProcessCrashed(NetworkProcessProxy*);

    void getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>);
#endif

#if ENABLE(DATABASE_PROCESS)
    void ensureDatabaseProcess();
    DatabaseProcessProxy* databaseProcess() { return m_databaseProcess.get(); }
    void getDatabaseProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply>);
    void databaseProcessCrashed(DatabaseProcessProxy*);
#endif

#if PLATFORM(COCOA)
    bool processSuppressionEnabled() const;
#endif

    void windowServerConnectionStateChanged();

    static void willStartUsingPrivateBrowsing();
    static void willStopUsingPrivateBrowsing();

    static bool isEphemeralSession(WebCore::SessionID);

#if USE(SOUP)
    void setIgnoreTLSErrors(bool);
    bool ignoreTLSErrors() const { return m_ignoreTLSErrors; }
#endif

    static void setInvalidMessageCallback(void (*)(WKStringRef));
    static void didReceiveInvalidMessage(const IPC::StringReference& messageReceiverName, const IPC::StringReference& messageName);

    void processDidCachePage(WebProcessProxy*);

    bool isURLKnownHSTSHost(const String& urlString, bool privateBrowsingEnabled) const;
    void resetHSTSHosts();
    void resetHSTSHostsAddedAfterDate(double startDateIntervalSince1970);

    void registerSchemeForCustomProtocol(const String&);
    void unregisterSchemeForCustomProtocol(const String&);

    static HashSet<String>& globalURLSchemesWithCustomProtocolHandlers();
    static void registerGlobalURLSchemeAsHavingCustomProtocolHandlers(const String&);
    static void unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(const String&);

#if PLATFORM(COCOA)
    void updateProcessSuppressionState();

    NSMutableDictionary *ensureBundleParameters();
    NSMutableDictionary *bundleParameters() { return m_bundleParameters.get(); }
#else
    void updateProcessSuppressionState() const { }
#endif

    void setMemoryCacheDisabled(bool);
    void setFontWhitelist(API::Array*);

    UserObservablePageToken userObservablePageCount()
    {
        return m_userObservablePageCounter.token<UserObservablePageTokenType>();
    }

    ProcessSuppressionDisabledToken processSuppressionDisabledForPageCount()
    {
        return m_processSuppressionDisabledForPageCounter.token<ProcessSuppressionDisabledTokenType>();
    }

    // FIXME: Move these to API::WebsiteDataStore.
    static String legacyPlatformDefaultLocalStorageDirectory();
    static String legacyPlatformDefaultIndexedDBDatabaseDirectory();
    static String legacyPlatformDefaultWebSQLDatabaseDirectory();
    static String legacyPlatformDefaultMediaKeysStorageDirectory();
    static String legacyPlatformDefaultApplicationCacheDirectory();
    static String legacyPlatformDefaultNetworkCacheDirectory();
    static bool isNetworkCacheEnabled();

private:
    void platformInitialize();

    void platformInitializeWebProcess(WebProcessCreationParameters&);
    void platformInvalidateContext();

    WebProcessProxy& createNewWebProcess();

    void requestWebContentStatistics(StatisticsRequest*);
    void requestNetworkingStatistics(StatisticsRequest*);

#if ENABLE(NETWORK_PROCESS)
    void platformInitializeNetworkProcess(NetworkProcessCreationParameters&);
#endif

    void handleMessage(IPC::Connection&, const String& messageName, const UserData& messageBody);
    void handleSynchronousMessage(IPC::Connection&, const String& messageName, const UserData& messageBody, UserData& returnUserData);

    void didGetStatistics(const StatisticsData&, uint64_t callbackID);

    // IPC::MessageReceiver.
    // Implemented in generated WebProcessPoolMessageReceiver.cpp
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;

    static void languageChanged(void* context);
    void languageChanged();

    String platformDefaultIconDatabasePath() const;

#if PLATFORM(IOS) || ENABLE(SECCOMP_FILTERS)
    String cookieStorageDirectory() const;
#endif

#if PLATFORM(IOS)
    String parentBundleDirectory() const;
    String networkingCachesDirectory() const;
    String webContentCachesDirectory() const;
    String containerTemporaryDirectory() const;
#endif

#if PLATFORM(COCOA)
    void registerNotificationObservers();
    void unregisterNotificationObservers();
#endif

    void addPlugInAutoStartOriginHash(const String& pageOrigin, unsigned plugInOriginHash, WebCore::SessionID);
    void plugInDidReceiveUserInteraction(unsigned plugInOriginHash, WebCore::SessionID);

    void setAnyPageGroupMightHavePrivateBrowsingEnabled(bool);

#if ENABLE(NETSCAPE_PLUGIN_API)
    // PluginInfoStoreClient:
    virtual void pluginInfoStoreDidLoadPlugins(PluginInfoStore*) override;
#endif

    Ref<API::ProcessPoolConfiguration> m_configuration;

    IPC::MessageReceiverMap m_messageReceiverMap;

    Vector<RefPtr<WebProcessProxy>> m_processes;
    bool m_haveInitialEmptyProcess;

    WebProcessProxy* m_processWithPageCache;

    Ref<WebPageGroup> m_defaultPageGroup;

    RefPtr<API::Object> m_injectedBundleInitializationUserData;
    WebContextInjectedBundleClient m_injectedBundleClient;

    WebContextClient m_client;
    WebContextConnectionClient m_connectionClient;
    std::unique_ptr<API::DownloadClient> m_downloadClient;
    std::unique_ptr<API::LegacyContextHistoryClient> m_historyClient;

#if ENABLE(NETSCAPE_PLUGIN_API)
    PluginInfoStore m_pluginInfoStore;
#endif
    Ref<VisitedLinkProvider> m_visitedLinkProvider;
    bool m_visitedLinksPopulated;

    PlugInAutoStartProvider m_plugInAutoStartProvider;
        
    HashSet<String> m_schemesToRegisterAsEmptyDocument;
    HashSet<String> m_schemesToRegisterAsSecure;
    HashSet<String> m_schemesToRegisterAsBypassingContentSecurityPolicy;
    HashSet<String> m_schemesToSetDomainRelaxationForbiddenFor;
    HashSet<String> m_schemesToRegisterAsLocal;
    HashSet<String> m_schemesToRegisterAsNoAccess;
    HashSet<String> m_schemesToRegisterAsDisplayIsolated;
    HashSet<String> m_schemesToRegisterAsCORSEnabled;
#if ENABLE(CACHE_PARTITIONING)
    HashSet<String> m_schemesToRegisterAsCachePartitioned;
#endif

    bool m_alwaysUsesComplexTextCodePath;
    bool m_shouldUseFontSmoothing;

    Vector<String> m_fontWhitelist;

    // Messages that were posted before any pages were created.
    // The client should use initialization messages instead, so that a restarted process would get the same state.
    Vector<std::pair<String, RefPtr<API::Object>>> m_messagesToInjectedBundlePostedToEmptyContext;

    bool m_memorySamplerEnabled;
    double m_memorySamplerInterval;

    RefPtr<WebIconDatabase> m_iconDatabase;

    const RefPtr<API::WebsiteDataStore> m_websiteDataStore;

    typedef HashMap<const char*, RefPtr<WebContextSupplement>, PtrHash<const char*>> WebContextSupplementMap;
    WebContextSupplementMap m_supplements;

#if USE(SOUP)
    HTTPCookieAcceptPolicy m_initialHTTPCookieAcceptPolicy;
#endif

#if PLATFORM(MAC)
    RetainPtr<NSObject> m_enhancedAccessibilityObserver;
    RetainPtr<NSObject> m_automaticTextReplacementNotificationObserver;
    RetainPtr<NSObject> m_automaticSpellingCorrectionNotificationObserver;
    RetainPtr<NSObject> m_automaticQuoteSubstitutionNotificationObserver;
    RetainPtr<NSObject> m_automaticDashSubstitutionNotificationObserver;
#endif

    String m_overrideIconDatabasePath;
    String m_overrideCookieStorageDirectory;

    bool m_shouldUseTestingNetworkSession;

    bool m_processTerminationEnabled;

#if ENABLE(NETWORK_PROCESS)
    bool m_canHandleHTTPSServerTrustEvaluation;
    bool m_didNetworkProcessCrash;
    RefPtr<NetworkProcessProxy> m_networkProcess;
#endif

#if ENABLE(DATABASE_PROCESS)
    RefPtr<DatabaseProcessProxy> m_databaseProcess;
#endif
    
    HashMap<uint64_t, RefPtr<DictionaryCallback>> m_dictionaryCallbacks;
    HashMap<uint64_t, RefPtr<StatisticsRequest>> m_statisticsRequests;

#if USE(SOUP)
    bool m_ignoreTLSErrors;
#endif

    bool m_memoryCacheDisabled;

    RefCounter m_userObservablePageCounter;
    RefCounter m_processSuppressionDisabledForPageCounter;

#if PLATFORM(COCOA)
    RetainPtr<NSMutableDictionary> m_bundleParameters;
    ProcessSuppressionDisabledToken m_pluginProcessManagerProcessSuppressionDisabledToken;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    HashMap<String, String> m_encodedContentExtensions;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    HashMap<String, HashMap<String, HashMap<String, uint8_t>>> m_pluginLoadClientPolicies;
#endif
};

template<typename T>
void WebProcessPool::sendToNetworkingProcess(T&& message)
{
    switch (processModel()) {
    case ProcessModelSharedSecondaryProcess:
#if ENABLE(NETWORK_PROCESS)
        if (usesNetworkProcess()) {
            if (m_networkProcess && m_networkProcess->canSendMessage())
                m_networkProcess->send(std::forward<T>(message), 0);
            return;
        }
#endif
        if (!m_processes.isEmpty() && m_processes[0]->canSendMessage())
            m_processes[0]->send(std::forward<T>(message), 0);
        return;
    case ProcessModelMultipleSecondaryProcesses:
#if ENABLE(NETWORK_PROCESS)
        if (m_networkProcess && m_networkProcess->canSendMessage())
            m_networkProcess->send(std::forward<T>(message), 0);
        return;
#else
        break;
#endif
    }
    ASSERT_NOT_REACHED();
}

template<typename T>
void WebProcessPool::sendToNetworkingProcessRelaunchingIfNecessary(T&& message)
{
    switch (processModel()) {
    case ProcessModelSharedSecondaryProcess:
#if ENABLE(NETWORK_PROCESS)
        if (usesNetworkProcess()) {
            ensureNetworkProcess();
            m_networkProcess->send(std::forward<T>(message), 0);
            return;
        }
#endif
        ensureSharedWebProcess();
        m_processes[0]->send(std::forward<T>(message), 0);
        return;
    case ProcessModelMultipleSecondaryProcesses:
#if ENABLE(NETWORK_PROCESS)
        ensureNetworkProcess();
        m_networkProcess->send(std::forward<T>(message), 0);
        return;
#else
        break;
#endif
    }
    ASSERT_NOT_REACHED();
}

template<typename T>
void WebProcessPool::sendToDatabaseProcessRelaunchingIfNecessary(T&& message)
{
#if ENABLE(DATABASE_PROCESS)
    ensureDatabaseProcess();
    m_databaseProcess->send(std::forward<T>(message), 0);
#else
    sendToAllProcessesRelaunchingThemIfNecessary(std::forward<T>(message));
#endif
}

template<typename T>
void WebProcessPool::sendToAllProcesses(const T& message)
{
    size_t processCount = m_processes.size();
    for (size_t i = 0; i < processCount; ++i) {
        WebProcessProxy* process = m_processes[i].get();
        if (process->canSendMessage())
            process->send(T(message), 0);
    }
}

template<typename T>
void WebProcessPool::sendToAllProcessesRelaunchingThemIfNecessary(const T& message)
{
    // FIXME (Multi-WebProcess): WebProcessPool doesn't track processes that have exited, so it cannot relaunch these. Perhaps this functionality won't be needed in this mode.
    if (processModel() == ProcessModelSharedSecondaryProcess)
        ensureSharedWebProcess();
    sendToAllProcesses(message);
}

template<typename T>
void WebProcessPool::sendToOneProcess(T&& message)
{
    if (processModel() == ProcessModelSharedSecondaryProcess)
        ensureSharedWebProcess();

    bool messageSent = false;
    size_t processCount = m_processes.size();
    for (size_t i = 0; i < processCount; ++i) {
        WebProcessProxy* process = m_processes[i].get();
        if (process->canSendMessage()) {
            process->send(std::forward<T>(message), 0);
            messageSent = true;
            break;
        }
    }

    if (!messageSent && processModel() == ProcessModelMultipleSecondaryProcesses) {
        warmInitialProcess();
        RefPtr<WebProcessProxy> process = m_processes.last();
        if (process->canSendMessage())
            process->send(std::forward<T>(message), 0);
    }
}

} // namespace WebKit

#endif // UIProcessPool_h
