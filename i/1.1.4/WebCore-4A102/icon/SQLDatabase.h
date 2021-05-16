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

#ifndef SQLDatabase_H
#define SQLDatabase_H

#include "config.h"

#include "PlatformString.h"
#include <sqlite3.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class SQLStatement;

class SQLDatabase : public Noncopyable
{
    friend class SQLStatement;
public:
    SQLDatabase();

    bool open(const String& filename);
    bool isOpen() { return m_db; }
    String getPath(){ return m_path; }
    void close();

    bool executeCommand(const String&);
    bool returnsAtLeastOneResult(const String&);
    
    bool tableExists(const String&);
    
    int64_t lastInsertRowID();

    void setBusyTimeout(int ms);
    void setBusyHandler(int(*)(void*, int));
    
    // TODO - add pragma and sqlite_master accessors here
    void setFullsync(bool);
    
    // The SQLite SYNCHRONOUS pragma can be either FULL, NORMAL, or OFF
    // FULL - Any writing calls to the DB block until the data is actually on the disk surface
    // NORMAL - SQLite pauses at some critical moments when writing, but much less than FULL
    // OFF - Calls return immediately after the data has been passed to disk
    enum SynchronousPragma {
        SyncOff = 0, SyncNormal = 1, SyncFull = 2
    };
    void setSynchronous(SynchronousPragma);
    
    int lastError() { return m_db ? sqlite3_errcode(m_db) : SQLITE_ERROR; }
    const char* lastErrorMsg() { return sqlite3_errmsg(m_db); }
    
private:
    String   m_path;
    sqlite3* m_db;
    int m_lastError;
    
}; // class SQLDatabase

class SQLTransaction : public Noncopyable
{
public:
    SQLTransaction(SQLDatabase& db, bool start = false);
    ~SQLTransaction();
    
    void begin();
    void commit();
    void rollback();
    
private:
    SQLDatabase& m_db;
    bool m_began;

};

class SQLStatement : public Noncopyable
{
public:
    SQLStatement(SQLDatabase& db, const String&);
    ~SQLStatement();
    
    int prepare();
    bool isPrepared() { return m_statement; }
    int bindBlob(int index, const void* blob, int size, bool copy = true);
    int bindText(int index, const char* text, bool copy = true);
    int bindInt64(int index, int64_t integer);

    int step();
    int finalize();
    int reset();
    
    int prepareAndStep() { prepare();  return step(); }
    
    // prepares, steps, and finalizes the query.
    // returns true if all 3 steps succeed with step() returning SQLITE_DONE
    // returns false otherwise  
    bool executeCommand();
    
    // prepares, steps, and finalizes.  
    // returns true is step() returns SQLITE_ROW
    // returns false otherwise
    bool returnsAtLeastOneResult();
    
    // Returns -1 on last-step failing.  Otherwise, returns number of rows
    // returned in the last step()
    int columnCount();
    
    String getColumnName(int col);
    String getColumnName16(int col);
    String getColumnText(int col);
    String getColumnText16(int col);
    double getColumnDouble(int col);
    int getColumnInt(int col);
    int64_t getColumnInt64(int col);
    const void* getColumnBlob(int col, int& size);
    Vector<unsigned char> getColumnBlobAsVector(int col);

    bool returnTextResults(int col, Vector<String>& v);
    bool returnTextResults16(int col, Vector<String>& v);
    bool returnIntResults(int col, Vector<int>& v);
    bool returnInt64Results(int col, Vector<int64_t>& v);
    bool returnDoubleResults(int col, Vector<double>& v);

    int lastError() { return m_database.lastError(); }
    const char* lastErrorMsg() { return m_database.lastErrorMsg(); }
    
private:
    SQLDatabase& m_database;
    String      m_query;

    sqlite3_stmt* m_statement;
};

} // namespace WebCore



#endif
