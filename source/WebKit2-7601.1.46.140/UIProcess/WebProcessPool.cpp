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

#include "config.h"
#include "WebProcessPool.h"

#include "APIArray.h"
#include "APIDownloadClient.h"
#include "APILegacyContextHistoryClient.h"
#include "APIProcessPoolConfiguration.h"
#include "CustomProtocolManagerMessages.h"
#include "DownloadProxy.h"
#include "DownloadProxyMessages.h"
#include "Logging.h"
#include "SandboxExtension.h"
#include "StatisticsData.h"
#include "TextChecker.h"
#include "WKContextPrivate.h"
#include "WebCertificateInfo.h"
#include "WebContextSupplement.h"
#include "WebCookieManagerProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebDatabaseManagerProxy.h"
#include "WebGeolocationManagerProxy.h"
#include "WebIconDatabase.h"
#include "WebKit2Initialize.h"
#include "WebMediaCacheManagerProxy.h"
#include "WebMemorySampler.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageGroup.h"
#include "WebPreferences.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/Language.h>
#include <WebCore/LinkHash.h>
#include <WebCore/Logging.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SessionID.h>
#include <runtime/JSCInlines.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

#if ENABLE(BATTERY_STATUS)
#include "WebBatteryManagerProxy.h"
#endif

#if ENABLE(DATABASE_PROCESS)
#include "DatabaseProcessCreationParameters.h"
#include "DatabaseProcessMessages.h"
#endif

#if ENABLE(NETWORK_PROCESS)
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#endif

#if ENABLE(SERVICE_CONTROLS)
#include "ServicesController.h"
#endif

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteInspector.h>
#endif

#if USE(SOUP)
#include "WebSoupCustomProtocolRequestManager.h"
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace WebCore;
using namespace WebKit;

namespace WebKit {

static const double sharedSecondaryProcessShutdownTimeout = 60;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, processPoolCounter, ("WebProcessPool"));

Ref<WebProcessPool> WebProcessPool::create(API::ProcessPoolConfiguration& configuration)
{
    InitializeWebKit2();
    return adoptRef(*new WebProcessPool(configuration));
}

static Vector<WebProcessPool*>& processPools()
{
    static NeverDestroyed<Vector<WebProcessPool*>> processPools;
    return processPools;
}

const Vector<WebProcessPool*>& WebProcessPool::allProcessPools()
{
    return processPools();
}

static WebsiteDataStore::Configuration legacyWebsiteDataStoreConfiguration(API::ProcessPoolConfiguration& processPoolConfiguration)
{
    WebsiteDataStore::Configuration configuration;

    configuration.localStorageDirectory = processPoolConfiguration.localStorageDirectory();
    configuration.webSQLDatabaseDirectory = processPoolConfiguration.webSQLDatabaseDirectory();
    configuration.applicationCacheDirectory = WebProcessPool::legacyPlatformDefaultApplicationCacheDirectory();
    configuration.mediaKeysStorageDirectory = WebProcessPool::legacyPlatformDefaultMediaKeysStorageDirectory();
    configuration.networkCacheDirectory = WebProcessPool::legacyPlatformDefaultNetworkCacheDirectory();

    return configuration;
}

WebProcessPool::WebProcessPool(API::ProcessPoolConfiguration& configuration)
    : m_configuration(configuration.copy())
    , m_haveInitialEmptyProcess(false)
    , m_processWithPageCache(0)
    , m_defaultPageGroup(WebPageGroup::createNonNull())
    , m_downloadClient(std::make_unique<API::DownloadClient>())
    , m_historyClient(std::make_unique<API::LegacyContextHistoryClient>())
    , m_visitedLinkProvider(VisitedLinkProvider::create())
    , m_visitedLinksPopulated(false)
    , m_plugInAutoStartProvider(this)
    , m_alwaysUsesComplexTextCodePath(false)
    , m_shouldUseFontSmoothing(true)
    , m_memorySamplerEnabled(false)
    , m_memorySamplerInterval(1400.0)
    , m_websiteDataStore(m_configuration->shouldHaveLegacyDataStore() ? API::WebsiteDataStore::create(legacyWebsiteDataStoreConfiguration(m_configuration)).ptr() : nullptr)
#if USE(SOUP)
    , m_initialHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain)
#endif
    , m_shouldUseTestingNetworkSession(false)
    , m_processTerminationEnabled(true)
#if ENABLE(NETWORK_PROCESS)
    , m_canHandleHTTPSServerTrustEvaluation(true)
    , m_didNetworkProcessCrash(false)
#endif
#if USE(SOUP)
    , m_ignoreTLSErrors(true)
#endif
    , m_memoryCacheDisabled(false)
    , m_userObservablePageCounter([this](bool) { updateProcessSuppressionState(); })
    , m_processSuppressionDisabledForPageCounter([this](bool) { updateProcessSuppressionState(); })
{
#if ENABLE(CACHE_PARTITIONING)
    for (const auto& urlScheme : m_configuration->cachePartitionedURLSchemes())
        m_schemesToRegisterAsCachePartitioned.add(urlScheme);
#endif

    platformInitialize();

    addMessageReceiver(Messages::WebProcessPool::messageReceiverName(), *this);

    // NOTE: These sub-objects must be initialized after m_messageReceiverMap..
    m_iconDatabase = WebIconDatabase::create(this);

    addSupplement<WebCookieManagerProxy>();
    addSupplement<WebGeolocationManagerProxy>();
    addSupplement<WebMediaCacheManagerProxy>();
    addSupplement<WebNotificationManagerProxy>();
    addSupplement<WebDatabaseManagerProxy>();
#if USE(SOUP)
    addSupplement<WebSoupCustomProtocolRequestManager>();
#endif
#if ENABLE(BATTERY_STATUS)
    addSupplement<WebBatteryManagerProxy>();
#endif

    processPools().append(this);

    addLanguageChangeObserver(this, languageChanged);

#if !LOG_DISABLED
    WebCore::initializeLoggingChannelsIfNecessary();
    WebKit::initializeLogChannelsIfNecessary();
#endif // !LOG_DISABLED

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginInfoStore.setClient(this);
#endif

#ifndef NDEBUG
    processPoolCounter.increment();
#endif
}

#if !PLATFORM(COCOA)
void WebProcessPool::platformInitialize()
{
}
#endif

