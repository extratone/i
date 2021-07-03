/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebsiteDataStore.h"

#include "APIProcessPoolConfiguration.h"
#include "APIWebsiteDataRecord.h"
#include "NetworkProcessMessages.h"
#include "StorageManager.h"
#include "WebProcessPool.h"
#include "WebsiteData.h"
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/OriginLock.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/RunLoop.h>

#if ENABLE(NETSCAPE_PLUGIN_API)
#include "PluginProcessManager.h"
#endif

namespace WebKit {

static WebCore::SessionID generateNonPersistentSessionID()
{
    // FIXME: We count backwards here to not conflict with API::Session.
    static uint64_t sessionID = std::numeric_limits<uint64_t>::max();

    return WebCore::SessionID(--sessionID);
}

static uint64_t generateIdentifier()
{
    static uint64_t identifier;

    return ++identifier;
}

Ref<WebsiteDataStore> WebsiteDataStore::createNonPersistent()
{
    return adoptRef(*new WebsiteDataStore(generateNonPersistentSessionID()));
}

Ref<WebsiteDataStore> WebsiteDataStore::create(Configuration configuration)
{
    return adoptRef(*new WebsiteDataStore(WTF::move(configuration)));
}

WebsiteDataStore::WebsiteDataStore(Configuration configuration)
    : m_identifier(generateIdentifier())
    , m_sessionID(WebCore::SessionID::defaultSessionID())
    , m_networkCacheDirectory(WTF::move(configuration.networkCacheDirectory))
    , m_applicationCacheDirectory(WTF::move(configuration.applicationCacheDirectory))
    , m_webSQLDatabaseDirectory(WTF::move(configuration.webSQLDatabaseDirectory))
    , m_mediaKeysStorageDirectory(WTF::move(configuration.mediaKeysStorageDirectory))
    , m_storageManager(StorageManager::create(WTF::move(configuration.localStorageDirectory)))
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
{
    platformInitialize();
}

WebsiteDataStore::WebsiteDataStore(WebCore::SessionID sessionID)
    : m_identifier(generateIdentifier())
    , m_sessionID(sessionID)
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
{
    platformInitialize();
}

WebsiteDataStore::~WebsiteDataStore()
{
    platformDestroy();

#if ENABLE(NETWORK_PROCESS)
    if (m_sessionID.isEphemeral()) {
        for (auto& processPool : WebProcessPool::allProcessPools())
            processPool->sendToNetworkingProcess(Messages::NetworkProcess::DestroyPrivateBrowsingSession(m_sessionID));
    }
#endif
}

void WebsiteDataStore::cloneSessionData(WebPageProxy& sourcePage, WebPageProxy& newPage)
{
    auto& sourceDataStore = sourcePage.websiteDataStore();
    auto& newDataStore = newPage.websiteDataStore();

    // FIXME: Handle this.
    if (&sourceDataStore != &newDataStore)
        return;

    if (!sourceDataStore.m_storageManager)
        return;

    sourceDataStore.m_storageManager->cloneSessionStorageNamespace(sourcePage.pageID(), newPage.pageID());
}

enum class ProcessAccessType {
    None,
    OnlyIfLaunched,
    Launch,
};

static ProcessAccessType computeNetworkProcessAccessTypeForDataFetch(WebsiteDataTypes dataTypes, bool isNonPersistentStore)
{
    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes & WebsiteDataTypeCookies) {
        if (isNonPersistentStore)
            processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);
        else
            processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
    }

    if (dataTypes & WebsiteDataTypeDiskCache && !isNonPersistentStore)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

    return processAccessType;
}

static ProcessAccessType computeWebProcessAccessTypeForDataFetch(WebsiteDataTypes dataTypes, bool isNonPersistentStore)
{
    UNUSED_PARAM(isNonPersistentStore);

    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes & WebsiteDataTypeMemoryCache)
        return ProcessAccessType::OnlyIfLaunched;

    return processAccessType;
}

