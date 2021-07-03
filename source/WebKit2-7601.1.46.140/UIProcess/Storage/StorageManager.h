/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef StorageManager_h
#define StorageManager_h

#include "Connection.h"
#include "LocalStorageDatabaseTracker.h"
#include <WebCore/SecurityOriginHash.h>
#include <chrono>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

struct SecurityOriginData;
class LocalStorageDatabaseTracker;
class WebProcessProxy;

class StorageManager : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<StorageManager> create(const String& localStorageDirectory);
    ~StorageManager();

    void createSessionStorageNamespace(uint64_t storageNamespaceID, unsigned quotaInBytes);
    void destroySessionStorageNamespace(uint64_t storageNamespaceID);
    void setAllowedSessionStorageNamespaceConnection(uint64_t storageNamespaceID, IPC::Connection* allowedConnection);
    void cloneSessionStorageNamespace(uint64_t storageNamespaceID, uint64_t newStorageNamespaceID);

    void processWillOpenConnection(WebProcessProxy&, IPC::Connection&);
    void processDidCloseConnection(WebProcessProxy&, IPC::Connection&);
    void applicationWillTerminate();

    void getSessionStorageOrigins(std::function<void (HashSet<RefPtr<WebCore::SecurityOrigin>>&&)> completionHandler);
    void deleteSessionStorageOrigins(std::function<void ()> completionHandler);
    void deleteSessionStorageEntriesForOrigins(const Vector<RefPtr<WebCore::SecurityOrigin>>&, std::function<void ()> completionHandler);

    void getLocalStorageOrigins(std::function<void (HashSet<RefPtr<WebCore::SecurityOrigin>>&&)> completionHandler);
    void deleteLocalStorageEntriesForOrigin(const WebCore::SecurityOrigin&);

    void deleteLocalStorageOriginsModifiedSince(std::chrono::system_clock::time_point, std::function<void ()> completionHandler);
    void deleteLocalStorageEntriesForOrigins(const Vector<RefPtr<WebCore::SecurityOrigin>>&, std::function<void ()> completionHandler);

    void getLocalStorageOriginDetails(std::function<void (Vector<LocalStorageDatabaseTracker::OriginDetails>)> completionHandler);

private:
    explicit StorageManager(const String& localStorageDirectory);

    // IPC::Connection::WorkQueueMessageReceiver.
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>& replyEncoder) override;

    // Message handlers.
    void createLocalStorageMap(IPC::Connection&, uint64_t storageMapID, uint64_t storageNamespaceID, const SecurityOriginData&);
    void createTransientLocalStorageMap(IPC::Connection&, uint64_t storageMapID, uint64_t storageNamespaceID, const SecurityOriginData& topLevelOriginData, const SecurityOriginData&);
    void createSessionStorageMap(IPC::Connection&, uint64_t storageMapID, uint64_t storageNamespaceID, const SecurityOriginData&);
    void destroyStorageMap(IPC::Connection&, uint64_t storageMapID);

    void getValues(IPC::Connection&, uint64_t storageMapID, uint64_t storageMapSeed, HashMap<String, String>& values);
    void setItem(IPC::Connection&, uint64_t storageAreaID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& key, const String& value, const String& urlString);
    void removeItem(IPC::Connection&, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& key, const String& urlString);
    void clear(IPC::Connection&, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& urlString);

    class StorageArea;
    StorageArea* findStorageArea(IPC::Connection&, uint64_t) const;

    class LocalStorageNamespace;
    LocalStorageNamespace* getOrCreateLocalStorageNamespace(uint64_t storageNamespaceID);

    class TransientLocalStorageNamespace;
    TransientLocalStorageNamespace* getOrCreateTransientLocalStorageNamespace(uint64_t storageNamespaceID, WebCore::SecurityOrigin& topLevelOrigin);

    RefPtr<WorkQueue> m_queue;

    RefPtr<LocalStorageDatabaseTracker> m_localStorageDatabaseTracker;
    HashMap<uint64_t, RefPtr<LocalStorageNamespace>> m_localStorageNamespaces;

    HashMap<std::pair<uint64_t, RefPtr<WebCore::SecurityOrigin>>, RefPtr<TransientLocalStorageNamespace>> m_transientLocalStorageNamespaces;

    class SessionStorageNamespace;
    HashMap<uint64_t, RefPtr<SessionStorageNamespace>> m_sessionStorageNamespaces;

    HashMap<std::pair<RefPtr<IPC::Connection>, uint64_t>, RefPtr<StorageArea>> m_storageAreasByConnection;
};

} // namespace WebKit

#endif // StorageManager_h