WebProcessPool::~WebProcessPool()
{
    bool removed = processPools().removeFirst(this);
    ASSERT_UNUSED(removed, removed);

    removeLanguageChangeObserver(this);

    m_messageReceiverMap.invalidate();

    WebContextSupplementMap::const_iterator it = m_supplements.begin();
    WebContextSupplementMap::const_iterator end = m_supplements.end();
    for (; it != end; ++it) {
        it->value->processPoolDestroyed();
        it->value->clearProcessPool();
    }

    m_iconDatabase->invalidate();
    m_iconDatabase->clearProcessPool();
    WebIconDatabase* rawIconDatabase = m_iconDatabase.release().leakRef();
    rawIconDatabase->derefWhenAppropriate();

    invalidateCallbackMap(m_dictionaryCallbacks, CallbackBase::Error::OwnerWasInvalidated);

    platformInvalidateContext();

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginInfoStore.setClient(0);
#endif

#ifndef NDEBUG
    processPoolCounter.decrement();
#endif

#if ENABLE(NETWORK_PROCESS)
    if (m_networkProcess)
        m_networkProcess->shutDownProcess();
#endif
}

void WebProcessPool::initializeClient(const WKContextClientBase* client)
{
    m_client.initialize(client);
}

void WebProcessPool::initializeInjectedBundleClient(const WKContextInjectedBundleClientBase* client)
{
    m_injectedBundleClient.initialize(client);
}

void WebProcessPool::initializeConnectionClient(const WKContextConnectionClientBase* client)
{
    m_connectionClient.initialize(client);
}

void WebProcessPool::setHistoryClient(std::unique_ptr<API::LegacyContextHistoryClient> historyClient)
{
    if (!historyClient)
        m_historyClient = std::make_unique<API::LegacyContextHistoryClient>();
    else
        m_historyClient = WTF::move(historyClient);
}

void WebProcessPool::setDownloadClient(std::unique_ptr<API::DownloadClient> downloadClient)
{
    if (!downloadClient)
        m_downloadClient = std::make_unique<API::DownloadClient>();
    else
        m_downloadClient = WTF::move(downloadClient);
}

void WebProcessPool::setProcessModel(ProcessModel processModel)
{
    // Guard against API misuse.
    if (!m_processes.isEmpty())
        CRASH();
    if (processModel != ProcessModelSharedSecondaryProcess && !m_messagesToInjectedBundlePostedToEmptyContext.isEmpty())
        CRASH();

    m_configuration->setProcessModel(processModel);
}

void WebProcessPool::setMaximumNumberOfProcesses(unsigned maximumNumberOfProcesses)
{
    // Guard against API misuse.
    if (!m_processes.isEmpty())
        CRASH();

    m_configuration->setMaximumProcessCount(maximumNumberOfProcesses);
}

