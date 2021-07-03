/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "ShareableResource.h"

#if ENABLE(SHAREABLE_RESOURCE)

#include "ArgumentCoders.h"
#include <WebCore/SharedBuffer.h>

using namespace WebCore;

namespace WebKit {

ShareableResource::Handle::Handle()
{
}

void ShareableResource::Handle::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << m_handle;
    encoder << m_offset;
    encoder << m_size;
}

bool ShareableResource::Handle::decode(IPC::ArgumentDecoder& decoder, Handle& handle)
{
    if (!decoder.decode(handle.m_handle))
        return false;
    if (!decoder.decode(handle.m_offset))
        return false;
    if (!decoder.decode(handle.m_size))
        return false;
    return true;
}

#if USE(CF)
static void shareableResourceDeallocate(void *ptr, void *info)
{
    (static_cast<ShareableResource*>(info))->deref(); // Balanced by ref() in createShareableResourceDeallocator()
}
    
static CFAllocatorRef createShareableResourceDeallocator(ShareableResource* resource)
{
    CFAllocatorContext context = { 0,
        resource,
        NULL, // retain
        NULL, // release
        NULL, // copyDescription
        NULL, // allocate
        NULL, // reallocate
        shareableResourceDeallocate,
        NULL, // preferredSize
    };

    return CFAllocatorCreate(kCFAllocatorDefault, &context);
}
#endif

PassRefPtr<SharedBuffer> ShareableResource::wrapInSharedBuffer()
{
    ref(); // Balanced by deref when SharedBuffer is deallocated.

#if USE(CF)
    RetainPtr<CFAllocatorRef> deallocator = adoptCF(createShareableResourceDeallocator(this));
    RetainPtr<CFDataRef> cfData = adoptCF(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(data()), static_cast<CFIndex>(size()), deallocator.get()));
    return SharedBuffer::wrapCFData(cfData.get());
#elif USE(SOUP)
    return SharedBuffer::wrapSoupBuffer(soup_buffer_new_with_owner(data(), size(), this, [](void* data) { static_cast<ShareableResource*>(data)->deref(); }));
#else
    ASSERT_NOT_REACHED();
    return nullptr;
#endif
}

PassRefPtr<SharedBuffer> ShareableResource::Handle::tryWrapInSharedBuffer() const
{
    RefPtr<ShareableResource> resource = ShareableResource::map(*this);
    if (!resource) {
        LOG_ERROR("Failed to recreate ShareableResource from handle.");
        return nullptr;
    }

    return resource->wrapInSharedBuffer();
}

Ref<ShareableResource> ShareableResource::create(PassRefPtr<SharedMemory> sharedMemory, unsigned offset, unsigned size)
{
    return adoptRef(*new ShareableResource(sharedMemory, offset, size));
}

PassRefPtr<ShareableResource> ShareableResource::map(const Handle& handle)
{
    RefPtr<SharedMemory> sharedMemory = SharedMemory::map(handle.m_handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return 0;

    return create(sharedMemory.release(), handle.m_offset, handle.m_size);
}

ShareableResource::ShareableResource(PassRefPtr<SharedMemory> sharedMemory, unsigned offset, unsigned size)
    : m_sharedMemory(sharedMemory)
    , m_offset(offset)
    , m_size(size)
{
    ASSERT(m_sharedMemory);
    ASSERT(m_offset + m_size <= m_sharedMemory->size());
    
    // FIXME (NetworkProcess): This data was received from another process.  If it is bogus, should we assume that process is compromised and we should kill it?
}

ShareableResource::~ShareableResource()
{
}

bool ShareableResource::createHandle(Handle& handle)
{
    if (!m_sharedMemory->createHandle(handle.m_handle, SharedMemory::Protection::ReadOnly))
        return false;

    handle.m_offset = m_offset;
    handle.m_size = m_size;

    return true;
}

const char* ShareableResource::data() const
{
    return static_cast<const char*>(m_sharedMemory->data()) + m_offset;
}

unsigned ShareableResource::size() const
{
    return m_size;
}
    
} // namespace WebKit

#endif // ENABLE(SHAREABLE_RESOURCE)
