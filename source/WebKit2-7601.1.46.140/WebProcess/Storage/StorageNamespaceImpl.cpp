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

#include "config.h"
#include "StorageNamespaceImpl.h"

#include "StorageAreaImpl.h"
#include "StorageAreaMap.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebProcess.h"
#include <WebCore/PageGroup.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

RefPtr<StorageNamespaceImpl> StorageNamespaceImpl::createSessionStorageNamespace(uint64_t identifier, unsigned quotaInBytes)
{
    return adoptRef(new StorageNamespaceImpl(SessionStorage, identifier, nullptr, quotaInBytes));
}

RefPtr<StorageNamespaceImpl> StorageNamespaceImpl::createLocalStorageNamespace(uint64_t identifier, unsigned quotaInBytes)
{
    return adoptRef(new StorageNamespaceImpl(LocalStorage, identifier, nullptr, quotaInBytes));
}

RefPtr<StorageNamespaceImpl> StorageNamespaceImpl::createTransientLocalStorageNamespace(uint64_t identifier, WebCore::SecurityOrigin& topLevelOrigin, uint64_t quotaInBytes)
{
    return adoptRef(new StorageNamespaceImpl(LocalStorage, identifier, &topLevelOrigin, quotaInBytes));
}

StorageNamespaceImpl::StorageNamespaceImpl(WebCore::StorageType storageType, uint64_t storageNamespaceID, WebCore::SecurityOrigin* topLevelOrigin, unsigned quotaInBytes)
    : m_storageType(storageType)
    , m_storageNamespaceID(storageNamespaceID)
    , m_topLevelOrigin(topLevelOrigin)
    , m_quotaInBytes(quotaInBytes)
{
}

StorageNamespaceImpl::~StorageNamespaceImpl()
{
}

void StorageNamespaceImpl::didDestroyStorageAreaMap(StorageAreaMap& map)
{
    m_storageAreaMaps.remove(&map.securityOrigin());
}

PassRefPtr<StorageArea> StorageNamespaceImpl::storageArea(PassRefPtr<SecurityOrigin> securityOrigin)
{
    RefPtr<StorageAreaMap> map;

    auto& slot = m_storageAreaMaps.add(securityOrigin.get(), nullptr).iterator->value;
    if (!slot) {
        map = StorageAreaMap::create(this, *securityOrigin);
        slot = map.get();
    } else
        map = slot;

    return StorageAreaImpl::create(WTF::move(map));
}

PassRefPtr<StorageNamespace> StorageNamespaceImpl::copy(Page* newPage)
{
    ASSERT(m_storageNamespaceID);

    return createSessionStorageNamespace(WebPage::fromCorePage(newPage)->pageID(), m_quotaInBytes);
}

} // namespace WebKit