IPC::Connection* WebProcessPool::networkingProcessConnection()
{
    switch (processModel()) {
    case ProcessModelSharedSecondaryProcess:
#if ENABLE(NETWORK_PROCESS)
        if (usesNetworkProcess())
            return m_networkProcess->connection();
#endif
        return m_processes[0]->connection();
    case ProcessModelMultipleSecondaryProcesses:
#if ENABLE(NETWORK_PROCESS)
        ASSERT(usesNetworkProcess());
        return m_networkProcess->connection();
#else
        break;
#endif
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void WebProcessPool::languageChanged(void* context)
{
    static_cast<WebProcessPool*>(context)->languageChanged();
}

void WebProcessPool::languageChanged()
{
    sendToAllProcesses(Messages::WebProcess::UserPreferredLanguagesChanged(userPreferredLanguages()));
#if USE(SOUP) && ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess() && m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::UserPreferredLanguagesChanged(userPreferredLanguages()), 0);
#endif
}

void WebProcessPool::fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled)
{
    sendToAllProcesses(Messages::WebProcess::FullKeyboardAccessModeChanged(fullKeyboardAccessEnabled));
}

void WebProcessPool::textCheckerStateChanged()
{
    sendToAllProcesses(Messages::WebProcess::SetTextCheckerState(TextChecker::state()));
}

void WebProcessPool::setUsesNetworkProcess(bool usesNetworkProcess)
{
#if ENABLE(NETWORK_PROCESS)
    m_configuration->setUseNetworkProcess(usesNetworkProcess);
#else
    UNUSED_PARAM(usesNetworkProcess);
#endif
}

bool WebProcessPool::usesNetworkProcess() const
{
#if ENABLE(NETWORK_PROCESS)
    return m_configuration->useNetworkProcess();
#else
    return false;
#endif
}

#if ENABLE(NETWORK_PROCESS)
NetworkProcessProxy& WebProcessPool::ensureNetworkProcess()
{
    if (m_networkProcess)
        return *m_networkProcess;

    m_networkProcess = NetworkProcessProxy::create(*this);

    NetworkProcessCreationParameters parameters;

    parameters.privateBrowsingEnabled = WebPreferences::anyPagesAreUsingPrivateBrowsing();

    parameters.cacheModel = cacheModel();
    parameters.diskCacheSizeOverride = m_configuration->diskCacheSizeOverride();
    parameters.canHandleHTTPSServerTrustEvaluation = m_canHandleHTTPSServerTrustEvaluation;

    parameters.diskCacheDirectory = m_configuration->diskCacheDirectory();
    if (!parameters.diskCacheDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.diskCacheDirectory, parameters.diskCacheDirectoryExtensionHandle);

#if ENABLE(SECCOMP_FILTERS)
    parameters.cookieStorageDirectory = this->cookieStorageDirectory();
#endif

#if PLATFORM(IOS)
    String cookieStorageDirectory = this->cookieStorageDirectory();
    if (!cookieStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(cookieStorageDirectory, parameters.cookieStorageDirectoryExtensionHandle);

    String containerCachesDirectory = this->networkingCachesDirectory();
    if (!containerCachesDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(containerCachesDirectory, parameters.containerCachesDirectoryExtensionHandle);

    String parentBundleDirectory = this->parentBundleDirectory();
    if (!parentBundleDirectory.isEmpty())
        SandboxExtension::createHandle(parentBundleDirectory, SandboxExtension::ReadOnly, parameters.parentBundleDirectoryExtensionHandle);
#endif

    parameters.shouldUseTestingNetworkSession = m_shouldUseTestingNetworkSession;

    // Add any platform specific parameters
    platformInitializeNetworkProcess(parameters);

    // Initialize the network process.
    m_networkProcess->send(Messages::NetworkProcess::InitializeNetworkProcess(parameters), 0);

#if PLATFORM(COCOA)
    m_networkProcess->send(Messages::NetworkProcess::SetQOS(networkProcessLatencyQOS(), networkProcessThroughputQOS()), 0);
#endif

    if (m_didNetworkProcessCrash) {
        m_didNetworkProcessCrash = false;
        for (auto& process : m_processes)
            process->reinstateNetworkProcessAssertionState(*m_networkProcess);
    }

    return *m_networkProcess;
}

void WebProcessPool::networkProcessCrashed(NetworkProcessProxy* networkProcessProxy)
{
    ASSERT(m_networkProcess);
    ASSERT(networkProcessProxy == m_networkProcess.get());
    m_didNetworkProcessCrash = true;

    WebContextSupplementMap::const_iterator it = m_supplements.begin();
    WebContextSupplementMap::const_iterator end = m_supplements.end();
    for (; it != end; ++it)
        it->value->processDidClose(networkProcessProxy);

    m_client.networkProcessDidCrash(this);

    // Leave the process proxy around during client call, so that the client could query the process identifier.
    m_networkProcess = nullptr;
}

void WebProcessPool::getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply)
{
    ASSERT(reply);

    ensureNetworkProcess();
    ASSERT(m_networkProcess);

    m_networkProcess->getNetworkProcessConnection(reply);
}
#endif

#if ENABLE(DATABASE_PROCESS)
void WebProcessPool::ensureDatabaseProcess()
{
    if (m_databaseProcess)
        return;

    m_databaseProcess = DatabaseProcessProxy::create(this);

    ASSERT(!m_configuration->indexedDBDatabaseDirectory().isEmpty());

    // *********
    // IMPORTANT: Do not change the directory structure for indexed databases on disk without first consulting a reviewer from Apple (<rdar://problem/17454712>)
    // *********
    DatabaseProcessCreationParameters parameters;
    parameters.indexedDatabaseDirectory = m_configuration->indexedDBDatabaseDirectory();

    SandboxExtension::createHandleForReadWriteDirectory(parameters.indexedDatabaseDirectory, parameters.indexedDatabaseDirectoryExtensionHandle);

    m_databaseProcess->send(Messages::DatabaseProcess::InitializeDatabaseProcess(parameters), 0);
}

void WebProcessPool::getDatabaseProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply> reply)
{
    ASSERT(reply);

    ensureDatabaseProcess();

    m_databaseProcess->getDatabaseProcessConnection(reply);
}

void WebProcessPool::databaseProcessCrashed(DatabaseProcessProxy* databaseProcessProxy)
{
    ASSERT(m_databaseProcess);
    ASSERT(databaseProcessProxy == m_databaseProcess.get());

    for (auto& supplement : m_supplements)
        supplement.value->processDidClose(databaseProcessProxy);

    m_databaseProcess = nullptr;
}
#endif

void WebProcessPool::willStartUsingPrivateBrowsing()
{
    for (auto* processPool : allProcessPools())
        processPool->setAnyPageGroupMightHavePrivateBrowsingEnabled(true);
}

void WebProcessPool::willStopUsingPrivateBrowsing()
{
    for (auto* processPool : allProcessPools())
        processPool->setAnyPageGroupMightHavePrivateBrowsingEnabled(false);
}

void WebProcessPool::windowServerConnectionStateChanged()
{
    size_t processCount = m_processes.size();
    for (size_t i = 0; i < processCount; ++i)
        m_processes[i]->windowServerConnectionStateChanged();
}

void WebProcessPool::setAnyPageGroupMightHavePrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    m_iconDatabase->setPrivateBrowsingEnabled(privateBrowsingEnabled);

#if ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess() && networkProcess()) {
        if (privateBrowsingEnabled)
            networkProcess()->send(Messages::NetworkProcess::EnsurePrivateBrowsingSession(SessionID::legacyPrivateSessionID()), 0);
        else
            networkProcess()->send(Messages::NetworkProcess::DestroyPrivateBrowsingSession(SessionID::legacyPrivateSessionID()), 0);
    }
#endif // ENABLED(NETWORK_PROCESS)

    if (privateBrowsingEnabled)
        sendToAllProcesses(Messages::WebProcess::EnsurePrivateBrowsingSession(SessionID::legacyPrivateSessionID()));
    else
        sendToAllProcesses(Messages::WebProcess::DestroyPrivateBrowsingSession(SessionID::legacyPrivateSessionID()));
}

void (*s_invalidMessageCallback)(WKStringRef messageName);

void WebProcessPool::setInvalidMessageCallback(void (*invalidMessageCallback)(WKStringRef messageName))
{
    s_invalidMessageCallback = invalidMessageCallback;
}

void WebProcessPool::didReceiveInvalidMessage(const IPC::StringReference& messageReceiverName, const IPC::StringReference& messageName)
{
    if (!s_invalidMessageCallback)
        return;

    StringBuilder messageNameStringBuilder;
    messageNameStringBuilder.append(messageReceiverName.data(), messageReceiverName.size());
    messageNameStringBuilder.append('.');
    messageNameStringBuilder.append(messageName.data(), messageName.size());

    s_invalidMessageCallback(toAPI(API::String::create(messageNameStringBuilder.toString()).ptr()));
}

void WebProcessPool::processDidCachePage(WebProcessProxy* process)
{
    if (m_processWithPageCache && m_processWithPageCache != process)
        m_processWithPageCache->releasePageCache();
    m_processWithPageCache = process;
}

WebProcessProxy& WebProcessPool::ensureSharedWebProcess()
{
    ASSERT(processModel() == ProcessModelSharedSecondaryProcess);
    if (m_processes.isEmpty())
        createNewWebProcess();
    return *m_processes[0];
}