void WebsiteDataStore::fetchData(WebsiteDataTypes dataTypes, std::function<void (Vector<WebsiteDataRecord>)> completionHandler)
{
    struct CallbackAggregator final : ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(std::function<void (Vector<WebsiteDataRecord>)> completionHandler)
            : completionHandler(WTF::move(completionHandler))
        {
        }

        ~CallbackAggregator()
        {
            ASSERT(!pendingCallbacks);
        }

        void addPendingCallback()
        {
            pendingCallbacks++;
        }

        void removePendingCallback(WebsiteData websiteData)
        {
            ASSERT(pendingCallbacks);
            --pendingCallbacks;

            for (auto& entry : websiteData.entries) {
                auto displayName = WebsiteDataRecord::displayNameForOrigin(*entry.origin);
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTF::move(displayName);

                record.add(entry.type, WTF::move(entry.origin));
            }

            for (auto& hostName : websiteData.hostNamesWithCookies) {
                auto displayName = WebsiteDataRecord::displayNameForCookieHostName(hostName);
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTF::move(displayName);

                record.addCookieHostName(hostName);
            }

#if ENABLE(NETSCAPE_PLUGIN_API)
            for (auto& hostName : websiteData.hostNamesWithPluginData) {
                auto displayName = WebsiteDataRecord::displayNameForPluginDataHostName(hostName);
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTF::move(displayName);

                record.addPluginDataHostName(hostName);
            }
#endif

            callIfNeeded();
        }

        void callIfNeeded()
        {
            if (pendingCallbacks)
                return;

            RefPtr<CallbackAggregator> callbackAggregator(this);
            RunLoop::main().dispatch([callbackAggregator] {

                WTF::Vector<WebsiteDataRecord> records;
                records.reserveInitialCapacity(callbackAggregator->m_websiteDataRecords.size());

                for (auto& record : callbackAggregator->m_websiteDataRecords.values())
                    records.uncheckedAppend(WTF::move(record));

                callbackAggregator->completionHandler(WTF::move(records));
            });
        }

        unsigned pendingCallbacks = 0;
        std::function<void (Vector<WebsiteDataRecord>)> completionHandler;

        HashMap<String, WebsiteDataRecord> m_websiteDataRecords;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTF::move(completionHandler)));

    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataFetch(dataTypes, !isPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        for (auto processPool : processPools()) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!processPool->networkProcess())
                    continue;
                break;

            case ProcessAccessType::Launch:
                processPool->ensureNetworkProcess();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            processPool->networkProcess()->fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator, processPool](WebsiteData websiteData) {
                callbackAggregator->removePendingCallback(WTF::move(websiteData));
            });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataFetch(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!process->canSendMessage())
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            process->fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator](WebsiteData websiteData) {
                callbackAggregator->removePendingCallback(WTF::move(websiteData));
            });
        }
    }

    if (dataTypes & WebsiteDataTypeSessionStorage && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->getSessionStorageOrigins([callbackAggregator](HashSet<RefPtr<WebCore::SecurityOrigin>>&& origins) {
            WebsiteData websiteData;

            while (!origins.isEmpty())
                websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataTypeSessionStorage });

            callbackAggregator->removePendingCallback(WTF::move(websiteData));
        });
    }

    if (dataTypes & WebsiteDataTypeLocalStorage && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->getLocalStorageOrigins([callbackAggregator](HashSet<RefPtr<WebCore::SecurityOrigin>>&& origins) {
            WebsiteData websiteData;

            while (!origins.isEmpty())
                websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataTypeLocalStorage });

            callbackAggregator->removePendingCallback(WTF::move(websiteData));
        });
    }

    if (dataTypes & WebsiteDataTypeOfflineWebApplicationCache && isPersistent()) {
        StringCapture applicationCacheDirectory { m_applicationCacheDirectory };

        callbackAggregator->addPendingCallback();

        m_queue->dispatch([applicationCacheDirectory, callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory.string(), "Files");

            HashSet<RefPtr<WebCore::SecurityOrigin>> origins;
            storage->getOriginsWithCache(origins);

            WTF::RunLoop::main().dispatch([callbackAggregator, origins]() mutable {
                WebsiteData websiteData;

                for (auto& origin : origins)
                    websiteData.entries.append(WebsiteData::Entry { origin, WebsiteDataTypeOfflineWebApplicationCache });

                callbackAggregator->removePendingCallback(WTF::move(websiteData));
            });
        });
    }

    if (dataTypes & WebsiteDataTypeWebSQLDatabases && isPersistent()) {
        StringCapture webSQLDatabaseDirectory { m_webSQLDatabaseDirectory };

        callbackAggregator->addPendingCallback();

        m_queue->dispatch([webSQLDatabaseDirectory, callbackAggregator] {
            Vector<RefPtr<WebCore::SecurityOrigin>> origins;
            WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory.string())->origins(origins);

            RunLoop::main().dispatch([callbackAggregator, origins]() mutable {
                WebsiteData websiteData;
                for (auto& origin : origins)
                    websiteData.entries.append(WebsiteData::Entry { WTF::move(origin), WebsiteDataTypeWebSQLDatabases });

                callbackAggregator->removePendingCallback(WTF::move(websiteData));
            });
        });
    }

