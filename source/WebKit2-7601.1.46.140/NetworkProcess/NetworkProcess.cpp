/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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
#include "NetworkProcess.h"

#if ENABLE(NETWORK_PROCESS)

#include "ArgumentCoders.h"
#include "Attachment.h"
#include "AuthenticationManager.h"
#include "ChildProcessMessages.h"
#include "CustomProtocolManager.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessPlatformStrategies.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkResourceLoader.h"
#include "RemoteNetworkingContext.h"
#include "SecurityOriginData.h"
#include "SessionTracker.h"
#include "StatisticsData.h"
#include "WebCookieManager.h"
#include "WebProcessPoolMessages.h"
#include "WebsiteData.h"
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/Logging.h>
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/SessionID.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShim.h"
#endif

#if ENABLE(NETWORK_CACHE)
#include "NetworkCache.h"
#include "NetworkCacheCoders.h"
#endif

using namespace WebCore;

namespace WebKit {

NetworkProcess& NetworkProcess::singleton()
{
    static NeverDestroyed<NetworkProcess> networkProcess;
    return networkProcess;
}

NetworkProcess::NetworkProcess()
    : m_hasSetCacheModel(false)
    , m_cacheModel(CacheModelDocumentViewer)
    , m_diskCacheIsDisabledForTesting(false)
    , m_canHandleHTTPSServerTrustEvaluation(true)
#if PLATFORM(COCOA)
    , m_clearCacheDispatchGroup(0)
#endif
#if PLATFORM(IOS)
    , m_webSQLiteDatabaseTracker(*this)
#endif
{
    NetworkProcessPlatformStrategies::initialize();

    addSupplement<AuthenticationManager>();
    addSupplement<WebCookieManager>();
    addSupplement<CustomProtocolManager>();
}

NetworkProcess::~NetworkProcess()
{
}

AuthenticationManager& NetworkProcess::authenticationManager()
{
    return *supplement<AuthenticationManager>();
}

DownloadManager& NetworkProcess::downloadManager()
{
    static NeverDestroyed<DownloadManager> downloadManager(this);
    return downloadManager;
}

void NetworkProcess::removeNetworkConnectionToWebProcess(NetworkConnectionToWebProcess* connection)
{
    size_t vectorIndex = m_webProcessConnections.find(connection);
    ASSERT(vectorIndex != notFound);

    m_webProcessConnections.remove(vectorIndex);
}

bool NetworkProcess::shouldTerminate()
{
    // Network process keeps session cookies and credentials, so it should never terminate (as long as UI process connection is alive).
    return false;
}

void NetworkProcess::didReceiveMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder)
{
    if (messageReceiverMap().dispatchMessage(connection, decoder))
        return;

    if (decoder.messageReceiverName() == Messages::ChildProcess::messageReceiverName()) {
        ChildProcess::didReceiveMessage(connection, decoder);
        return;
    }

    didReceiveNetworkProcessMessage(connection, decoder);
}

void NetworkProcess::didReceiveSyncMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{
    if (messageReceiverMap().dispatchSyncMessage(connection, decoder, replyEncoder))
        return;

    didReceiveSyncNetworkProcessMessage(connection, decoder, replyEncoder);
}

void NetworkProcess::didClose(IPC::Connection&)
{
    // The UIProcess just exited.
    RunLoop::current().stop();
}

void NetworkProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference)
{
    RunLoop::current().stop();
}

void NetworkProcess::didCreateDownload()
{
    disableTermination();
}

void NetworkProcess::didDestroyDownload()
{
    enableTermination();
}

IPC::Connection* NetworkProcess::downloadProxyConnection()
{
    return parentProcessConnection();
}

AuthenticationManager& NetworkProcess::downloadsAuthenticationManager()
{
    return authenticationManager();
}

void NetworkProcess::lowMemoryHandler(Critical critical)
{
    platformLowMemoryHandler(critical);
    WTF::releaseFastMallocFreeMemory();
}