WebProcessProxy& WebProcessPool::createNewWebProcess()
{
#if ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess())
        ensureNetworkProcess();
#endif

    Ref<WebProcessProxy> process = WebProcessProxy::create(*this);

    WebProcessCreationParameters parameters;

    parameters.injectedBundlePath = injectedBundlePath();
    if (!parameters.injectedBundlePath.isEmpty())
        SandboxExtension::createHandle(parameters.injectedBundlePath, SandboxExtension::ReadOnly, parameters.injectedBundlePathExtensionHandle);

    parameters.applicationCacheDirectory = m_configuration->applicationCacheDirectory();
    if (!parameters.applicationCacheDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.applicationCacheDirectory, parameters.applicationCacheDirectoryExtensionHandle);

    parameters.webSQLDatabaseDirectory = m_configuration->webSQLDatabaseDirectory();
    if (!parameters.webSQLDatabaseDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.webSQLDatabaseDirectory, parameters.webSQLDatabaseDirectoryExtensionHandle);

#if ENABLE(SECCOMP_FILTERS)
    parameters.cookieStorageDirectory = this->cookieStorageDirectory();
#endif

#if PLATFORM(IOS)
    String cookieStorageDirectory = this->cookieStorageDirectory();
    if (!cookieStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(cookieStorageDirectory, parameters.cookieStorageDirectoryExtensionHandle);

    String containerCachesDirectory = this->webContentCachesDirectory();
    if (!containerCachesDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(containerCachesDirectory, parameters.containerCachesDirectoryExtensionHandle);

    String containerTemporaryDirectory = this->containerTemporaryDirectory();
    if (!containerTemporaryDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(containerTemporaryDirectory, parameters.containerTemporaryDirectoryExtensionHandle);
#endif

    parameters.mediaKeyStorageDirectory = m_configuration->mediaKeysStorageDirectory();
    if (!parameters.mediaKeyStorageDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.mediaKeyStorageDirectory, parameters.mediaKeyStorageDirectoryExtensionHandle);

    parameters.shouldUseTestingNetworkSession = m_shouldUseTestingNetworkSession;

    parameters.cacheModel = cacheModel();
    parameters.languages = userPreferredLanguages();

    copyToVector(m_schemesToRegisterAsEmptyDocument, parameters.urlSchemesRegisteredAsEmptyDocument);
    copyToVector(m_schemesToRegisterAsSecure, parameters.urlSchemesRegisteredAsSecure);
    copyToVector(m_schemesToRegisterAsBypassingContentSecurityPolicy, parameters.urlSchemesRegisteredAsBypassingContentSecurityPolicy);
    copyToVector(m_schemesToSetDomainRelaxationForbiddenFor, parameters.urlSchemesForWhichDomainRelaxationIsForbidden);
    copyToVector(m_schemesToRegisterAsLocal, parameters.urlSchemesRegisteredAsLocal);
    copyToVector(m_schemesToRegisterAsNoAccess, parameters.urlSchemesRegisteredAsNoAccess);
    copyToVector(m_schemesToRegisterAsDisplayIsolated, parameters.urlSchemesRegisteredAsDisplayIsolated);
    copyToVector(m_schemesToRegisterAsCORSEnabled, parameters.urlSchemesRegisteredAsCORSEnabled);
#if ENABLE(CACHE_PARTITIONING)
    copyToVector(m_schemesToRegisterAsCachePartitioned, parameters.urlSchemesRegisteredAsCachePartitioned);
#endif

    parameters.shouldAlwaysUseComplexTextCodePath = m_alwaysUsesComplexTextCodePath;
    parameters.shouldUseFontSmoothing = m_shouldUseFontSmoothing;

    // FIXME: This leaves UI process and WebProcess disagreeing about the state if the client hasn't set the path.
    // iconDatabasePath is non-empty by default, but m_iconDatabase isn't enabled in UI process unless setDatabasePath is called explicitly.
    parameters.iconDatabaseEnabled = !iconDatabasePath().isEmpty();

    parameters.terminationTimeout = (processModel() == ProcessModelSharedSecondaryProcess) ? sharedSecondaryProcessShutdownTimeout : 0;

    parameters.textCheckerState = TextChecker::state();

    parameters.fullKeyboardAccessEnabled = WebProcessProxy::fullKeyboardAccessEnabled();

    parameters.defaultRequestTimeoutInterval = API::URLRequest::defaultTimeoutInterval();

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    // FIXME: There should be a generic way for supplements to add to the intialization parameters.
    supplement<WebNotificationManagerProxy>()->populateCopyOfNotificationPermissions(parameters.notificationPermissions);
#endif

#if ENABLE(NETWORK_PROCESS)
    parameters.usesNetworkProcess = usesNetworkProcess();
#endif

    parameters.plugInAutoStartOriginHashes = m_plugInAutoStartProvider.autoStartOriginHashesCopy();
    copyToVector(m_plugInAutoStartProvider.autoStartOrigins(), parameters.plugInAutoStartOrigins);

    parameters.memoryCacheDisabled = m_memoryCacheDisabled;

#if ENABLE(SERVICE_CONTROLS)
    auto& serviceController = ServicesController::singleton();
    parameters.hasImageServices = serviceController.hasImageServices();
    parameters.hasSelectionServices = serviceController.hasSelectionServices();
    parameters.hasRichContentServices = serviceController.hasRichContentServices();
    serviceController.refreshExistingServices();
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
    parameters.pluginLoadClientPolicies = m_pluginLoadClientPolicies;
#endif

#if OS(LINUX)
    parameters.shouldEnableMemoryPressureReliefLogging = true;
#endif

    // Add any platform specific parameters
    platformInitializeWebProcess(parameters);

    RefPtr<API::Object> injectedBundleInitializationUserData = m_injectedBundleClient.getInjectedBundleInitializationUserData(this);
    if (!injectedBundleInitializationUserData)
        injectedBundleInitializationUserData = m_injectedBundleInitializationUserData;
    parameters.initializationUserData = UserData(process->transformObjectsToHandles(injectedBundleInitializationUserData.get()));

    process->send(Messages::WebProcess::InitializeWebProcess(parameters), 0);

#if PLATFORM(COCOA)
    process->send(Messages::WebProcess::SetQOS(webProcessLatencyQOS(), webProcessThroughputQOS()), 0);
#endif
#if PLATFORM(IOS)
    ApplicationCacheStorage::singleton().setDefaultOriginQuota(25ULL * 1024 * 1024);
#endif

    if (WebPreferences::anyPagesAreUsingPrivateBrowsing())
        process->send(Messages::WebProcess::EnsurePrivateBrowsingSession(SessionID::legacyPrivateSessionID()), 0);

    m_processes.append(process.ptr());

    if (processModel() == ProcessModelSharedSecondaryProcess) {
        for (size_t i = 0; i != m_messagesToInjectedBundlePostedToEmptyContext.size(); ++i) {
            auto& messageNameAndBody = m_messagesToInjectedBundlePostedToEmptyContext[i];

            process->send(Messages::WebProcess::HandleInjectedBundleMessage(messageNameAndBody.first, UserData(process->transformObjectsToHandles(messageNameAndBody.second.get()).get())), 0);
        }
        m_messagesToInjectedBundlePostedToEmptyContext.clear();
    } else
        ASSERT(m_messagesToInjectedBundlePostedToEmptyContext.isEmpty());

#if ENABLE(REMOTE_INSPECTOR)
    // Initialize remote inspector connection now that we have a sub-process that is hosting one of our web views.
    Inspector::RemoteInspector::singleton(); 
#endif

    return process;
}

void WebProcessPool::warmInitialProcess()  
{
    if (m_haveInitialEmptyProcess) {
        ASSERT(!m_processes.isEmpty());
        return;
    }

    if (m_processes.size() >= maximumNumberOfProcesses())
        return;

    createNewWebProcess();
    m_haveInitialEmptyProcess = true;
}

void WebProcessPool::enableProcessTermination()
{
    m_processTerminationEnabled = true;
    Vector<RefPtr<WebProcessProxy>> processes = m_processes;
    for (size_t i = 0; i < processes.size(); ++i) {
        if (shouldTerminate(processes[i].get()))
            processes[i]->terminate();
    }
}

bool WebProcessPool::shouldTerminate(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    if (!m_processTerminationEnabled)
        return false;

    for (const auto& supplement : m_supplements.values()) {
        if (!supplement->shouldTerminate(process))
            return false;
    }

    return true;
}

void WebProcessPool::processDidFinishLaunching(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    if (!m_visitedLinksPopulated) {
        populateVisitedLinks();
        m_visitedLinksPopulated = true;
    }

    // Sometimes the memorySampler gets initialized after process initialization has happened but before the process has finished launching
    // so check if it needs to be started here
    if (m_memorySamplerEnabled) {
        SandboxExtension::Handle sampleLogSandboxHandle;        
        double now = WTF::currentTime();
        String sampleLogFilePath = String::format("WebProcess%llupid%d", static_cast<unsigned long long>(now), process->processIdentifier());
        sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::ReadWrite, sampleLogSandboxHandle);
        
        process->send(Messages::WebProcess::StartMemorySampler(sampleLogSandboxHandle, sampleLogFilePath, m_memorySamplerInterval), 0);
    }

    m_connectionClient.didCreateConnection(this, process->webConnection());
}

void WebProcessPool::disconnectProcess(WebProcessProxy* process)
{
    ASSERT(m_processes.contains(process));

    if (m_haveInitialEmptyProcess && process == m_processes.last())
        m_haveInitialEmptyProcess = false;

    // FIXME (Multi-WebProcess): <rdar://problem/12239765> Some of the invalidation calls below are still necessary in multi-process mode, but they should only affect data structures pertaining to the process being disconnected.
    // Clearing everything causes assertion failures, so it's less trouble to skip that for now.
    if (processModel() != ProcessModelSharedSecondaryProcess) {
        RefPtr<WebProcessProxy> protect(process);
        if (m_processWithPageCache == process)
            m_processWithPageCache = 0;

        static_cast<WebContextSupplement*>(supplement<WebGeolocationManagerProxy>())->processDidClose(process);

        m_processes.removeFirst(process);
        return;
    }

    WebContextSupplementMap::const_iterator it = m_supplements.begin();
    WebContextSupplementMap::const_iterator end = m_supplements.end();
    for (; it != end; ++it)
        it->value->processDidClose(process);

    // The vector may have the last reference to process proxy, which in turn may have the last reference to the context.
    // Since vector elements are destroyed in place, we would recurse into WebProcessProxy destructor
    // if it were invoked from Vector::remove(). RefPtr delays destruction until it's safe.
    RefPtr<WebProcessProxy> protect(process);
    if (m_processWithPageCache == process)
        m_processWithPageCache = nullptr;
    m_processes.removeFirst(process);
}

WebProcessProxy& WebProcessPool::createNewWebProcessRespectingProcessCountLimit()
{
    if (m_processes.size() < maximumNumberOfProcesses())
        return createNewWebProcess();

    // Choose the process with fewest pages.
    auto& process = *std::min_element(m_processes.begin(), m_processes.end(), [](const RefPtr<WebProcessProxy>& a, const RefPtr<WebProcessProxy>& b) {
        return a->pageCount() < b->pageCount();
    });

    return *process;
}

PassRefPtr<WebPageProxy> WebProcessPool::createWebPage(PageClient& pageClient, WebPageConfiguration configuration)
{
    if (!configuration.pageGroup)
        configuration.pageGroup = m_defaultPageGroup.ptr();
    if (!configuration.preferences)
        configuration.preferences = &configuration.pageGroup->preferences();
    if (!configuration.visitedLinkProvider)
        configuration.visitedLinkProvider = m_visitedLinkProvider.ptr();
    if (!configuration.websiteDataStore) {
        ASSERT(!configuration.sessionID.isValid());
        configuration.websiteDataStore = &m_websiteDataStore->websiteDataStore();
        configuration.sessionID = configuration.preferences->privateBrowsingEnabled() ? SessionID::legacyPrivateSessionID() : SessionID::defaultSessionID();
    }

    RefPtr<WebProcessProxy> process;
    if (processModel() == ProcessModelSharedSecondaryProcess) {
        process = &ensureSharedWebProcess();
    } else {
        if (m_haveInitialEmptyProcess) {
            process = m_processes.last();
            m_haveInitialEmptyProcess = false;
        } else if (configuration.relatedPage) {
            // Sharing processes, e.g. when creating the page via window.open().
            process = &configuration.relatedPage->process();
        } else
            process = &createNewWebProcessRespectingProcessCountLimit();
    }

    return process->createWebPage(pageClient, WTF::move(configuration));
}

DownloadProxy* WebProcessPool::download(WebPageProxy* initiatingPage, const ResourceRequest& request)
{
    DownloadProxy* downloadProxy = createDownloadProxy(request);
    uint64_t initiatingPageID = initiatingPage ? initiatingPage->pageID() : 0;

#if ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess() && networkProcess()) {
        // FIXME (NetworkProcess): Replicate whatever FrameLoader::setOriginalURLForDownloadRequest does with the request here.
        networkProcess()->send(Messages::NetworkProcess::DownloadRequest(downloadProxy->downloadID(), request), 0);
        return downloadProxy;
    }
#endif

    m_processes[0]->send(Messages::WebProcess::DownloadRequest(downloadProxy->downloadID(), initiatingPageID, request), 0);
    return downloadProxy;
}

DownloadProxy* WebProcessPool::resumeDownload(const API::Data* resumeData, const String& path)
{
    DownloadProxy* downloadProxy = createDownloadProxy(ResourceRequest());

    SandboxExtension::Handle sandboxExtensionHandle;
    if (!path.isEmpty())
        SandboxExtension::createHandle(path, SandboxExtension::ReadWrite, sandboxExtensionHandle);

#if ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess() && networkProcess()) {
        networkProcess()->send(Messages::NetworkProcess::ResumeDownload(downloadProxy->downloadID(), resumeData->dataReference(), path, sandboxExtensionHandle), 0);
        return downloadProxy;
    }
#endif

    m_processes[0]->send(Messages::WebProcess::ResumeDownload(downloadProxy->downloadID(), resumeData->dataReference(), path, sandboxExtensionHandle), 0);
    return downloadProxy;
}

