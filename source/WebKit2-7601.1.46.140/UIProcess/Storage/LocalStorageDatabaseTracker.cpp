/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
#include "LocalStorageDatabaseTracker.h"

#include <WebCore/FileSystem.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/TextEncoding.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<LocalStorageDatabaseTracker> LocalStorageDatabaseTracker::create(PassRefPtr<WorkQueue> queue, const String& localStorageDirectory)
{
    return adoptRef(new LocalStorageDatabaseTracker(queue, localStorageDirectory));
}

LocalStorageDatabaseTracker::LocalStorageDatabaseTracker(PassRefPtr<WorkQueue> queue, const String& localStorageDirectory)
    : m_queue(queue)
    , m_localStorageDirectory(localStorageDirectory.isolatedCopy())
{
    ASSERT(!m_localStorageDirectory.isEmpty());

    // Make sure the encoding is initialized before we start dispatching things to the queue.
    UTF8Encoding();

    RefPtr<LocalStorageDatabaseTracker> localStorageDatabaseTracker(this);
    m_queue->dispatch([localStorageDatabaseTracker] {
        localStorageDatabaseTracker->importOriginIdentifiers();
    });
}

LocalStorageDatabaseTracker::~LocalStorageDatabaseTracker()
{
}

String LocalStorageDatabaseTracker::databasePath(SecurityOrigin* securityOrigin) const
{
    return databasePath(securityOrigin->databaseIdentifier() + ".localstorage");
}

void LocalStorageDatabaseTracker::didOpenDatabaseWithOrigin(SecurityOrigin* securityOrigin)
{
    addDatabaseWithOriginIdentifier(securityOrigin->databaseIdentifier(), databasePath(securityOrigin));
}

void LocalStorageDatabaseTracker::deleteDatabaseWithOrigin(SecurityOrigin* securityOrigin)
{
    removeDatabaseWithOriginIdentifier(securityOrigin->databaseIdentifier());
}

void LocalStorageDatabaseTracker::deleteAllDatabases()
{
    m_origins.clear();

    openTrackerDatabase(SkipIfNonExistent);
    if (!m_database.isOpen())
        return;

    SQLiteStatement statement(m_database, "SELECT origin, path FROM Origins");
    if (statement.prepare() != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement.");
        return;
    }

    int result;
    while ((result = statement.step()) == SQLITE_ROW) {
        deleteFile(statement.getColumnText(1));

        // FIXME: Call out to the client.
    }

    if (result != SQLITE_DONE)
        LOG_ERROR("Failed to read in all origins from the database.");

    if (m_database.isOpen())
        m_database.close();

    if (!deleteFile(trackerDatabasePath())) {
        // In the case where it is not possible to delete the database file (e.g some other program
        // like a virus scanner is accessing it), make sure to remove all entries.
        openTrackerDatabase(SkipIfNonExistent);
        if (!m_database.isOpen())
            return;

        SQLiteStatement deleteStatement(m_database, "DELETE FROM Origins");
        if (deleteStatement.prepare() != SQLITE_OK) {
            LOG_ERROR("Unable to prepare deletion of all origins");
            return;
        }
        if (!deleteStatement.executeCommand()) {
            LOG_ERROR("Unable to execute deletion of all origins");
            return;
        }
    }

    deleteEmptyDirectory(m_localStorageDirectory);
}

static Optional<time_t> fileCreationTime(const String& filePath)
{
    time_t time;
    return getFileCreationTime(filePath, time) ? time : Optional<time_t>(Nullopt);
}

static Optional<time_t> fileModificationTime(const String& filePath)
{
    time_t time;
    if (!getFileModificationTime(filePath, time))
        return Nullopt;

    return time;
}

