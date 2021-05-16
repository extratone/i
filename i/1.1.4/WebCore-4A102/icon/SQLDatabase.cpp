/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "SQLDatabase.h"
#include "Logging.h"

using namespace WebCore;

SQLDatabase::SQLDatabase()
    : m_db(0)
{

}

bool SQLDatabase::open(const String& filename)
{
    close();
    
    //SQLite expects a null terminator on its UTF16 strings
    m_path = filename;
    m_path.append(UChar(0));
    
    m_lastError = sqlite3_open16(m_path.characters(), &m_db);
    if (m_lastError != SQLITE_OK) {
        LOG_ERROR("SQLite database failed to load from %s\nCause - %s", filename.ascii().data(),
            sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = 0;
    }
    if (!SQLStatement(*this, "PRAGMA temp_store = MEMORY;").executeCommand())
        LOG_ERROR("SQLite database could not set temp_store to memory");

    return isOpen();
}

void SQLDatabase::close()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_path.truncate(0);
        m_db = 0;
    }
}

void SQLDatabase::setFullsync(bool fsync) 
{
    if (fsync) 
        executeCommand("PRAGMA fullfsync = 1;");
    else
        executeCommand("PRAGMA fullfsync = 0;");
}

void SQLDatabase::setSynchronous(SynchronousPragma sync)
{
    executeCommand(String::sprintf("PRAGMA synchronous = %i", sync));
}

void SQLDatabase::setBusyTimeout(int ms)
{
    if (m_db)
        sqlite3_busy_timeout(m_db, ms);
    else
        LOG(IconDatabase, "BusyTimeout set on non-open database");
}

void SQLDatabase::setBusyHandler(int(*handler)(void*, int))
{
    if (m_db)
        sqlite3_busy_handler(m_db, handler, NULL);
    else
        LOG(IconDatabase, "Busy handler set on non-open database");
}

bool SQLDatabase::executeCommand(const String& sql)
{
    return SQLStatement(*this, sql).executeCommand();
}

bool SQLDatabase::returnsAtLeastOneResult(const String& sql)
{
    return SQLStatement(*this, sql).returnsAtLeastOneResult();
}

bool SQLDatabase::tableExists(const String& tablename)
{
    if (!isOpen())
        return false;
        
    String statement = "SELECT name FROM sqlite_master WHERE type = 'table' AND name = '" + tablename + "';";
    
    SQLStatement sql(*this, statement);
    sql.prepare();
    return sql.step() == SQLITE_ROW;
}

int64_t SQLDatabase::lastInsertRowID()
{
    if (!m_db)
        return 0;
    return sqlite3_last_insert_rowid(m_db);
}



