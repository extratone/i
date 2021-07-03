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
#include "SQLiteIDBCursor.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "IDBSerialization.h"
#include "Logging.h"
#include "SQLiteIDBTransaction.h"
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteTransaction.h>
#include <sqlite3.h>

using namespace WebCore;

namespace WebKit {

std::unique_ptr<SQLiteIDBCursor> SQLiteIDBCursor::maybeCreate(SQLiteIDBTransaction* transaction, const IDBIdentifier& cursorIdentifier, int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, const IDBKeyRangeData& keyRange)
{
    auto cursor = std::unique_ptr<SQLiteIDBCursor>(new SQLiteIDBCursor(transaction, cursorIdentifier, objectStoreID, indexID, cursorDirection, cursorType, taskType, keyRange));

    if (!cursor->establishStatement())
        return nullptr;

    if (!cursor->advance(1))
        return nullptr;

    return cursor;
}

SQLiteIDBCursor::SQLiteIDBCursor(SQLiteIDBTransaction* transaction, const IDBIdentifier& cursorIdentifier, int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType, IDBDatabaseBackend::TaskType, const IDBKeyRangeData& keyRange)
    : m_transaction(transaction)
    , m_cursorIdentifier(cursorIdentifier)
    , m_objectStoreID(objectStoreID)
    , m_indexID(indexID)
    , m_cursorDirection(cursorDirection)
    , m_keyRange(keyRange)
    , m_currentRecordID(-1)
    , m_statementNeedsReset(false)
    , m_boundID(0)
    , m_completed(false)
    , m_errored(false)
{
    ASSERT(m_objectStoreID);
}

static String buildIndexStatement(const IDBKeyRangeData& keyRange, IndexedDB::CursorDirection cursorDirection)
{
    StringBuilder builder;

    builder.appendLiteral("SELECT rowid, key, value FROM IndexRecords WHERE indexID = ? AND key ");
    if (!keyRange.lowerKey.isNull && !keyRange.lowerOpen)
        builder.appendLiteral(">=");
    else
        builder.append('>');

    builder.appendLiteral(" CAST(? AS TEXT) AND key ");
    if (!keyRange.upperKey.isNull && !keyRange.upperOpen)
        builder.appendLiteral("<=");
    else
        builder.append('<');

    builder.appendLiteral(" CAST(? AS TEXT) ORDER BY key");
    if (cursorDirection == IndexedDB::CursorDirection::Prev || cursorDirection == IndexedDB::CursorDirection::PrevNoDuplicate)
        builder.appendLiteral(" DESC");

    builder.appendLiteral(", value");
    if (cursorDirection == IndexedDB::CursorDirection::Prev)
        builder.appendLiteral(" DESC");

    builder.append(';');

    return builder.toString();
}

static String buildObjectStoreStatement(const IDBKeyRangeData& keyRange, IndexedDB::CursorDirection cursorDirection)
{
    StringBuilder builder;

    builder.appendLiteral("SELECT rowid, key, value FROM Records WHERE objectStoreID = ? AND key ");

    if (!keyRange.lowerKey.isNull && !keyRange.lowerOpen)
        builder.appendLiteral(">=");
    else
        builder.append('>');

    builder.appendLiteral(" CAST(? AS TEXT) AND key ");

    if (!keyRange.upperKey.isNull && !keyRange.upperOpen)
        builder.appendLiteral("<=");
    else
        builder.append('<');

    builder.appendLiteral(" CAST(? AS TEXT) ORDER BY key");

    if (cursorDirection == IndexedDB::CursorDirection::Prev || cursorDirection == IndexedDB::CursorDirection::PrevNoDuplicate)
        builder.appendLiteral(" DESC");

    builder.append(';');

    return builder.toString();
}

bool SQLiteIDBCursor::establishStatement()
{
    ASSERT(!m_statement);
    String sql;

    if (m_indexID != IDBIndexMetadata::InvalidId) {
        sql = buildIndexStatement(m_keyRange, m_cursorDirection);
        m_boundID = m_indexID;
    } else {
        sql = buildObjectStoreStatement(m_keyRange, m_cursorDirection);
        m_boundID = m_objectStoreID;
    }

    m_currentLowerKey = m_keyRange.lowerKey.isNull ? IDBKeyData::minimum() : m_keyRange.lowerKey;
    m_currentUpperKey = m_keyRange.upperKey.isNull ? IDBKeyData::maximum() : m_keyRange.upperKey;

    return createSQLiteStatement(sql);
}

bool SQLiteIDBCursor::createSQLiteStatement(const String& sql)
{
    LOG(IDB, "Creating cursor with SQL query: \"%s\"", sql.utf8().data());

    ASSERT(!m_currentLowerKey.isNull);
    ASSERT(!m_currentUpperKey.isNull);
    ASSERT(m_transaction->sqliteTransaction());

    m_statement = std::make_unique<SQLiteStatement>(m_transaction->sqliteTransaction()->database(), sql);

    if (m_statement->prepare() != SQLITE_OK) {
        LOG_ERROR("Could not create cursor statement (prepare/id) - '%s'", m_transaction->sqliteTransaction()->database().lastErrorMsg());
        return false;
    }

    return bindArguments();
}

void SQLiteIDBCursor::objectStoreRecordsChanged()
{
    // If ObjectStore or Index contents changed, we need to reset the statement and bind new parameters to it.
    // This is to pick up any changes that might exist.

    m_statementNeedsReset = true;
}

void SQLiteIDBCursor::resetAndRebindStatement()
{
    ASSERT(!m_currentLowerKey.isNull);
    ASSERT(!m_currentUpperKey.isNull);
    ASSERT(m_transaction->sqliteTransaction());
    ASSERT(m_statement);
    ASSERT(m_statementNeedsReset);

    m_statementNeedsReset = false;

    // If this cursor never fetched any records, we don't need to reset the statement.
    if (m_currentKey.isNull)
        return;

    // Otherwise update the lower key or upper key used for the cursor range.
    // This is so the cursor can pick up where we left off.
    if (m_cursorDirection == IndexedDB::CursorDirection::Next || m_cursorDirection == IndexedDB::CursorDirection::NextNoDuplicate)
        m_currentLowerKey = m_currentKey;
    else
        m_currentUpperKey = m_currentKey;

    if (m_statement->reset() != SQLITE_OK) {
        LOG_ERROR("Could not reset cursor statement to respond to object store changes");
        return;
    }

    bindArguments();
}

bool SQLiteIDBCursor::bindArguments()
{
    if (m_statement->bindInt64(1, m_boundID) != SQLITE_OK) {
        LOG_ERROR("Could not bind id argument (bound ID)");
        return false;
    }

    RefPtr<SharedBuffer> buffer = serializeIDBKeyData(m_currentLowerKey);
    if (m_statement->bindBlob(2, buffer->data(), buffer->size()) != SQLITE_OK) {
        LOG_ERROR("Could not create cursor statement (lower key)");
        return false;
    }

    buffer = serializeIDBKeyData(m_currentUpperKey);
    if (m_statement->bindBlob(3, buffer->data(), buffer->size()) != SQLITE_OK) {
        LOG_ERROR("Could not create cursor statement (upper key)");
        return false;
    }

    return true;
}

bool SQLiteIDBCursor::advance(uint64_t count)
{
    bool isUnique = m_cursorDirection == IndexedDB::CursorDirection::NextNoDuplicate || m_cursorDirection == IndexedDB::CursorDirection::PrevNoDuplicate;

    for (uint64_t i = 0; i < count; ++i) {
        if (!isUnique) {
            if (!advanceOnce())
                return false;
        } else {
            if (!advanceUnique())
                return false;
        }
    }

    return true;
}

bool SQLiteIDBCursor::advanceUnique()
{
    IDBKeyData currentKey = m_currentKey;

    while (!m_completed) {
        if (!advanceOnce())
            return false;

        // If the new current key is different from the old current key, we're done.
        if (currentKey.compare(m_currentKey))
            return true;
    }

    return false;
}

bool SQLiteIDBCursor::advanceOnce()
{
    if (m_statementNeedsReset)
        resetAndRebindStatement();

    AdvanceResult result;
    do {
        result = internalAdvanceOnce();
    } while (result == AdvanceResult::ShouldAdvanceAgain);

    return result == AdvanceResult::Success;
}

SQLiteIDBCursor::AdvanceResult SQLiteIDBCursor::internalAdvanceOnce()
{
    ASSERT(m_transaction->sqliteTransaction());
    ASSERT(m_statement);

    if (m_completed) {
        LOG_ERROR("Attempt to advance a completed cursor");
        return AdvanceResult::Failure;
    }

    int result = m_statement->step();
    if (result == SQLITE_DONE) {
        m_completed = true;

        // When a cursor reaches its end, that is indicated by having undefined keys/values
        m_currentKey = IDBKeyData();
        m_currentPrimaryKey = IDBKeyData();
        m_currentValueBuffer.clear();

        return AdvanceResult::Success;
    }

    if (result != SQLITE_ROW) {
        LOG_ERROR("Error advancing cursor - (%i) %s", result, m_transaction->sqliteTransaction()->database().lastErrorMsg());
        m_completed = true;
        m_errored = true;
        return AdvanceResult::Failure;
    }

    int64_t recordID = m_statement->getColumnInt64(0);

    // If the recordID of the record just fetched is the same as the current record ID
    // then this statement must have been re-prepared in response to an object store change.
    // We don't want to re-use the current record so we'll move on to the next one.
    if (recordID == m_currentRecordID)
        return AdvanceResult::ShouldAdvanceAgain;

    m_currentRecordID = recordID;

    Vector<uint8_t> keyData;
    m_statement->getColumnBlobAsVector(1, keyData);

    if (!deserializeIDBKeyData(keyData.data(), keyData.size(), m_currentKey)) {
        LOG_ERROR("Unable to deserialize key data from database while advancing cursor");
        m_completed = true;
        m_errored = true;
        return AdvanceResult::Failure;
    }

    m_statement->getColumnBlobAsVector(2, keyData);
    m_currentValueBuffer = keyData;

    // The primaryKey of an ObjectStore cursor is the same as its key.
    if (m_indexID == IDBIndexMetadata::InvalidId)
        m_currentPrimaryKey = m_currentKey;
    else {
        if (!deserializeIDBKeyData(keyData.data(), keyData.size(), m_currentPrimaryKey)) {
            LOG_ERROR("Unable to deserialize value data from database while advancing index cursor");
            m_completed = true;
            m_errored = true;
            return AdvanceResult::Failure;
        }

        SQLiteStatement objectStoreStatement(m_statement->database(), "SELECT value FROM Records WHERE key = CAST(? AS TEXT) and objectStoreID = ?;");

        if (objectStoreStatement.prepare() != SQLITE_OK
            || objectStoreStatement.bindBlob(1, m_currentValueBuffer.data(), m_currentValueBuffer.size()) != SQLITE_OK
            || objectStoreStatement.bindInt64(2, m_objectStoreID) != SQLITE_OK) {
            LOG_ERROR("Could not create index cursor statement into object store records (%i) '%s'", m_statement->database().lastError(), m_statement->database().lastErrorMsg());
            m_completed = true;
            m_errored = true;
            return AdvanceResult::Failure;
        }

        int result = objectStoreStatement.step();

        if (result == SQLITE_ROW)
            objectStoreStatement.getColumnBlobAsVector(0, m_currentValueBuffer);
        else if (result == SQLITE_DONE) {
            // This indicates that the record we're trying to retrieve has been removed from the object store.
            // Skip over it.
            return AdvanceResult::ShouldAdvanceAgain;
        } else {
            LOG_ERROR("Could not step index cursor statement into object store records (%i) '%s'", m_statement->database().lastError(), m_statement->database().lastErrorMsg());
            m_completed = true;
            m_errored = true;
            return AdvanceResult::Failure;

        }
    }

    return AdvanceResult::Success;
}

bool SQLiteIDBCursor::iterate(const WebCore::IDBKeyData& targetKey)
{
    ASSERT(m_transaction->sqliteTransaction());
    ASSERT(m_statement);

    bool result = advance(1);

    // Iterating with no key is equivalent to advancing 1 step.
    if (targetKey.isNull || !result)
        return result;

    while (!m_completed) {
        if (!result)
            return false;

        // Search for the next key >= the target if the cursor is a Next cursor, or the next key <= if the cursor is a Previous cursor.
        if (m_cursorDirection == IndexedDB::CursorDirection::Next || m_cursorDirection == IndexedDB::CursorDirection::NextNoDuplicate) {
            if (m_currentKey.compare(targetKey) >= 0)
                break;
        } else if (m_currentKey.compare(targetKey) <= 0)
            break;

        result = advance(1);
    }

    return result;
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