void WebProcessPool::postMessageToInjectedBundle(const String& messageName, API::Object* messageBody)
{
    if (m_processes.isEmpty()) {
        if (processModel() == ProcessModelSharedSecondaryProcess)
            m_messagesToInjectedBundlePostedToEmptyContext.append(std::make_pair(messageName, messageBody));
        return;
    }

    for (auto& process : m_processes) {
        // FIXME: Return early if the message body contains any references to WKPageRefs/WKFrameRefs etc. since they're local to a process.
        process->send(Messages::WebProcess::HandleInjectedBundleMessage(messageName, UserData(process->transformObjectsToHandles(messageBody).get())), 0);
    }
}

void WebProcessPool::populateVisitedLinks()
{
    m_historyClient->populateVisitedLinks(*this);
}

WebProcessPool::Statistics& WebProcessPool::statistics()
{
    static Statistics statistics = Statistics();

    return statistics;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessPool::setAdditionalPluginsDirectory(const String& directory)
{
    Vector<String> directories;
    directories.append(directory);

    m_pluginInfoStore.setAdditionalPluginsDirectories(directories);
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(NETWORK_PROCESS)
PlatformProcessIdentifier WebProcessPool::networkProcessIdentifier()
{
    if (!m_networkProcess)
        return 0;

    return m_networkProcess->processIdentifier();
}
#endif

void WebProcessPool::setAlwaysUsesComplexTextCodePath(bool alwaysUseComplexText)
{
    m_alwaysUsesComplexTextCodePath = alwaysUseComplexText;
    sendToAllProcesses(Messages::WebProcess::SetAlwaysUsesComplexTextCodePath(alwaysUseComplexText));
}

void WebProcessPool::setShouldUseFontSmoothing(bool useFontSmoothing)
{
    m_shouldUseFontSmoothing = useFontSmoothing;
    sendToAllProcesses(Messages::WebProcess::SetShouldUseFontSmoothing(useFontSmoothing));
}

void WebProcessPool::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    m_schemesToRegisterAsEmptyDocument.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsEmptyDocument(urlScheme));
}

