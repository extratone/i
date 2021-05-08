// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
 * Copyright (C) 2005 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA 
 *
 */

#include "config.h"
#include "JSLock.h"

#include "collector.h"

namespace KJS {

#if USE(MULTIPLE_THREADS)

static pthread_once_t interpreterLockOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t interpreterLock;
static int interpreterLockCount = 0;

static void initializeJSLock()
{
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&interpreterLock, &attr);
}

void JSLock::lock()
{
  pthread_once(&interpreterLockOnce, initializeJSLock);
  int result;
  result = pthread_mutex_lock(&interpreterLock);
  ASSERT(result == 0);
  interpreterLockCount++;
  Collector::registerThread();
}

void JSLock::unlock()
{
  interpreterLockCount--;
  int result;
  result = pthread_mutex_unlock(&interpreterLock);
  ASSERT(result == 0);
}

#else

// If threading support is off, set the lock count to a constant value of 1 so assertions
// that the lock is held don't fail
const int interpreterLockCount = 1;

void JSLock::lock()
{
}

void JSLock::unlock()
{
}

#endif

int JSLock::lockCount()
{
    return interpreterLockCount;
}
        
JSLock::DropAllLocks::DropAllLocks()
{
    int lockCount = JSLock::lockCount();
    for (int i = 0; i < lockCount; i++) {
        JSLock::unlock();
    }
    m_lockCount = lockCount;
}

JSLock::DropAllLocks::~DropAllLocks()
{
    int lockCount = m_lockCount;
    for (int i = 0; i < lockCount; i++) {
        JSLock::lock();
    }
}

}


#include "JSLockC.h"

#ifdef __cplusplus
extern "C" {
#endif

int JSLockDropAllLocks(void)
{
    KJS::JSLock::lock();
    int lockCount = KJS::JSLock::lockCount();
    for (int i = 0; i < lockCount; i++)
        KJS::JSLock::unlock();
    return lockCount - 1;
}

void JSLockRecoverAllLocks(int lockCount)
{
    ASSERT(KJS::JSLock::lockCount() == 0);
    for (int i = 0; i < lockCount; i++)
        KJS::JSLock::lock();
}    

static pthread_t javaScriptCollectionThread = 0;

void JSSetJavaScriptCollectionThread (pthread_t thread)
{
    javaScriptCollectionThread = thread;
}

pthread_t JSJavaScriptCollectionThread (void)
{
    return javaScriptCollectionThread;
}

#ifdef __cplusplus
}
#endif