void NetworkProcess::initializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
    platformInitializeNetworkProcess(parameters);

    WTF::setCurrentThreadIsUserInitiated();

    auto& memoryPressureHandler = MemoryPressureHandler::singleton();
    memoryPressureHandler.setLowMemoryHandler([this] (Critical critical, Synchronous) {
        lowMemoryHandler(critical);
    });
    memoryPressureHandler.install();

    m_diskCacheIsDisabledForTesting = parameters.shouldUseTestingNetworkSession;

    m_diskCacheSizeOverride = parameters.diskCacheSizeOverride;
    setCacheModel(static_cast<uint32_t>(parameters.cacheModel));

    setCanHandleHTTPSServerTrustEvaluation(parameters.canHandleHTTPSServerTrustEvaluation);

#if PLATFORM(MAC) || USE(CFNETWORK)
    SessionTracker::setIdentifierBase(parameters.uiProcessBundleIdentifier);
#endif

    // FIXME: instead of handling this here, a message should be sent later (scales to multiple sessions)
    if (parameters.privateBrowsingEnabled)
        RemoteNetworkingContext::ensurePrivateBrowsingSession(SessionID::legacyPrivateSessionID());

    if (parameters.shouldUseTestingNetworkSession)
        NetworkStorageSession::switchToNewTestingSession();

    NetworkProcessSupplementMap::const_iterator it = m_supplements.begin();
    NetworkProcessSupplementMap::const_iterator end = m_supplements.end();
    for (; it != end; ++it)
        it->value->initialize(parameters);
}

void NetworkProcess::initializeConnection(IPC::Connection* connection)
{
    ChildProcess::initializeConnection(connection);

#if ENABLE(SEC_ITEM_SHIM)
    SecItemShim::singleton().initializeConnection(connection);
#endif

    NetworkProcessSupplementMap::const_iterator it = m_supplements.begin();
    NetworkProcessSupplementMap::const_iterator end = m_supplements.end();
    for (; it != end; ++it)
        it->value->initializeConnection(connection);
}

void NetworkProcess::createNetworkConnectionToWebProcess()
{
#if OS(DARWIN)
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    RefPtr<NetworkConnectionToWebProcess> connection = NetworkConnectionToWebProcess::create(IPC::Connection::Identifier(listeningPort));
    m_webProcessConnections.append(connection.release());

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidCreateNetworkConnectionToWebProcess(clientPort), 0);
#elif USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection();

    RefPtr<NetworkConnectionToWebProcess> connection = NetworkConnectionToWebProcess::create(socketPair.server);
    m_webProcessConnections.append(connection.release());

    IPC::Attachment clientSocket(socketPair.client);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidCreateNetworkConnectionToWebProcess(clientSocket), 0);
#else
    notImplemented();
#endif
}

void NetworkProcess::ensurePrivateBrowsingSession(SessionID sessionID)
{
    RemoteNetworkingContext::ensurePrivateBrowsingSession(sessionID);
}

void NetworkProcess::destroyPrivateBrowsingSession(SessionID sessionID)
{
    SessionTracker::destroySession(sessionID);
}

static void fetchDiskCacheEntries(SessionID sessionID, std::function<void (Vector<WebsiteData::Entry>)> completionHandler)
{
#if ENABLE(NETWORK_CACHE)
    if (NetworkCache::singleton().isEnabled()) {
        auto* origins = new HashSet<RefPtr<SecurityOrigin>>();

        NetworkCache::singleton().traverse([completionHandler, origins](const NetworkCache::Entry *entry) {
            if (!entry) {
                Vector<WebsiteData::Entry> entries;

                for (auto& origin : *origins)
                    entries.append(WebsiteData::Entry { origin, WebsiteDataTypeDiskCache });

                delete origins;

                RunLoop::main().dispatch([completionHandler, entries] {
                    completionHandler(entries);
                });

                return;
            }

            origins->add(SecurityOrigin::create(entry->response().url()));
        });

        return;
    }
#endif

    Vector<WebsiteData::Entry> entries;

#if USE(CFURLCACHE)
    for (auto& origin : NetworkProcess::cfURLCacheOrigins())
        entries.append(WebsiteData::Entry { WTF::move(origin), WebsiteDataTypeDiskCache });
#endif

    RunLoop::main().dispatch([completionHandler, entries] {
        completionHandler(entries);
    });
}

