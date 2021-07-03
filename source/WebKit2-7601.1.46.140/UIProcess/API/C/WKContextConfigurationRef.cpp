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
#include "WKContextConfigurationRef.h"

#include "APIProcessPoolConfiguration.h"
#include "WKAPICast.h"

using namespace WebKit;

WKContextConfigurationRef WKContextConfigurationCreate()
{
    auto configuration = API::ProcessPoolConfiguration::create();
    
    // FIXME: A context created like this shouldn't have a data store,
    // instead there should be a WKPageConfigurationRef object that specifies the data store.
    configuration->setShouldHaveLegacyDataStore(true);
    
    return toAPI(&configuration.leakRef());
}

WKStringRef WKContextConfigurationCopyDiskCacheDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->diskCacheDirectory());
}

void WKContextConfigurationSetDiskCacheDirectory(WKContextConfigurationRef configuration, WKStringRef diskCacheDirectory)
{
    toImpl(configuration)->setDiskCacheDirectory(toImpl(diskCacheDirectory)->string());
}

WKStringRef WKContextConfigurationCopyApplicationCacheDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->applicationCacheDirectory());
}

void WKContextConfigurationSetApplicationCacheDirectory(WKContextConfigurationRef configuration, WKStringRef applicationCacheDirectory)
{
    toImpl(configuration)->setApplicationCacheDirectory(toImpl(applicationCacheDirectory)->string());
}

WKStringRef WKContextConfigurationCopyIndexedDBDatabaseDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->indexedDBDatabaseDirectory());
}

void WKContextConfigurationSetIndexedDBDatabaseDirectory(WKContextConfigurationRef configuration, WKStringRef indexedDBDatabaseDirectory)
{
    toImpl(configuration)->setIndexedDBDatabaseDirectory(toImpl(indexedDBDatabaseDirectory)->string());
}

WKStringRef WKContextConfigurationCopyInjectedBundlePath(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->injectedBundlePath());
}

void WKContextConfigurationSetInjectedBundlePath(WKContextConfigurationRef configuration, WKStringRef injectedBundlePath)
{
    toImpl(configuration)->setInjectedBundlePath(toImpl(injectedBundlePath)->string());
}

WKStringRef WKContextConfigurationCopyLocalStorageDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->localStorageDirectory());
}

void WKContextConfigurationSetLocalStorageDirectory(WKContextConfigurationRef configuration, WKStringRef localStorageDirectory)
{
    toImpl(configuration)->setLocalStorageDirectory(toImpl(localStorageDirectory)->string());
}

WKStringRef WKContextConfigurationCopyWebSQLDatabaseDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->webSQLDatabaseDirectory());
}

void WKContextConfigurationSetWebSQLDatabaseDirectory(WKContextConfigurationRef configuration, WKStringRef webSQLDatabaseDirectory)
{
    toImpl(configuration)->setWebSQLDatabaseDirectory(toImpl(webSQLDatabaseDirectory)->string());
}

WKStringRef WKContextConfigurationCopyMediaKeysStorageDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->mediaKeysStorageDirectory());
}

void WKContextConfigurationSetMediaKeysStorageDirectory(WKContextConfigurationRef configuration, WKStringRef mediaKeysStorageDirectory)
{
    toImpl(configuration)->setMediaKeysStorageDirectory(toImpl(mediaKeysStorageDirectory)->string());
}