#if ENABLE(DATABASE_PROCESS)
    if (dataTypes & WebsiteDataTypeIndexedDBDatabases && isPersistent()) {
        for (auto processPool : processPools()) {
            processPool->ensureDatabaseProcess();

            callbackAggregator->addPendingCallback();
            processPool->databaseProcess()->fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator, processPool](WebsiteData websiteData) {
                callbackAggregator->removePendingCallback(WTF::move(websiteData));
            });
        }
    }
#endif

    if (dataTypes & WebsiteDataTypeMediaKeys && isPersistent()) {
        StringCapture mediaKeysStorageDirectory { m_mediaKeysStorageDirectory };

        callbackAggregator->addPendingCallback();

        m_queue->dispatch([mediaKeysStorageDirectory, callbackAggregator] {
            auto origins = mediaKeyOrigins(mediaKeysStorageDirectory.string());

            RunLoop::main().dispatch([callbackAggregator, origins]() mutable {
                WebsiteData websiteData;
                for (auto& origin : origins)
                    websiteData.entries.append(WebsiteData::Entry { WTF::move(origin), WebsiteDataTypeMediaKeys });

                callbackAggregator->removePendingCallback(WTF::move(websiteData));
            });
        });
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (dataTypes & WebsiteDataTypePlugInData && isPersistent()) {
        class State {
        public:
            static void fetchData(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins)
            {
                new State(WTF::move(callbackAggregator), WTF::move(plugins));
            }

        private:
            State(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins)
                : m_callbackAggregator(WTF::move(callbackAggregator))
                , m_plugins(WTF::move(plugins))
            {
                m_callbackAggregator->addPendingCallback();

                fetchWebsiteDataForNextPlugin();
            }

            ~State()
            {
                ASSERT(m_plugins.isEmpty());
            }

            void fetchWebsiteDataForNextPlugin()
            {
                if (m_plugins.isEmpty()) {
                    WebsiteData websiteData;
                    websiteData.hostNamesWithPluginData = WTF::move(m_hostNames);

                    m_callbackAggregator->removePendingCallback(WTF::move(websiteData));

                    delete this;
                    return;
                }

                auto plugin = m_plugins.takeLast();
                PluginProcessManager::singleton().fetchWebsiteData(plugin, [this](Vector<String> hostNames) {
                    for (auto& hostName : hostNames)
                        m_hostNames.add(WTF::move(hostName));
                    fetchWebsiteDataForNextPlugin();
                });
            }

            Ref<CallbackAggregator> m_callbackAggregator;
            Vector<PluginModuleInfo> m_plugins;
            HashSet<String> m_hostNames;
        };

        State::fetchData(*callbackAggregator, plugins());
    }
#endif

    callbackAggregator->callIfNeeded();
}

static ProcessAccessType computeNetworkProcessAccessTypeForDataRemoval(WebsiteDataTypes dataTypes, bool isNonPersistentStore)
{
    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes & WebsiteDataTypeCookies) {
        if (isNonPersistentStore)
            processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);
        else
            processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
    }

    if (dataTypes & WebsiteDataTypeDiskCache && !isNonPersistentStore)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

    if (dataTypes & WebsiteDataTypeHSTSCache)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

    return processAccessType;
}

static ProcessAccessType computeWebProcessAccessTypeForDataRemoval(WebsiteDataTypes dataTypes, bool isNonPersistentStore)
{
    UNUSED_PARAM(isNonPersistentStore);

    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes & WebsiteDataTypeMemoryCache)
        processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);

    return processAccessType;
}

