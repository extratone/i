/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WKApplicationCacheManager.h"

#include "APIWebsiteDataStore.h"
#include "WKAPICast.h"
#include "WebsiteDataRecord.h"

using namespace WebKit;

WKTypeID WKApplicationCacheManagerGetTypeID()
{
    return toAPI(API::WebsiteDataStore::APIType);
}

void WKApplicationCacheManagerGetApplicationCacheOrigins(WKApplicationCacheManagerRef applicationCacheManager, void* context, WKApplicationCacheManagerGetApplicationCacheOriginsFunction callback)
{
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(applicationCacheManager))->websiteDataStore();
    websiteDataStore.fetchData(WebsiteDataTypes::WebsiteDataTypeOfflineWebApplicationCache, [context, callback](Vector<WebsiteDataRecord> dataRecords) {
        Vector<RefPtr<API::Object>> securityOrigins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                securityOrigins.append(API::SecurityOrigin::create(*origin));
        }

        callback(toAPI(API::Array::create(WTF::move(securityOrigins)).ptr()), nullptr, context);
    });
}

void WKApplicationCacheManagerDeleteEntriesForOrigin(WKApplicationCacheManagerRef applicationCacheManager, WKSecurityOriginRef origin)
{
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(applicationCacheManager))->websiteDataStore();

    WebsiteDataRecord dataRecord;
    dataRecord.add(WebsiteDataTypes::WebsiteDataTypeOfflineWebApplicationCache, &toImpl(origin)->securityOrigin());

    websiteDataStore.removeData(WebsiteDataTypes::WebsiteDataTypeOfflineWebApplicationCache, { dataRecord }, [] { });
}

void WKApplicationCacheManagerDeleteAllEntries(WKApplicationCacheManagerRef applicationCacheManager)
{
    auto& websiteDataStore = toImpl(reinterpret_cast<WKWebsiteDataStoreRef>(applicationCacheManager))->websiteDataStore();
    websiteDataStore.removeData(WebsiteDataTypes::WebsiteDataTypeOfflineWebApplicationCache, std::chrono::system_clock::time_point::min(), [] { });
}