void WebProcessPool::registerURLSchemeAsSecure(const String& urlScheme)
{
    m_schemesToRegisterAsSecure.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsSecure(urlScheme));
}

void WebProcessPool::registerURLSchemeAsBypassingContentSecurityPolicy(const String& urlScheme)
{
    m_schemesToRegisterAsBypassingContentSecurityPolicy.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsBypassingContentSecurityPolicy(urlScheme));
}

void WebProcessPool::setDomainRelaxationForbiddenForURLScheme(const String& urlScheme)
{
    m_schemesToSetDomainRelaxationForbiddenFor.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::SetDomainRelaxationForbiddenForURLScheme(urlScheme));
}

void WebProcessPool::setCanHandleHTTPSServerTrustEvaluation(bool value)
{
#if ENABLE(NETWORK_PROCESS)
    m_canHandleHTTPSServerTrustEvaluation = value;
    if (usesNetworkProcess() && m_networkProcess) {
        m_networkProcess->send(Messages::NetworkProcess::SetCanHandleHTTPSServerTrustEvaluation(value), 0);
        return;
    }
#else
    UNUSED_PARAM(value);
#endif
}

void WebProcessPool::registerURLSchemeAsLocal(const String& urlScheme)
{
    m_schemesToRegisterAsLocal.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsLocal(urlScheme));
}

void WebProcessPool::registerURLSchemeAsNoAccess(const String& urlScheme)
{
    m_schemesToRegisterAsNoAccess.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsNoAccess(urlScheme));
}

void WebProcessPool::registerURLSchemeAsDisplayIsolated(const String& urlScheme)
{
    m_schemesToRegisterAsDisplayIsolated.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsDisplayIsolated(urlScheme));
}

void WebProcessPool::registerURLSchemeAsCORSEnabled(const String& urlScheme)
{
    m_schemesToRegisterAsCORSEnabled.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCORSEnabled(urlScheme));
}

HashSet<String>& WebProcessPool::globalURLSchemesWithCustomProtocolHandlers()
{
    static NeverDestroyed<HashSet<String>> set;
    return set;
}

void WebProcessPool::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(const String& urlScheme)
{
    if (!urlScheme)
        return;

    String schemeLower = urlScheme.lower();
    globalURLSchemesWithCustomProtocolHandlers().add(schemeLower);
    for (auto* processPool : allProcessPools())
        processPool->registerSchemeForCustomProtocol(schemeLower);
}

void WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(const String& urlScheme)
{
    if (!urlScheme)
        return;

    String schemeLower = urlScheme.lower();
    globalURLSchemesWithCustomProtocolHandlers().remove(schemeLower);
    for (auto* processPool : allProcessPools())
        processPool->unregisterSchemeForCustomProtocol(schemeLower);
}

#if ENABLE(CACHE_PARTITIONING)
void WebProcessPool::registerURLSchemeAsCachePartitioned(const String& urlScheme)
{
    m_schemesToRegisterAsCachePartitioned.add(urlScheme);
    sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsCachePartitioned(urlScheme));
}
#endif

void WebProcessPool::setCacheModel(CacheModel cacheModel)
{
    m_configuration->setCacheModel(cacheModel);
    sendToAllProcesses(Messages::WebProcess::SetCacheModel(cacheModel));

#if ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess() && m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::SetCacheModel(cacheModel), 0);
#endif
}

void WebProcessPool::setDefaultRequestTimeoutInterval(double timeoutInterval)
{
    sendToAllProcesses(Messages::WebProcess::SetDefaultRequestTimeoutInterval(timeoutInterval));
}

DownloadProxy* WebProcessPool::createDownloadProxy(const ResourceRequest& request)
{
#if ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess()) {
        ensureNetworkProcess();
        ASSERT(m_networkProcess);
        return m_networkProcess->createDownloadProxy(request);
    }
#endif

    return ensureSharedWebProcess().createDownloadProxy(request);
}