void WebsiteDataStore::removeData(WebsiteDataTypes dataTypes, std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler)
{
    struct CallbackAggregator : ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator (std::function<void ()> completionHandler)
            : completionHandler(WTF::move(completionHandler))
        {
        }

        void addPendingCallback()
        {
            pendingCallbacks++;
        }

        void removePendingCallback()
        {
            ASSERT(pendingCallbacks);
            --pendingCallbacks;

            callIfNeeded();
        }

        void callIfNeeded()
        {
            if (!pendingCallbacks)
                RunLoop::main().dispatch(WTF::move(completionHandler));
        }

        unsigned pendingCallbacks = 0;
        std::function<void ()> completionHandler;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTF::move(completionHandler)));

    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        for (auto processPool : processPools()) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!processPool->networkProcess())
                    continue;
                break;

            case ProcessAccessType::Launch:
                processPool->ensureNetworkProcess();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            processPool->networkProcess()->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator, processPool] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!process->canSendMessage())
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            process->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    if (dataTypes & WebsiteDataTypeSessionStorage && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->deleteSessionStorageOrigins([callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes & WebsiteDataTypeLocalStorage && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->deleteLocalStorageOriginsModifiedSince(modifiedSince, [callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes & WebsiteDataTypeOfflineWebApplicationCache && isPersistent()) {
        StringCapture applicationCacheDirectory { m_applicationCacheDirectory };

        callbackAggregator->addPendingCallback();

        m_queue->dispatch([applicationCacheDirectory, callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory.string(), "Files");

            storage->deleteAllEntries();

            WTF::RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if (dataTypes & WebsiteDataTypeWebSQLDatabases && isPersistent()) {
        StringCapture webSQLDatabaseDirectory { m_webSQLDatabaseDirectory };

        callbackAggregator->addPendingCallback();

        m_queue->dispatch([webSQLDatabaseDirectory, callbackAggregator, modifiedSince] {
            WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory.string())->deleteDatabasesModifiedSince(modifiedSince);

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

#if ENABLE(DATABASE_PROCESS)
    if (dataTypes & WebsiteDataTypeIndexedDBDatabases && isPersistent()) {
        for (auto processPool : processPools()) {
            processPool->ensureDatabaseProcess();

            callbackAggregator->addPendingCallback();
            processPool->databaseProcess()->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator, processPool] {
                callbackAggregator->removePendingCallback();
            });
        }
    }
#endif

    if (dataTypes & WebsiteDataTypeMediaKeys && isPersistent()) {
        StringCapture mediaKeysStorageDirectory { m_mediaKeysStorageDirectory };

        callbackAggregator->addPendingCallback();

        m_queue->dispatch([mediaKeysStorageDirectory, callbackAggregator, modifiedSince] {
            removeMediaKeys(mediaKeysStorageDirectory.string(), modifiedSince);

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (dataTypes & WebsiteDataTypePlugInData && isPersistent()) {
        class State {
        public:
            static void deleteData(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, std::chrono::system_clock::time_point modifiedSince)
            {
                new State(WTF::move(callbackAggregator), WTF::move(plugins), modifiedSince);
            }

        private:
            State(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, std::chrono::system_clock::time_point modifiedSince)
                : m_callbackAggregator(WTF::move(callbackAggregator))
                , m_plugins(WTF::move(plugins))
                , m_modifiedSince(modifiedSince)
            {
                m_callbackAggregator->addPendingCallback();

                deleteWebsiteDataForNextPlugin();
            }

            ~State()
            {
                ASSERT(m_plugins.isEmpty());
            }

            void deleteWebsiteDataForNextPlugin()
            {
                if (m_plugins.isEmpty()) {
                    m_callbackAggregator->removePendingCallback();

                    delete this;
                    return;
                }

                auto plugin = m_plugins.takeLast();
                PluginProcessManager::singleton().deleteWebsiteData(plugin, m_modifiedSince, [this] {
                    deleteWebsiteDataForNextPlugin();
                });
            }

            Ref<CallbackAggregator> m_callbackAggregator;
            Vector<PluginModuleInfo> m_plugins;
            std::chrono::system_clock::time_point m_modifiedSince;
        };

        State::deleteData(*callbackAggregator, plugins(), modifiedSince);
    }
#endif

    // There's a chance that we don't have any pending callbacks. If so, we want to dispatch the completion handler right away.
    callbackAggregator->callIfNeeded();
}

void WebsiteDataStore::removeData(WebsiteDataTypes dataTypes, const Vector<WebsiteDataRecord>& dataRecords, std::function<void ()> completionHandler)
{
    Vector<RefPtr<WebCore::SecurityOrigin>> origins;

    for (const auto& dataRecord : dataRecords) {
        for (auto& origin : dataRecord.origins)
            origins.append(origin);
    }

    struct CallbackAggregator : ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator (std::function<void ()> completionHandler)
            : completionHandler(WTF::move(completionHandler))
        {
        }

        void addPendingCallback()
        {
            pendingCallbacks++;
        }

        void removePendingCallback()
        {
            ASSERT(pendingCallbacks);
            --pendingCallbacks;

            callIfNeeded();
        }

        void callIfNeeded()
        {
            if (!pendingCallbacks)
                RunLoop::main().dispatch(WTF::move(completionHandler));
        }

        unsigned pendingCallbacks = 0;
        std::function<void ()> completionHandler;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTF::move(completionHandler)));

    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        for (auto processPool : processPools()) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!processPool->networkProcess())
                    continue;
                break;

            case ProcessAccessType::Launch:
                processPool->ensureNetworkProcess();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            Vector<String> cookieHostNames;
            for (const auto& dataRecord : dataRecords) {
                for (auto& hostName : dataRecord.cookieHostNames)
                    cookieHostNames.append(hostName);
            }

            callbackAggregator->addPendingCallback();
            processPool->networkProcess()->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, cookieHostNames, [callbackAggregator, processPool] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!process->canSendMessage())
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();

            process->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    if (dataTypes & WebsiteDataTypeSessionStorage && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->deleteSessionStorageEntriesForOrigins(origins, [callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes & WebsiteDataTypeLocalStorage && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->deleteLocalStorageEntriesForOrigins(origins, [callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes & WebsiteDataTypeOfflineWebApplicationCache && isPersistent()) {
        StringCapture applicationCacheDirectory { m_applicationCacheDirectory };

        HashSet<RefPtr<WebCore::SecurityOrigin>> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins, applicationCacheDirectory, callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory.string(), "Files");

            for (const auto& origin : origins)
                storage->deleteCacheForOrigin(*origin);

            WTF::RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if (dataTypes & WebsiteDataTypeWebSQLDatabases && isPersistent()) {
        StringCapture webSQLDatabaseDirectory { m_webSQLDatabaseDirectory };

        HashSet<RefPtr<WebCore::SecurityOrigin>> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins, callbackAggregator, webSQLDatabaseDirectory] {
            auto databaseTracker = WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory.string());

            for (const auto& origin : origins)
                databaseTracker->deleteOrigin(origin.get());

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

#if ENABLE(DATABASE_PROCESS)
    if (dataTypes & WebsiteDataTypeIndexedDBDatabases && isPersistent()) {
        for (auto processPool : processPools()) {
            processPool->ensureDatabaseProcess();

            callbackAggregator->addPendingCallback();
            processPool->databaseProcess()->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, [callbackAggregator, processPool] {
                callbackAggregator->removePendingCallback();
            });
        }
    }
#endif

    if (dataTypes & WebsiteDataTypeMediaKeys && isPersistent()) {
        StringCapture mediaKeysStorageDirectory { m_mediaKeysStorageDirectory };
        HashSet<RefPtr<WebCore::SecurityOrigin>> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([mediaKeysStorageDirectory, callbackAggregator, origins] {

            removeMediaKeys(mediaKeysStorageDirectory.string(), origins);

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (dataTypes & WebsiteDataTypePlugInData && isPersistent()) {
        Vector<String> hostNames;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& hostName : dataRecord.pluginDataHostNames)
                hostNames.append(hostName);
        }


        class State {
        public:
            static void deleteData(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, Vector<String>&& hostNames)
            {
                new State(WTF::move(callbackAggregator), WTF::move(plugins), WTF::move(hostNames));
            }

        private:
            State(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, Vector<String>&& hostNames)
                : m_callbackAggregator(WTF::move(callbackAggregator))
                , m_plugins(WTF::move(plugins))
                , m_hostNames(WTF::move(hostNames))
            {
                m_callbackAggregator->addPendingCallback();

                deleteWebsiteDataForNextPlugin();
            }

            ~State()
            {
                ASSERT(m_plugins.isEmpty());
            }

            void deleteWebsiteDataForNextPlugin()
            {
                if (m_plugins.isEmpty()) {
                    m_callbackAggregator->removePendingCallback();

                    delete this;
                    return;
                }

                auto plugin = m_plugins.takeLast();
                PluginProcessManager::singleton().deleteWebsiteDataForHostNames(plugin, m_hostNames, [this] {
                    deleteWebsiteDataForNextPlugin();
                });
            }

            Ref<CallbackAggregator> m_callbackAggregator;
            Vector<PluginModuleInfo> m_plugins;
            Vector<String> m_hostNames;
        };

        State::deleteData(*callbackAggregator, plugins(), WTF::move(hostNames));
    }
#endif

    // There's a chance that we don't have any pending callbacks. If so, we want to dispatch the completion handler right away.
    callbackAggregator->callIfNeeded();
}

void WebsiteDataStore::webPageWasAdded(WebPageProxy& webPageProxy)
{
    if (m_storageManager)
        m_storageManager->createSessionStorageNamespace(webPageProxy.pageID(), std::numeric_limits<unsigned>::max());
}

void WebsiteDataStore::webPageWasRemoved(WebPageProxy& webPageProxy)
{
    if (m_storageManager)
        m_storageManager->destroySessionStorageNamespace(webPageProxy.pageID());
}

void WebsiteDataStore::webProcessWillOpenConnection(WebProcessProxy& webProcessProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->processWillOpenConnection(webProcessProxy, connection);
}

void WebsiteDataStore::webPageWillOpenConnection(WebPageProxy& webPageProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->setAllowedSessionStorageNamespaceConnection(webPageProxy.pageID(), &connection);
}

void WebsiteDataStore::webPageDidCloseConnection(WebPageProxy& webPageProxy, IPC::Connection&)
{
    if (m_storageManager)
        m_storageManager->setAllowedSessionStorageNamespaceConnection(webPageProxy.pageID(), nullptr);
}

void WebsiteDataStore::webProcessDidCloseConnection(WebProcessProxy& webProcessProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->processDidCloseConnection(webProcessProxy, connection);
}

HashSet<RefPtr<WebProcessPool>> WebsiteDataStore::processPools() const
{
    HashSet<RefPtr<WebProcessPool>> processPools;
    for (auto& process : processes())
        processPools.add(&process->processPool());

    if (processPools.isEmpty()) {
        // Check if we're one of the legacy data stores.
        for (auto& processPool : WebProcessPool::allProcessPools()) {
            if (auto dataStore = processPool->websiteDataStore()) {
                if (&dataStore->websiteDataStore() == this) {
                    processPools.add(processPool);
                    break;
                }
            }
        }
    }

    if (processPools.isEmpty()) {
        auto processPool = WebProcessPool::create(API::ProcessPoolConfiguration::create());

        processPools.add(processPool.ptr());
    }

    return processPools;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
Vector<PluginModuleInfo> WebsiteDataStore::plugins() const
{
    Vector<PluginModuleInfo> plugins;

    for (auto processPool : processPools()) {
        for (auto& plugin : processPool->pluginInfoStore().plugins())
            plugins.append(plugin);
    }

    return plugins;
}
#endif

static String computeMediaKeyFile(const String& mediaKeyDirectory)
{
    return WebCore::pathByAppendingComponent(mediaKeyDirectory, "SecureStop.plist");
}

Vector<RefPtr<WebCore::SecurityOrigin>> WebsiteDataStore::mediaKeyOrigins(const String& mediaKeysStorageDirectory)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    Vector<RefPtr<WebCore::SecurityOrigin>> origins;

    for (const auto& originPath : WebCore::listDirectory(mediaKeysStorageDirectory, "*")) {
        auto mediaKeyFile = computeMediaKeyFile(originPath);
        if (!WebCore::fileExists(mediaKeyFile))
            continue;

        auto mediaKeyIdentifier = WebCore::pathGetFileName(originPath);

        if (auto securityOrigin = WebCore::SecurityOrigin::maybeCreateFromDatabaseIdentifier(mediaKeyIdentifier))
            origins.append(WTF::move(securityOrigin));
    }

    return origins;
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, std::chrono::system_clock::time_point modifiedSince)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& mediaKeyDirectory : WebCore::listDirectory(mediaKeysStorageDirectory, "*")) {
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        time_t modificationTime;
        if (!WebCore::getFileModificationTime(mediaKeyFile, modificationTime))
            continue;

        if (std::chrono::system_clock::from_time_t(modificationTime) < modifiedSince)
            continue;

        WebCore::deleteFile(mediaKeyFile);
        WebCore::deleteEmptyDirectory(mediaKeyDirectory);
    }
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, const HashSet<RefPtr<WebCore::SecurityOrigin>>& origins)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& origin : origins) {
        auto mediaKeyDirectory = WebCore::pathByAppendingComponent(mediaKeysStorageDirectory, origin->databaseIdentifier());
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        WebCore::deleteFile(mediaKeyFile);
        WebCore::deleteEmptyDirectory(mediaKeyDirectory);
    }
}

}