void NetworkProcess::fetchWebsiteData(SessionID sessionID, uint64_t websiteDataTypes, uint64_t callbackID)
{
    struct CallbackAggregator final : public RefCounted<CallbackAggregator> {
        explicit CallbackAggregator(std::function<void (WebsiteData)> completionHandler)
            : m_completionHandler(WTF::move(completionHandler))
        {
        }

        ~CallbackAggregator()
        {
            ASSERT(RunLoop::isMain());

            auto completionHandler = WTF::move(m_completionHandler);
            auto websiteData = WTF::move(m_websiteData);

            RunLoop::main().dispatch([completionHandler, websiteData] {
                completionHandler(websiteData);
            });
        }

        std::function<void (WebsiteData)> m_completionHandler;
        WebsiteData m_websiteData;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator([this, callbackID](WebsiteData websiteData) {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidFetchWebsiteData(callbackID, websiteData), 0);
    }));

    if (websiteDataTypes & WebsiteDataTypeCookies) {
        if (auto* networkStorageSession = SessionTracker::session(sessionID))
            getHostnamesWithCookies(*networkStorageSession, callbackAggregator->m_websiteData.hostNamesWithCookies);
    }

    if (websiteDataTypes & WebsiteDataTypeDiskCache) {
        fetchDiskCacheEntries(sessionID, [callbackAggregator](Vector<WebsiteData::Entry> entries) {
            callbackAggregator->m_websiteData.entries.appendVector(entries);
        });
    }
}

void NetworkProcess::deleteWebsiteData(SessionID sessionID, uint64_t websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID)
{
#if PLATFORM(COCOA)
    if (websiteDataTypes & WebsiteDataTypeHSTSCache) {
        if (auto* networkStorageSession = SessionTracker::session(sessionID))
            clearHSTSCache(*networkStorageSession, modifiedSince);
    }
#endif

    if (websiteDataTypes & WebsiteDataTypeCookies) {
        if (auto* networkStorageSession = SessionTracker::session(sessionID))
            deleteAllCookiesModifiedSince(*networkStorageSession, modifiedSince);
    }

    auto completionHandler = [this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteData(callbackID), 0);
    };

    if ((websiteDataTypes & WebsiteDataTypeDiskCache) && !sessionID.isEphemeral()) {
        clearDiskCache(modifiedSince, WTF::move(completionHandler));
        return;
    }

    completionHandler();
}

static void clearDiskCacheEntries(const Vector<SecurityOriginData>& origins, std::function<void ()> completionHandler)
{
#if ENABLE(NETWORK_CACHE)
    if (NetworkCache::singleton().isEnabled()) {
        auto* originsToDelete = new HashSet<RefPtr<SecurityOrigin>>();

        for (auto& origin : origins)
            originsToDelete->add(origin.securityOrigin());

        auto* cacheKeysToDelete = new Vector<NetworkCache::Key>;

        NetworkCache::singleton().traverse([completionHandler, originsToDelete, cacheKeysToDelete](const NetworkCache::Entry *entry) {

            if (entry) {
                if (originsToDelete->contains(SecurityOrigin::create(entry->response().url())))
                    cacheKeysToDelete->append(entry->key());
                return;
            }

            delete originsToDelete;

            for (auto& key : *cacheKeysToDelete)
                NetworkCache::singleton().remove(key);

            delete cacheKeysToDelete;

            RunLoop::main().dispatch(completionHandler);
            return;
        });

        return;
    }
#endif

#if USE(CFURLCACHE)
    NetworkProcess::clearCFURLCacheForOrigins(origins);
#endif

    RunLoop::main().dispatch(WTF::move(completionHandler));
}