void WebProcessPool::addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void WebProcessPool::addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void WebProcessPool::removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

bool WebProcessPool::dispatchMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder)
{
    return m_messageReceiverMap.dispatchMessage(connection, decoder);
}

bool WebProcessPool::dispatchSyncMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{
    return m_messageReceiverMap.dispatchSyncMessage(connection, decoder, replyEncoder);
}

void WebProcessPool::setEnhancedAccessibility(bool flag)
{
    sendToAllProcesses(Messages::WebProcess::SetEnhancedAccessibility(flag));
}
    
void WebProcessPool::startMemorySampler(const double interval)
{    
    // For new WebProcesses we will also want to start the Memory Sampler
    m_memorySamplerEnabled = true;
    m_memorySamplerInterval = interval;
    
    // For UIProcess
#if ENABLE(MEMORY_SAMPLER)
    WebMemorySampler::singleton()->start(interval);
#endif
    
    // For WebProcess
    SandboxExtension::Handle sampleLogSandboxHandle;    
    double now = WTF::currentTime();
    String sampleLogFilePath = String::format("WebProcess%llu", static_cast<unsigned long long>(now));
    sampleLogFilePath = SandboxExtension::createHandleForTemporaryFile(sampleLogFilePath, SandboxExtension::ReadWrite, sampleLogSandboxHandle);
    
    sendToAllProcesses(Messages::WebProcess::StartMemorySampler(sampleLogSandboxHandle, sampleLogFilePath, interval));
}

void WebProcessPool::stopMemorySampler()
{    
    // For WebProcess
    m_memorySamplerEnabled = false;
    
    // For UIProcess
#if ENABLE(MEMORY_SAMPLER)
    WebMemorySampler::singleton()->stop();
#endif

    sendToAllProcesses(Messages::WebProcess::StopMemorySampler());
}

void WebProcessPool::setIconDatabasePath(const String& path)
{
    m_overrideIconDatabasePath = path;
    if (!m_overrideIconDatabasePath.isEmpty()) {
        // This implicitly enables the database on UI process side.
        m_iconDatabase->setDatabasePath(path);
    }
}

String WebProcessPool::iconDatabasePath() const
{
    if (!m_overrideIconDatabasePath.isNull())
        return m_overrideIconDatabasePath;

    return platformDefaultIconDatabasePath();
}

#if ENABLE(SECCOMP_FILTERS)
String WebProcessPool::cookieStorageDirectory() const
{
    if (!m_overrideCookieStorageDirectory.isEmpty())
        return m_overrideCookieStorageDirectory;

    // FIXME: This doesn't make much sense. Is this function used at all? We used to call platform code, but no existing platforms implemented that function.
    return emptyString();
}
#endif

void WebProcessPool::useTestingNetworkSession()
{
    ASSERT(m_processes.isEmpty());
#if ENABLE(NETWORK_PROCESS)
    ASSERT(!m_networkProcess);

    if (m_networkProcess)
        return;
#endif

    if (!m_processes.isEmpty())
        return;

    m_shouldUseTestingNetworkSession = true;
}

void WebProcessPool::allowSpecificHTTPSCertificateForHost(const WebCertificateInfo* certificate, const String& host)
{
#if ENABLE(NETWORK_PROCESS)
    if (usesNetworkProcess() && m_networkProcess) {
        m_networkProcess->send(Messages::NetworkProcess::AllowSpecificHTTPSCertificateForHost(certificate->certificateInfo(), host), 0);
        return;
    }
#endif

#if USE(SOUP)
    m_processes[0]->send(Messages::WebProcess::AllowSpecificHTTPSCertificateForHost(certificate->certificateInfo(), host), 0);
    return;
#else
    UNUSED_PARAM(certificate);
    UNUSED_PARAM(host);
#endif

#if !PLATFORM(IOS)
    ASSERT_NOT_REACHED();
#endif
}

void WebProcessPool::setHTTPPipeliningEnabled(bool enabled)
{
#if PLATFORM(COCOA)
    ResourceRequest::setHTTPPipeliningEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

bool WebProcessPool::httpPipeliningEnabled() const
{
#if PLATFORM(COCOA)
    return ResourceRequest::httpPipeliningEnabled();
#else
    return false;
#endif
}

void WebProcessPool::getStatistics(uint32_t statisticsMask, std::function<void (API::Dictionary*, CallbackBase::Error)> callbackFunction)
{
    if (!statisticsMask) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    RefPtr<StatisticsRequest> request = StatisticsRequest::create(DictionaryCallback::create(WTF::move(callbackFunction)));

    if (statisticsMask & StatisticsRequestTypeWebContent)
        requestWebContentStatistics(request.get());
    
    if (statisticsMask & StatisticsRequestTypeNetworking)
        requestNetworkingStatistics(request.get());
}

void WebProcessPool::requestWebContentStatistics(StatisticsRequest* request)
{
    if (processModel() == ProcessModelSharedSecondaryProcess) {
        if (m_processes.isEmpty())
            return;
        
        uint64_t requestID = request->addOutstandingRequest();
        m_statisticsRequests.set(requestID, request);
        m_processes[0]->send(Messages::WebProcess::GetWebCoreStatistics(requestID), 0);

    } else {
        // FIXME (Multi-WebProcess) <rdar://problem/13200059>: Make getting statistics from multiple WebProcesses work.
    }
}

void WebProcessPool::requestNetworkingStatistics(StatisticsRequest* request)
{
    bool networkProcessUnavailable;
#if ENABLE(NETWORK_PROCESS)
    networkProcessUnavailable = !usesNetworkProcess() || !m_networkProcess;
#else
    networkProcessUnavailable = true;
#endif

    if (networkProcessUnavailable) {
        LOG_ERROR("Attempt to get NetworkProcess statistics but the NetworkProcess is unavailable");
        return;
    }

#if ENABLE(NETWORK_PROCESS)
    uint64_t requestID = request->addOutstandingRequest();
    m_statisticsRequests.set(requestID, request);
    m_networkProcess->send(Messages::NetworkProcess::GetNetworkProcessStatistics(requestID), 0);
#else
    UNUSED_PARAM(request);
#endif
}

void WebProcessPool::handleMessage(IPC::Connection& connection, const String& messageName, const WebKit::UserData& messageBody)
{
    auto* webProcessProxy = WebProcessProxy::fromConnection(&connection);
    if (!webProcessProxy)
        return;
    m_injectedBundleClient.didReceiveMessageFromInjectedBundle(this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.object()).get());
}

