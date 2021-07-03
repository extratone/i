/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 University of Szeged
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#if USE(UNIX_DOMAIN_SOCKETS)
#include "SharedMemory.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>
#include <wtf/RandomNumber.h>
#include <wtf/UniStdExtras.h>
#include <wtf/text/CString.h>

namespace WebKit {

SharedMemory::Handle::Handle()
{
}

SharedMemory::Handle::~Handle()
{
}

void SharedMemory::Handle::clear()
{
    m_attachment = IPC::Attachment();
}

bool SharedMemory::Handle::isNull() const
{
    return m_attachment.fileDescriptor() == -1;
}

void SharedMemory::Handle::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << releaseAttachment();
}

bool SharedMemory::Handle::decode(IPC::ArgumentDecoder& decoder, Handle& handle)
{
    ASSERT_ARG(handle, handle.isNull());

    IPC::Attachment attachment;
    if (!decoder.decode(attachment))
        return false;

    handle.adoptAttachment(WTF::move(attachment));
    return true;
}

IPC::Attachment SharedMemory::Handle::releaseAttachment() const
{
    return WTF::move(m_attachment);
}

void SharedMemory::Handle::adoptAttachment(IPC::Attachment&& attachment)
{
    ASSERT(isNull());

    m_attachment = WTF::move(attachment);
}

RefPtr<SharedMemory> SharedMemory::allocate(size_t size)
{
    CString tempName;

    int fileDescriptor = -1;
    for (int tries = 0; fileDescriptor == -1 && tries < 10; ++tries) {
        String name = String("/WK2SharedMemory.") + String::number(static_cast<unsigned>(WTF::randomNumber() * (std::numeric_limits<unsigned>::max() + 1.0)));
        tempName = name.utf8();

        do {
            fileDescriptor = shm_open(tempName.data(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        } while (fileDescriptor == -1 && errno == EINTR);
    }
    if (fileDescriptor == -1) {
        WTFLogAlways("Failed to create shared memory file %s: %s", tempName.data(), strerror(errno));
        return 0;
    }

    while (ftruncate(fileDescriptor, size) == -1) {
        if (errno != EINTR) {
            closeWithRetry(fileDescriptor);
            shm_unlink(tempName.data());
            return 0;
        }
    }

    void* data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileDescriptor, 0);
    if (data == MAP_FAILED) {
        closeWithRetry(fileDescriptor);
        shm_unlink(tempName.data());
        return 0;
    }

    shm_unlink(tempName.data());

    RefPtr<SharedMemory> instance = adoptRef(new SharedMemory());
    instance->m_data = data;
    instance->m_fileDescriptor = fileDescriptor;
    instance->m_size = size;
    return instance.release();
}

static inline int accessModeMMap(SharedMemory::Protection protection)
{
    switch (protection) {
    case SharedMemory::Protection::ReadOnly:
        return PROT_READ;
    case SharedMemory::Protection::ReadWrite:
        return PROT_READ | PROT_WRITE;
    }

    ASSERT_NOT_REACHED();
    return PROT_READ | PROT_WRITE;
}

RefPtr<SharedMemory> SharedMemory::map(const Handle& handle, Protection protection)
{
    ASSERT(!handle.isNull());

    void* data = mmap(0, handle.m_attachment.size(), accessModeMMap(protection), MAP_SHARED, handle.m_attachment.fileDescriptor(), 0);
    if (data == MAP_FAILED)
        return nullptr;

    RefPtr<SharedMemory> instance = wrapMap(data, handle.m_attachment.size(), handle.m_attachment.releaseFileDescriptor());
    instance->m_isWrappingMap = false;
    return instance;
}

RefPtr<SharedMemory> SharedMemory::wrapMap(void* data, size_t size, int fileDescriptor)
{
    RefPtr<SharedMemory> instance = adoptRef(new SharedMemory());
    instance->m_data = data;
    instance->m_size = size;
    instance->m_fileDescriptor = fileDescriptor;
    instance->m_isWrappingMap = true;
    return instance;
}

SharedMemory::~SharedMemory()
{
    if (!m_isWrappingMap) {
        munmap(m_data, m_size);
        closeWithRetry(m_fileDescriptor);
    }
}

bool SharedMemory::createHandle(Handle& handle, Protection)
{
    ASSERT_ARG(handle, handle.isNull());

    // FIXME: Handle the case where the passed Protection is ReadOnly.
    // See https://bugs.webkit.org/show_bug.cgi?id=131542.

    int duplicatedHandle;
    while ((duplicatedHandle = dup(m_fileDescriptor)) == -1) {
        if (errno != EINTR) {
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    while (fcntl(duplicatedHandle, F_SETFD, FD_CLOEXEC) == -1) {
        if (errno != EINTR) {
            ASSERT_NOT_REACHED();
            closeWithRetry(duplicatedHandle);
            return false;
        }
    }
    handle.m_attachment = IPC::Attachment(duplicatedHandle, m_size);
    return true;
}

unsigned SharedMemory::systemPageSize()
{
    static unsigned pageSize = 0;

    if (!pageSize)
        pageSize = getpagesize();

    return pageSize;
}

} // namespace WebKit

#endif

