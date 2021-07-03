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
#include "NetworkCacheIOChannel.h"

#if ENABLE(NETWORK_CACHE)

#include "NetworkCacheFileSystem.h"
#include <dispatch/dispatch.h>
#include <mach/vm_param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
namespace NetworkCache {

IOChannel::IOChannel(const String& filePath, Type type)
    : m_path(filePath)
    , m_type(type)
{
    auto path = WebCore::fileSystemRepresentation(filePath);
    int oflag;
    mode_t mode;
    bool useLowIOPriority = false;

    switch (m_type) {
    case Type::Create:
        // We don't want to truncate any existing file (with O_TRUNC) as another thread might be mapping it.
        unlink(path.data());
        oflag = O_RDWR | O_CREAT | O_NONBLOCK;
        mode = S_IRUSR | S_IWUSR;
        useLowIOPriority = true;
        break;
    case Type::Write:
        oflag = O_WRONLY | O_NONBLOCK;
        mode = S_IRUSR | S_IWUSR;
        useLowIOPriority = true;
        break;
    case Type::Read:
        oflag = O_RDONLY | O_NONBLOCK;
        mode = 0;
    }

    int fd = ::open(path.data(), oflag, mode);
    m_fileDescriptor = fd;

    m_dispatchIO = adoptDispatch(dispatch_io_create(DISPATCH_IO_RANDOM, fd, dispatch_get_main_queue(), [fd](int) {
        close(fd);
    }));
    ASSERT(m_dispatchIO.get());

    // This makes the channel read/write all data before invoking the handlers.
    dispatch_io_set_low_water(m_dispatchIO.get(), std::numeric_limits<size_t>::max());

    if (useLowIOPriority) {
        // The target queue of a dispatch I/O channel specifies the priority of the global queue where its I/O operations are executed.
        dispatch_set_target_queue(m_dispatchIO.get(), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0));
    }
}

Ref<IOChannel> IOChannel::open(const String& filePath, IOChannel::Type type)
{
    return adoptRef(*new IOChannel(filePath, type));
}

void IOChannel::read(size_t offset, size_t size, WorkQueue* queue, std::function<void (Data&, int error)> completionHandler)
{
    RefPtr<IOChannel> channel(this);
    bool didCallCompletionHandler = false;
    auto dispatchQueue = queue ? queue->dispatchQueue() : dispatch_get_main_queue();
    dispatch_io_read(m_dispatchIO.get(), offset, size, dispatchQueue, [channel, completionHandler, didCallCompletionHandler](bool done, dispatch_data_t fileData, int error) mutable {
        ASSERT_UNUSED(done, done || !didCallCompletionHandler);
        if (didCallCompletionHandler)
            return;
        DispatchPtr<dispatch_data_t> fileDataPtr(fileData);
        Data data(fileDataPtr);
        completionHandler(data, error);
        didCallCompletionHandler = true;
    });
}

void IOChannel::write(size_t offset, const Data& data, WorkQueue* queue, std::function<void (int error)> completionHandler)
{
    RefPtr<IOChannel> channel(this);
    auto dispatchData = data.dispatchData();
    auto dispatchQueue = queue ? queue->dispatchQueue() : dispatch_get_main_queue();
    dispatch_io_write(m_dispatchIO.get(), offset, dispatchData, dispatchQueue, [channel, completionHandler](bool done, dispatch_data_t fileData, int error) {
        ASSERT_UNUSED(done, done);
        completionHandler(error);
    });
}

}
}

#endif