void WebProcessPool::handleSynchronousMessage(IPC::Connection& connection, const String& messageName, const UserData& messageBody, UserData& returnUserData)
{
    auto* webProcessProxy = WebProcessProxy::fromConnection(&connection);
    if (!webProcessProxy)
        return;

    RefPtr<API::Object> returnData;
    m_injectedBundleClient.didReceiveSynchronousMessageFromInjectedBundle(this, messageName, webProcessProxy->transformHandlesToObjects(messageBody.object()).get(), returnData);
    returnUserData = UserData(webProcessProxy->transformObjectsToHandles(returnData.get()));
}

void WebProcessPool::didGetStatistics(const StatisticsData& statisticsData, uint64_t requestID)
{
    RefPtr<StatisticsRequest> request = m_statisticsRequests.take(requestID);
    if (!request) {
        LOG_ERROR("Cannot report networking statistics.");
        return;
    }

    request->completedRequest(requestID, statisticsData);
}

void WebProcessPool::garbageCollectJavaScriptObjects()
{
    sendToAllProcesses(Messages::WebProcess::GarbageCollectJavaScriptObjects());
}

void WebProcessPool::setJavaScriptGarbageCollectorTimerEnabled(bool flag)
{
    sendToAllProcesses(Messages::WebProcess::SetJavaScriptGarbageCollectorTimerEnabled(flag));
}

void WebProcessPool::addPlugInAutoStartOriginHash(const String& pageOrigin, unsigned plugInOriginHash, SessionID sessionID)
{
    m_plugInAutoStartProvider.addAutoStartOriginHash(pageOrigin, plugInOriginHash, sessionID);
}

void WebProcessPool::plugInDidReceiveUserInteraction(unsigned plugInOriginHash, SessionID sessionID)
{
    m_plugInAutoStartProvider.didReceiveUserInteraction(plugInOriginHash, sessionID);
}

PassRefPtr<API::Dictionary> WebProcessPool::plugInAutoStartOriginHashes() const
{
    return m_plugInAutoStartProvider.autoStartOriginsTableCopy();
}

void WebProcessPool::setPlugInAutoStartOriginHashes(API::Dictionary& dictionary)
{
    m_plugInAutoStartProvider.setAutoStartOriginsTable(dictionary);
}

void WebProcessPool::setPlugInAutoStartOrigins(API::Array& array)
{
    m_plugInAutoStartProvider.setAutoStartOriginsArray(array);
}

void WebProcessPool::setPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(API::Dictionary& dictionary, double time)
{
    m_plugInAutoStartProvider.setAutoStartOriginsFilteringOutEntriesAddedAfterTime(dictionary, time);
}

void WebProcessPool::registerSchemeForCustomProtocol(const String& scheme)
{
    sendToNetworkingProcess(Messages::CustomProtocolManager::RegisterScheme(scheme));
}

void WebProcessPool::unregisterSchemeForCustomProtocol(const String& scheme)
{
    sendToNetworkingProcess(Messages::CustomProtocolManager::UnregisterScheme(scheme));
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebProcessPool::pluginInfoStoreDidLoadPlugins(PluginInfoStore* store)
{
#ifdef NDEBUG
    UNUSED_PARAM(store);
#endif
    ASSERT(store == &m_pluginInfoStore);

    Vector<PluginModuleInfo> pluginModules = m_pluginInfoStore.plugins();

    Vector<RefPtr<API::Object>> plugins;
    plugins.reserveInitialCapacity(pluginModules.size());

    for (const auto& pluginModule : pluginModules) {
        API::Dictionary::MapType map;
        map.set(ASCIILiteral("path"), API::String::create(pluginModule.path));
        map.set(ASCIILiteral("name"), API::String::create(pluginModule.info.name));
        map.set(ASCIILiteral("file"), API::String::create(pluginModule.info.file));
        map.set(ASCIILiteral("desc"), API::String::create(pluginModule.info.desc));

        Vector<RefPtr<API::Object>> mimeTypes;
        mimeTypes.reserveInitialCapacity(pluginModule.info.mimes.size());
        for (const auto& mimeClassInfo : pluginModule.info.mimes)
            mimeTypes.uncheckedAppend(API::String::create(mimeClassInfo.type));
        map.set(ASCIILiteral("mimes"), API::Array::create(WTF::move(mimeTypes)));

#if PLATFORM(COCOA)
        map.set(ASCIILiteral("bundleId"), API::String::create(pluginModule.bundleIdentifier));
        map.set(ASCIILiteral("version"), API::String::create(pluginModule.versionString));
#endif

        plugins.uncheckedAppend(API::Dictionary::create(WTF::move(map)));
    }

    m_client.plugInInformationBecameAvailable(this, API::Array::create(WTF::move(plugins)).ptr());
}

void WebProcessPool::setPluginLoadClientPolicy(WebCore::PluginLoadClientPolicy policy, const String& host, const String& bundleIdentifier, const String& versionString)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    HashMap<String, HashMap<String, uint8_t>> policiesByIdentifier;
    if (m_pluginLoadClientPolicies.contains(host))
        policiesByIdentifier = m_pluginLoadClientPolicies.get(host);

    HashMap<String, uint8_t> versionsToPolicies;
    if (policiesByIdentifier.contains(bundleIdentifier))
        versionsToPolicies = policiesByIdentifier.get(bundleIdentifier);

    versionsToPolicies.set(versionString, policy);
    policiesByIdentifier.set(bundleIdentifier, versionsToPolicies);
    m_pluginLoadClientPolicies.set(host, policiesByIdentifier);
#endif

    sendToAllProcesses(Messages::WebProcess::SetPluginLoadClientPolicy(policy, host, bundleIdentifier, versionString));
}

void WebProcessPool::clearPluginClientPolicies()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    m_pluginLoadClientPolicies.clear();
#endif
    sendToAllProcesses(Messages::WebProcess::ClearPluginClientPolicies());
}
#endif
    
void WebProcessPool::setMemoryCacheDisabled(bool disabled)
{
    m_memoryCacheDisabled = disabled;
    sendToAllProcesses(Messages::WebProcess::SetMemoryCacheDisabled(disabled));
}

void WebProcessPool::setFontWhitelist(API::Array* array)
{
    m_fontWhitelist.clear();
    if (array) {
        for (size_t i = 0; i < array->size(); ++i) {
            if (API::String* font = array->at<API::String>(i))
                m_fontWhitelist.append(font->string());
        }
    }
}

} // namespace WebKit