Vector<Ref<SecurityOrigin>> LocalStorageDatabaseTracker::deleteDatabasesModifiedSince(std::chrono::system_clock::time_point time)
{
    Vector<String> originIdentifiersToDelete;

    for (const String& origin : m_origins) {
        String filePath = pathForDatabaseWithOriginIdentifier(origin);

        auto modificationTime = fileModificationTime(filePath);
        if (!modificationTime)
            continue;

        if (modificationTime.value() >= std::chrono::system_clock::to_time_t(time))
            originIdentifiersToDelete.append(origin);
    }

    Vector<Ref<SecurityOrigin>> deletedDatabaseOrigins;
    deletedDatabaseOrigins.reserveInitialCapacity(originIdentifiersToDelete.size());

    for (const auto& originIdentifier : originIdentifiersToDelete) {
        removeDatabaseWithOriginIdentifier(originIdentifier);

        deletedDatabaseOrigins.uncheckedAppend(SecurityOrigin::createFromDatabaseIdentifier(originIdentifier));
    }

    return deletedDatabaseOrigins;
}

Vector<Ref<WebCore::SecurityOrigin>> LocalStorageDatabaseTracker::origins() const
{
    Vector<Ref<SecurityOrigin>> origins;
    origins.reserveInitialCapacity(m_origins.size());

    for (const String& origin : m_origins)
        origins.uncheckedAppend(SecurityOrigin::createFromDatabaseIdentifier(origin));

    return origins;
}

Vector<LocalStorageDatabaseTracker::OriginDetails> LocalStorageDatabaseTracker::originDetails()
{
    Vector<OriginDetails> result;
    result.reserveInitialCapacity(m_origins.size());

    for (const String& origin : m_origins) {
        String filePath = pathForDatabaseWithOriginIdentifier(origin);

        OriginDetails details;
        details.originIdentifier = origin.isolatedCopy();
        details.creationTime = fileCreationTime(filePath);
        details.modificationTime = fileModificationTime(filePath);
        result.uncheckedAppend(details);
    }

    return result;
}

String LocalStorageDatabaseTracker::databasePath(const String& filename) const
{
    if (!makeAllDirectories(m_localStorageDirectory)) {
        LOG_ERROR("Unable to create LocalStorage database path %s", m_localStorageDirectory.utf8().data());
        return String();
    }

    return pathByAppendingComponent(m_localStorageDirectory, filename);
}

String LocalStorageDatabaseTracker::trackerDatabasePath() const
{
    return databasePath("StorageTracker.db");
}

void LocalStorageDatabaseTracker::openTrackerDatabase(DatabaseOpeningStrategy openingStrategy)
{
    if (m_database.isOpen())
        return;

    String databasePath = trackerDatabasePath();

    if (!fileExists(databasePath) && openingStrategy == SkipIfNonExistent)
        return;

    if (!m_database.open(databasePath)) {
        LOG_ERROR("Failed to open databasePath %s.", databasePath.ascii().data());
        return;
    }

    // Since a WorkQueue isn't bound to a specific thread, we have to disable threading checks
    // even though we never access the database from different threads simultaneously.
    m_database.disableThreadingChecks();

    if (m_database.tableExists("Origins"))
        return;

    if (!m_database.executeCommand("CREATE TABLE Origins (origin TEXT UNIQUE ON CONFLICT REPLACE, path TEXT);"))
        LOG_ERROR("Failed to create Origins table.");
}

void LocalStorageDatabaseTracker::importOriginIdentifiers()
{
    openTrackerDatabase(SkipIfNonExistent);

    if (m_database.isOpen()) {
        SQLiteStatement statement(m_database, "SELECT origin FROM Origins");
        if (statement.prepare() != SQLITE_OK) {
            LOG_ERROR("Failed to prepare statement.");
            return;
        }

        int result;

        while ((result = statement.step()) == SQLITE_ROW)
            m_origins.add(statement.getColumnText(0));

        if (result != SQLITE_DONE) {
            LOG_ERROR("Failed to read in all origins from the database.");
            return;
        }
    }

    updateTrackerDatabaseFromLocalStorageDatabaseFiles();
}

