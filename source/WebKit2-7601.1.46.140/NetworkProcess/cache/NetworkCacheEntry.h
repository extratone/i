/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef NetworkCacheEntry_h
#define NetworkCacheEntry_h

#if ENABLE(NETWORK_CACHE)

#include "NetworkCacheStorage.h"
#include "ShareableResource.h"
#include <WebCore/ResourceResponse.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class ResourceRequest;
class SharedBuffer;
}

namespace WebKit {
namespace NetworkCache {

class Entry {
    WTF_MAKE_NONCOPYABLE(Entry); WTF_MAKE_FAST_ALLOCATED;
public:
    Entry(const Key&, const WebCore::ResourceResponse&, RefPtr<WebCore::SharedBuffer>&&, const Vector<std::pair<String, String>>& varyingRequestHeaders);

    Storage::Record encodeAsStorageRecord() const;
    static std::unique_ptr<Entry> decodeStorageRecord(const Storage::Record&);

    const Key& key() const { return m_key; }
    std::chrono::system_clock::time_point timeStamp() const { return m_timeStamp; }
    const WebCore::ResourceResponse& response() const { return m_response; }
    const Vector<std::pair<String, String>>& varyingRequestHeaders() const { return m_varyingRequestHeaders; }

    WebCore::SharedBuffer* buffer() const;
#if ENABLE(SHAREABLE_RESOURCE)
    ShareableResource::Handle& shareableResourceHandle() const;
#endif

    bool needsValidation() const;
    void setNeedsValidation();

    const Storage::Record& sourceStorageRecord() const { return m_sourceStorageRecord; }

    void asJSON(StringBuilder&, const Storage::RecordInfo&) const;

private:
    Entry(const Storage::Record&);
    void initializeBufferFromStorageRecord() const;
#if ENABLE(SHAREABLE_RESOURCE)
    void initializeShareableResourceHandleFromStorageRecord() const;
#endif

    Key m_key;
    std::chrono::system_clock::time_point m_timeStamp;
    WebCore::ResourceResponse m_response;
    Vector<std::pair<String, String>> m_varyingRequestHeaders;

    mutable RefPtr<WebCore::SharedBuffer> m_buffer;
#if ENABLE(SHAREABLE_RESOURCE)
    mutable ShareableResource::Handle m_shareableResourceHandle;
#endif

    Storage::Record m_sourceStorageRecord { };
};

}
}

#endif
#endif
