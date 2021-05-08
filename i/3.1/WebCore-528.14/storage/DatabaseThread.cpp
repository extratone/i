/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DatabaseThread.h"

#include "AutodrainedPool.h"
#include "Database.h"
#include "DatabaseTask.h"
#include "Logging.h"

namespace WebCore {

DatabaseThread::DatabaseThread()
    : m_threadID(0)
{
    m_selfRef = this;
}

DatabaseThread::~DatabaseThread()
{
    // FIXME: Any cleanup required here?  Since the thread deletes itself after running its detached course, I don't think so.  Lets be sure.
}

bool DatabaseThread::start()
{
    MutexLocker lock(m_threadCreationMutex);

    if (m_threadID)
        return true;

    m_threadID = createThread(DatabaseThread::databaseThreadStart, this, "WebCore::Database");

    return m_threadID;
}

void DatabaseThread::requestTermination()
{
    LOG(StorageAPI, "DatabaseThread %p was asked to terminate\n", this);
    m_queue.kill();
}

bool DatabaseThread::terminationRequested() const
{
    return m_queue.killed();
}

void* DatabaseThread::databaseThreadStart(void* vDatabaseThread)
{
    DatabaseThread* dbThread = static_cast<DatabaseThread*>(vDatabaseThread);
    return dbThread->databaseThread();
}

void* DatabaseThread::databaseThread()
{
    {
        // Wait for DatabaseThread::start() to complete.
        MutexLocker lock(m_threadCreationMutex);
        LOG(StorageAPI, "Started DatabaseThread %p", this);
    }

    AutodrainedPool pool;
    while (true) {
        RefPtr<DatabaseTask> task;
        if (!m_queue.waitForMessage(task))
            break;

        task->performTask();

        pool.cycle();
    }

    LOG(StorageAPI, "About to detach thread %i and clear the ref to DatabaseThread %p, which currently has %i ref(s)", m_threadID, this, refCount());

    // Detach the thread so its resources are no longer of any concern to anyone else
    detachThread(m_threadID);

    // Clear the self refptr, possibly resulting in deletion
    m_selfRef = 0;

    return 0;
}

void DatabaseThread::scheduleTask(PassRefPtr<DatabaseTask> task)
{
    m_queue.append(task);
}

void DatabaseThread::scheduleImmediateTask(PassRefPtr<DatabaseTask> task)
{
    m_queue.prepend(task);
}

void DatabaseThread::unscheduleDatabaseTasks(Database* database)
{
    // Note that the thread loop is running, so some tasks for the database
    // may still be executed. This is unavoidable.

    Deque<RefPtr<DatabaseTask> > filteredReverseQueue;
    RefPtr<DatabaseTask> task;
    while (m_queue.tryGetMessage(task)) {
        if (task->database() != database)
            filteredReverseQueue.append(task);
    }

    while (!filteredReverseQueue.isEmpty()) {
        m_queue.append(filteredReverseQueue.first());
        filteredReverseQueue.removeFirst();
    }
}

} // namespace WebCore