void LocalStorageDatabaseTracker::updateTrackerDatabaseFromLocalStorageDatabaseFiles()
{
    Vector<String> paths = listDirectory(m_localStorageDirectory, "*.localstorage");

    HashSet<String> origins(m_origins);
    HashSet<String> originsFromLocalStorageDatabaseFiles;

    for (size_t i = 0; i < paths.size(); ++i) {
        const String& path = paths[i];

        if (!path.endsWith(".localstorage"))
            continue;

        String filename = pathGetFileName(path);

        String originIdentifier = filename.substring(0, filename.length() - strlen(".localstorage"));

        if (!m_origins.contains(originIdentifier))
            addDatabaseWithOriginIdentifier(originIdentifier, path);

        originsFromLocalStorageDatabaseFiles.add(originIdentifier);
    }

    for (auto it = origins.begin(), end = origins.end(); it != end; ++it) {
        const String& originIdentifier = *it;
        if (origins.contains(originIdentifier))
            continue;

        removeDatabaseWithOriginIdentifier(originIdentifier);
    }
}

void LocalStorageDatabaseTracker::addDatabaseWithOriginIdentifier(const String& originIdentifier, const String& databasePath)
{
    openTrackerDatabase(CreateIfNonExistent);
    if (!m_database.isOpen())
        return;

    SQLiteStatement statement(m_database, "INSERT INTO Origins VALUES (?, ?)");
    if (statement.prepare() != SQLITE_OK) {
        LOG_ERROR("Unable to establish origin '%s' in the tracker", originIdentifier.utf8().data());
        return;
    }

    statement.bindText(1, originIdentifier);
    statement.bindText(2, databasePath);

    if (statement.step() != SQLITE_DONE)
        LOG_ERROR("Unable to establish origin '%s' in the tracker", originIdentifier.utf8().data());

    m_origins.add(originIdentifier);

    // FIXME: Tell clients that the origin was added.
}

void LocalStorageDatabaseTracker::removeDatabaseWithOriginIdentifier(const String& originIdentifier)
{
    openTrackerDatabase(SkipIfNonExistent);
    if (!m_database.isOpen())
        return;

    String path = pathForDatabaseWithOriginIdentifier(originIdentifier);
    if (path.isEmpty())
        return;

    SQLiteStatement deleteStatement(m_database, "DELETE FROM Origins where origin=?");
    if (deleteStatement.prepare() != SQLITE_OK) {
        LOG_ERROR("Unable to prepare deletion of origin '%s'", originIdentifier.ascii().data());
        return;
    }
    deleteStatement.bindText(1, originIdentifier);
    if (!deleteStatement.executeCommand()) {
        LOG_ERROR("Unable to execute deletion of origin '%s'", originIdentifier.ascii().data());
        return;
    }

    deleteFile(path);

    m_origins.remove(originIdentifier);
    if (m_origins.isEmpty()) {
        // There are no origins left; delete the tracker database.
        m_database.close();
        deleteFile(trackerDatabasePath());
        deleteEmptyDirectory(m_localStorageDirectory);
    }

    // FIXME: Tell clients that the origin was removed.
}

String LocalStorageDatabaseTracker::pathForDatabaseWithOriginIdentifier(const String& originIdentifier)
{
    if (!m_database.isOpen())
        return String();

    SQLiteStatement pathStatement(m_database, "SELECT path FROM Origins WHERE origin=?");
    if (pathStatement.prepare() != SQLITE_OK) {
        LOG_ERROR("Unable to prepare selection of path for origin '%s'", originIdentifier.utf8().data());
        return String();
    }

    pathStatement.bindText(1, originIdentifier);

    int result = pathStatement.step();
    if (result != SQLITE_ROW)
        return String();

    return pathStatement.getColumnText(0);
}

} // namespace WebKit
