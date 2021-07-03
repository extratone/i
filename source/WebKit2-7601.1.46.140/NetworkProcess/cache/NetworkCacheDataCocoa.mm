/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "NetworkCacheData.h"

#if ENABLE(NETWORK_CACHE)

#include "SharedMemory.h"
#include <dispatch/dispatch.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace WebKit {
namespace NetworkCache {

Data::Data(const uint8_t* data, size_t size)
    : m_dispatchData(adoptDispatch(dispatch_data_create(data, size, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT)))
    , m_size(size)
{
}

Data::Data(DispatchPtr<dispatch_data_t> dispatchData, Backing backing)
    : m_dispatchData(dispatchData)
    , m_size(m_dispatchData ? dispatch_data_get_size(m_dispatchData.get()) : 0)
    , m_isMap(m_size && backing == Backing::Map)
{
}

Data Data::empty()
{
    return { DispatchPtr<dispatch_data_t>(dispatch_data_empty) };
}

const uint8_t* Data::data() const
{
    if (!m_data && m_dispatchData) {
        const void* data;
        size_t size;
        m_dispatchData = adoptDispatch(dispatch_data_create_map(m_dispatchData.get(), &data, &size));
        ASSERT(size == m_size);
        m_data = static_cast<const uint8_t*>(data);
    }
    return m_data;
}

bool Data::isNull() const
{
    return !m_dispatchData;
}

bool Data::apply(const std::function<bool (const uint8_t*, size_t)>&& applier) const
{
    if (!m_size)
        return false;
    return dispatch_data_apply(m_dispatchData.get(), [&applier](dispatch_data_t, size_t, const void* data, size_t size) {
        return applier(static_cast<const uint8_t*>(data), size);
    });
}

Data Data::subrange(size_t offset, size_t size) const
{
    return { adoptDispatch(dispatch_data_create_subrange(dispatchData(), offset, size)) };
}

Data concatenate(const Data& a, const Data& b)
{
    if (a.isNull())
        return b;
    if (b.isNull())
        return a;
    return { adoptDispatch(dispatch_data_create_concat(a.dispatchData(), b.dispatchData())) };
}

Data Data::adoptMap(void* map, size_t size, int fd)
{
    ASSERT(map && map != MAP_FAILED);
    close(fd);
    auto bodyMap = adoptDispatch(dispatch_data_create(map, size, dispatch_get_main_queue(), [map, size] {
        munmap(map, size);
    }));
    return { bodyMap, Data::Backing::Map };
}

RefPtr<SharedMemory> Data::tryCreateSharedMemory() const
{
    if (isNull() || !isMap())
        return nullptr;

    return SharedMemory::create(const_cast<uint8_t*>(data()), m_size, SharedMemory::Protection::ReadOnly);
}

}
}

#endif
