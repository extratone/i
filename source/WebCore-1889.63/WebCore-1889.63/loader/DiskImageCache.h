/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef DiskImageCache_h
#define DiskImageCache_h

#if ENABLE(DISK_IMAGE_CACHE)

#include "DiskImageCacheClient.h"
#include "FileSystem.h"
#include "SharedBuffer.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>
#include <wtf/text/WTFString.h>

typedef unsigned disk_cache_id_t;

namespace WebCore {

// Global disk image cache object.
class DiskImageCache {
    WTF_MAKE_NONCOPYABLE(DiskImageCache);
private:

    // Internal entrys kept in a table.
    // Lifetime of this Entry and the SharedBuffer it wraps is managed on the WebThread.
    class Entry : public RefCounted<Entry> {
    private:
        Entry(SharedBuffer* buffer, disk_cache_id_t id);

    public:
        static PassRefPtr<DiskImageCache::Entry> create(SharedBuffer* buffer, disk_cache_id_t id)
        {
            return adoptRef(new DiskImageCache::Entry(buffer, id));
        }

        ~Entry();

        void map(const String& path);
        void unmap();
        void removeFile();
        void clearDataWithoutMapping();

        disk_cache_id_t id() const { return m_id; }
        unsigned size() const { return m_size; }
        void* data() const { return m_mapping; }
        bool isMapped() const { return m_mapping != NULL; }

    private:
        bool mapInternal(const String& path);
        int writeToFileInternal(PlatformFileHandle handle);

        SharedBuffer* m_buffer;
        disk_cache_id_t m_id;
        unsigned m_size;
        void* m_mapping;
        String m_path;
    };

public:
    friend DiskImageCache* diskImageCache();
    static const disk_cache_id_t invalidDiskCacheId = 0;

    // Write out an item.
    disk_cache_id_t writeItem(PassRefPtr<SharedBuffer>);

    // Remove item.
    void removeItem(disk_cache_id_t);

    // Mapping for an item.
    void* dataForItem(disk_cache_id_t);

    // Setup a client for callbacks.
    void setClient(PassRefPtr<DiskImageCacheClient> client) { m_client = client; }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    unsigned maximumCacheSize() const { return m_maximumCacheSize; }
    void setMaximumCacheSize(unsigned maximum) { m_maximumCacheSize = maximum; }

    unsigned minimumImageSize() const { return m_minimumImageSize; }
    void setMinimumImageSize(unsigned minimum) { m_minimumImageSize = minimum; }

    const String& cacheDirectory() const { return m_cacheDirectory; }

    unsigned size() const { return m_size; }
    bool isFull() const { return m_size >= m_maximumCacheSize; }

private:
    DiskImageCache();
    ~DiskImageCache() { }

    bool createDirectoryIfNeeded();
    disk_cache_id_t nextAvailableId();

    String temporaryDirectory();
    String temporaryFile();

    void updateSize(unsigned delta);

    bool m_enabled;
    unsigned m_size;
    unsigned m_maximumCacheSize;
    unsigned m_minimumImageSize;
    disk_cache_id_t m_nextAvailableId;
    String m_cacheDirectory;

    RefPtr<DiskImageCacheClient> m_client;
    HashMap<disk_cache_id_t, RefPtr<DiskImageCache::Entry> > m_table;

    Mutex m_mutex;
};

// Function to obtain the global disk image cache.
DiskImageCache* diskImageCache();

} // namespace WebCore

#endif // ENABLE(DISK_IMAGE_CACHE)

#endif // DiskImageCache_h
