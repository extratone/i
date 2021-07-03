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

#ifndef UniqueIDBDatabaseBackingStoreSQLite_h
#define UniqueIDBDatabaseBackingStoreSQLite_h

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "UniqueIDBDatabase.h" 
#include "UniqueIDBDatabaseBackingStore.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class JSGlobalObject;
class VM;
}

namespace WebCore {
class SQLiteDatabase;
struct IDBDatabaseMetadata;
}

namespace WebKit {

class SQLiteIDBCursor;
class SQLiteIDBTransaction;

class UniqueIDBDatabaseBackingStoreSQLite final : public UniqueIDBDatabaseBackingStore {
public:
    static Ref<UniqueIDBDatabaseBackingStore> create(const UniqueIDBDatabaseIdentifier& identifier, const String& databaseDirectory)
    {
        return adoptRef(*new UniqueIDBDatabaseBackingStoreSQLite(identifier, databaseDirectory));
    }

    virtual ~UniqueIDBDatabaseBackingStoreSQLite();

    virtual std::unique_ptr<WebCore::IDBDatabaseMetadata> getOrEstablishMetadata() override;

    virtual bool establishTransaction(const IDBIdentifier& transactionIdentifier, const Vector<int64_t>& objectStoreIDs, WebCore::IndexedDB::TransactionMode) override;
    virtual bool beginTransaction(const IDBIdentifier&) override;
    virtual bool commitTransaction(const IDBIdentifier&) override;
    virtual bool resetTransaction(const IDBIdentifier&) override;
    virtual bool rollbackTransaction(const IDBIdentifier&) override;

    virtual bool changeDatabaseVersion(const IDBIdentifier& transactionIdentifier, uint64_t newVersion) override;
    virtual bool createObjectStore(const IDBIdentifier& transactionIdentifier, const WebCore::IDBObjectStoreMetadata&) override;
    virtual bool deleteObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID) override;
    virtual bool clearObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID) override;
    virtual bool createIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBIndexMetadata&) override;
    virtual bool deleteIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID) override;

    virtual bool generateKeyNumber(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t& generatedKey) override;
    virtual bool updateKeyGeneratorNumber(const IDBIdentifier& transactionIdentifier, int64_t objectStoreId, int64_t keyNumber, bool checkCurrent) override;

    virtual bool keyExistsInObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyData&, bool& keyExists) override;
    virtual bool putRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyData&, const uint8_t* valueBuffer, size_t valueSize) override;
    virtual bool putIndexRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyData& keyValue, const WebCore::IDBKeyData& indexKey) override;
    virtual bool getIndexRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyRangeData&, WebCore::IndexedDB::CursorType, WebCore::IDBGetResult&) override;
    virtual bool deleteRange(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyRangeData&) override;
    virtual bool deleteRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyData&) override;

    virtual bool getKeyRecordFromObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKey&, RefPtr<WebCore::SharedBuffer>& result) override;
    virtual bool getKeyRangeRecordFromObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyRange&, RefPtr<WebCore::SharedBuffer>& result, RefPtr<WebCore::IDBKey>& resultKey) override;
    virtual bool count(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyRangeData&, int64_t& count) override;

    virtual bool openCursor(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, WebCore::IndexedDB::CursorDirection, WebCore::IndexedDB::CursorType, WebCore::IDBDatabaseBackend::TaskType, const WebCore::IDBKeyRangeData&, int64_t& cursorID, WebCore::IDBKeyData&, WebCore::IDBKeyData&, Vector<uint8_t>&) override;
    virtual bool advanceCursor(const IDBIdentifier& cursorIdentifier, uint64_t count, WebCore::IDBKeyData&, WebCore::IDBKeyData&, Vector<uint8_t>&) override;
    virtual bool iterateCursor(const IDBIdentifier& cursorIdentifier, const WebCore::IDBKeyData&, WebCore::IDBKeyData&, WebCore::IDBKeyData&, Vector<uint8_t>&) override;
    virtual void notifyCursorsOfChanges(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID) override;

    void unregisterCursor(SQLiteIDBCursor*);

private:
    UniqueIDBDatabaseBackingStoreSQLite(const UniqueIDBDatabaseIdentifier&, const String& databaseDirectory);

    std::unique_ptr<WebCore::SQLiteDatabase> openSQLiteDatabaseAtPath(const String&);
    std::unique_ptr<WebCore::IDBDatabaseMetadata> extractExistingMetadata();
    std::unique_ptr<WebCore::IDBDatabaseMetadata> createAndPopulateInitialMetadata();

    bool ensureValidRecordsTable();

    bool deleteRecord(SQLiteIDBTransaction&, int64_t objectStoreID, const WebCore::IDBKeyData&);
    bool uncheckedPutIndexRecord(int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyData& keyValue, const WebCore::IDBKeyData& indexKey);

    int idbKeyCollate(int aLength, const void* a, int bLength, const void* b);

    UniqueIDBDatabaseIdentifier m_identifier;
    String m_absoluteDatabaseDirectory;

    std::unique_ptr<WebCore::SQLiteDatabase> m_sqliteDB;

    HashMap<IDBIdentifier, std::unique_ptr<SQLiteIDBTransaction>> m_transactions;
    HashMap<IDBIdentifier, SQLiteIDBCursor*> m_cursors;

    RefPtr<JSC::VM> m_vm;
    JSC::Strong<JSC::JSGlobalObject> m_globalObject;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // UniqueIDBDatabaseBackingStoreSQLite_h