void NetworkProcess::deleteWebsiteDataForOrigins(SessionID sessionID, uint64_t websiteDataTypes, const Vector<SecurityOriginData>& origins, const Vector<String>& cookieHostNames, uint64_t callbackID)
{
    if (websiteDataTypes & WebsiteDataTypeCookies) {
        if (auto* networkStorageSession = SessionTracker::session(sessionID))
            deleteCookiesForHostnames(*networkStorageSession, cookieHostNames);
    }

    auto completionHandler = [this, callbackID] {
        parentProcessConnection()->send(Messages::NetworkProcessProxy::DidDeleteWebsiteDataForOrigins(callbackID), 0);
    };

    if ((websiteDataTypes & WebsiteDataTypeDiskCache) && !sessionID.isEphemeral()) {
        clearDiskCacheEntries(origins, WTF::move(completionHandler));
        return;
    }

    completionHandler();
}

void NetworkProcess::downloadRequest(uint64_t downloadID, const ResourceRequest& request)
{
    downloadManager().startDownload(downloadID, request);
}

void NetworkProcess::resumeDownload(uint64_t downloadID, const IPC::DataReference& resumeData, const String& path, const WebKit::SandboxExtension::Handle& sandboxExtensionHandle)
{
    downloadManager().resumeDownload(downloadID, resumeData, path, sandboxExtensionHandle);
}

void NetworkProcess::cancelDownload(uint64_t downloadID)
{
    downloadManager().cancelDownload(downloadID);
}

void NetworkProcess::setCacheModel(uint32_t cm)
{
    CacheModel cacheModel = static_cast<CacheModel>(cm);

    if (!m_hasSetCacheModel || cacheModel != m_cacheModel) {
        m_hasSetCacheModel = true;
        m_cacheModel = cacheModel;
        platformSetCacheModel(cacheModel);
    }
}

void NetworkProcess::setCanHandleHTTPSServerTrustEvaluation(bool value)
{
    m_canHandleHTTPSServerTrustEvaluation = value;
}

void NetworkProcess::getNetworkProcessStatistics(uint64_t callbackID)
{
    StatisticsData data;

    auto& networkProcess = NetworkProcess::singleton();
    data.statisticsNumbers.set("DownloadsActiveCount", networkProcess.downloadManager().activeDownloadCount());
    data.statisticsNumbers.set("OutstandingAuthenticationChallengesCount", networkProcess.authenticationManager().outstandingAuthenticationChallengeCount());

    parentProcessConnection()->send(Messages::WebProcessPool::DidGetStatistics(data, callbackID), 0);
}

void NetworkProcess::logDiagnosticMessage(uint64_t webPageID, const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogSampledDiagnosticMessage(webPageID, message, description), 0);
}

void NetworkProcess::logDiagnosticMessageWithResult(uint64_t webPageID, const String& message, const String& description, WebCore::DiagnosticLoggingResultType result, WebCore::ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogSampledDiagnosticMessageWithResult(webPageID, message, description, result), 0);
}

void NetworkProcess::logDiagnosticMessageWithValue(uint64_t webPageID, const String& message, const String& description, const String& value, WebCore::ShouldSample shouldSample)
{
    if (!DiagnosticLoggingClient::shouldLogAfterSampling(shouldSample))
        return;

    parentProcessConnection()->send(Messages::NetworkProcessProxy::LogSampledDiagnosticMessageWithValue(webPageID, message, description, value), 0);
}

void NetworkProcess::terminate()
{
    platformTerminate();
    ChildProcess::terminate();
}

void NetworkProcess::processWillSuspendImminently(bool& handled)
{
    lowMemoryHandler(Critical::Yes);
    handled = true;
}

void NetworkProcess::prepareToSuspend()
{
    lowMemoryHandler(Critical::Yes);
    parentProcessConnection()->send(Messages::NetworkProcessProxy::ProcessReadyToSuspend(), 0);
}

void NetworkProcess::cancelPrepareToSuspend()
{
    parentProcessConnection()->send(Messages::NetworkProcessProxy::DidCancelProcessSuspension(), 0);
}

void NetworkProcess::processDidResume()
{
}

#if !PLATFORM(COCOA)
void NetworkProcess::initializeProcess(const ChildProcessInitializationParameters&)
{
}

void NetworkProcess::initializeProcessName(const ChildProcessInitializationParameters&)
{
}

void NetworkProcess::initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&)
{
}

void NetworkProcess::platformLowMemoryHandler(Critical)
{
}
#endif

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
