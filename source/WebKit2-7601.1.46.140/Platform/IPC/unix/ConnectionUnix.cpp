/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Igalia S.L.
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
#include "Connection.h"

#include "DataReference.h"
#include "SharedMemory.h"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniStdExtras.h>

#if PLATFORM(GTK)
#include <glib.h>
#endif

#ifdef SOCK_SEQPACKET
#define SOCKET_TYPE SOCK_SEQPACKET
#else
#if PLATFORM(GTK)
#define SOCKET_TYPE SOCK_STREAM
#else
#define SOCKET_TYPE SOCK_DGRAM
#endif
#endif // SOCK_SEQPACKET

namespace IPC {

static const size_t messageMaxSize = 4096;
static const size_t attachmentMaxAmount = 255;

enum {
    MessageBodyIsOutOfLine = 1U << 31
};

class MessageInfo {
public:
    MessageInfo() { }

    MessageInfo(size_t bodySize, size_t initialAttachmentCount)
        : m_bodySize(bodySize)
        , m_attachmentCount(initialAttachmentCount)
        , m_isMessageBodyOutOfLine(false)
    {
    }

    void setMessageBodyIsOutOfLine()
    {
        ASSERT(!isMessageBodyIsOutOfLine());

        m_isMessageBodyOutOfLine = true;
        m_attachmentCount++;
    }

    bool isMessageBodyIsOutOfLine() const { return m_isMessageBodyOutOfLine; }

    size_t bodySize() const { return m_bodySize; }

    size_t attachmentCount() const { return m_attachmentCount; }

private:
    size_t m_bodySize;
    size_t m_attachmentCount;
    bool m_isMessageBodyOutOfLine;
};

class AttachmentInfo {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AttachmentInfo()
        : m_type(Attachment::Uninitialized)
        , m_size(0)
        , m_isNull(false)
    {
    }

    void setType(Attachment::Type type) { m_type = type; }
    Attachment::Type getType() { return m_type; }
    void setSize(size_t size)
    {
        ASSERT(m_type == Attachment::MappedMemoryType);
        m_size = size;
    }

    size_t getSize()
    {
        ASSERT(m_type == Attachment::MappedMemoryType);
        return m_size;
    }

    // The attachment is not null unless explicitly set.
    void setNull() { m_isNull = true; }
    bool isNull() { return m_isNull; }

private:
    Attachment::Type m_type;
    size_t m_size;
    bool m_isNull;
};

void Connection::platformInitialize(Identifier identifier)
{
    m_socketDescriptor = identifier;
    m_readBuffer.resize(messageMaxSize);
    m_readBufferSize = 0;
    m_fileDescriptors.resize(attachmentMaxAmount);
    m_fileDescriptorsSize = 0;
}

void Connection::platformInvalidate()
{
    // In GTK+ platform the socket is closed by the work queue.
#if !PLATFORM(GTK)
    if (m_socketDescriptor != -1)
        closeWithRetry(m_socketDescriptor);
#endif

    if (!m_isConnected)
        return;

#if PLATFORM(GTK) || PLATFORM(EFL)
    m_connectionQueue->unregisterSocketEventHandler(m_socketDescriptor);
#endif

    m_socketDescriptor = -1;
    m_isConnected = false;
}

bool Connection::processMessage()
{
    if (m_readBufferSize < sizeof(MessageInfo))
        return false;

    uint8_t* messageData = m_readBuffer.data();
    MessageInfo messageInfo;
    memcpy(&messageInfo, messageData, sizeof(messageInfo));
    messageData += sizeof(messageInfo);

    size_t messageLength = sizeof(MessageInfo) + messageInfo.attachmentCount() * sizeof(AttachmentInfo) + (messageInfo.isMessageBodyIsOutOfLine() ? 0 : messageInfo.bodySize());
    if (m_readBufferSize < messageLength)
        return false;

    size_t attachmentFileDescriptorCount = 0;
    size_t attachmentCount = messageInfo.attachmentCount();
    std::unique_ptr<AttachmentInfo[]> attachmentInfo;

    if (attachmentCount) {
        attachmentInfo = std::make_unique<AttachmentInfo[]>(attachmentCount);
        memcpy(attachmentInfo.get(), messageData, sizeof(AttachmentInfo) * attachmentCount);
        messageData += sizeof(AttachmentInfo) * attachmentCount;

        for (size_t i = 0; i < attachmentCount; ++i) {
            switch (attachmentInfo[i].getType()) {
            case Attachment::MappedMemoryType:
            case Attachment::SocketType:
                if (!attachmentInfo[i].isNull())
                    attachmentFileDescriptorCount++;
                break;
            case Attachment::Uninitialized:
            default:
                break;
            }
        }

        if (messageInfo.isMessageBodyIsOutOfLine())
            attachmentCount--;
    }

    Vector<Attachment> attachments(attachmentCount);
    RefPtr<WebKit::SharedMemory> oolMessageBody;

    size_t fdIndex = 0;
    for (size_t i = 0; i < attachmentCount; ++i) {
        int fd = -1;
        switch (attachmentInfo[i].getType()) {
        case Attachment::MappedMemoryType:
            if (!attachmentInfo[i].isNull())
                fd = m_fileDescriptors[fdIndex++];
            attachments[attachmentCount - i - 1] = Attachment(fd, attachmentInfo[i].getSize());
            break;
        case Attachment::SocketType:
            if (!attachmentInfo[i].isNull())
                fd = m_fileDescriptors[fdIndex++];
            attachments[attachmentCount - i - 1] = Attachment(fd);
            break;
        case Attachment::Uninitialized:
            attachments[attachmentCount - i - 1] = Attachment();
        default:
            break;
        }
    }

    if (messageInfo.isMessageBodyIsOutOfLine()) {
        ASSERT(messageInfo.bodySize());

        if (attachmentInfo[attachmentCount].isNull()) {
            ASSERT_NOT_REACHED();
            return false;
        }

        WebKit::SharedMemory::Handle handle;
        handle.adoptAttachment(IPC::Attachment(m_fileDescriptors[attachmentFileDescriptorCount - 1], attachmentInfo[attachmentCount].getSize()));

        oolMessageBody = WebKit::SharedMemory::map(handle, WebKit::SharedMemory::Protection::ReadOnly);
        if (!oolMessageBody) {
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    ASSERT(attachments.size() == (messageInfo.isMessageBodyIsOutOfLine() ? messageInfo.attachmentCount() - 1 : messageInfo.attachmentCount()));

    uint8_t* messageBody = messageData;
    if (messageInfo.isMessageBodyIsOutOfLine())
        messageBody = reinterpret_cast<uint8_t*>(oolMessageBody->data());

    auto decoder = std::make_unique<MessageDecoder>(DataReference(messageBody, messageInfo.bodySize()), WTF::move(attachments));

    processIncomingMessage(WTF::move(decoder));

    if (m_readBufferSize > messageLength) {
        memmove(m_readBuffer.data(), m_readBuffer.data() + messageLength, m_readBufferSize - messageLength);
        m_readBufferSize -= messageLength;
    } else
        m_readBufferSize = 0;

    if (attachmentFileDescriptorCount) {
        if (m_fileDescriptorsSize > attachmentFileDescriptorCount) {
            size_t fileDescriptorsLength = attachmentFileDescriptorCount * sizeof(int);
            memmove(m_fileDescriptors.data(), m_fileDescriptors.data() + fileDescriptorsLength, m_fileDescriptorsSize - fileDescriptorsLength);
            m_fileDescriptorsSize -= fileDescriptorsLength;
        } else
            m_fileDescriptorsSize = 0;
    }


    return true;
}

static ssize_t readBytesFromSocket(int socketDescriptor, uint8_t* buffer, int count, int* fileDescriptors, size_t* fileDescriptorsCount)
{
    struct msghdr message;
    memset(&message, 0, sizeof(message));

    struct iovec iov[1];
    memset(&iov, 0, sizeof(iov));

    message.msg_controllen = CMSG_SPACE(sizeof(int) * attachmentMaxAmount);
    MallocPtr<char> attachmentDescriptorBuffer = MallocPtr<char>::malloc(sizeof(char) * message.msg_controllen);
    memset(attachmentDescriptorBuffer.get(), 0, sizeof(char) * message.msg_controllen);
    message.msg_control = attachmentDescriptorBuffer.get();

    iov[0].iov_base = buffer;
    iov[0].iov_len = count;

    message.msg_iov = iov;
    message.msg_iovlen = 1;

    while (true) {
        ssize_t bytesRead = recvmsg(socketDescriptor, &message, 0);

        if (bytesRead < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        bool found = false;
        struct cmsghdr* controlMessage;
        for (controlMessage = CMSG_FIRSTHDR(&message); controlMessage; controlMessage = CMSG_NXTHDR(&message, controlMessage)) {
            if (controlMessage->cmsg_level == SOL_SOCKET && controlMessage->cmsg_type == SCM_RIGHTS) {
                *fileDescriptorsCount = (controlMessage->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                memcpy(fileDescriptors, CMSG_DATA(controlMessage), sizeof(int) * *fileDescriptorsCount);

                for (size_t i = 0; i < *fileDescriptorsCount; ++i) {
                    while (fcntl(fileDescriptors[i], F_SETFD, FD_CLOEXEC) == -1) {
                        if (errno != EINTR) {
                            ASSERT_NOT_REACHED();
                            break;
                        }
                    }
                }

                found = true;
                break;
            }
        }

        if (!found)
            *fileDescriptorsCount = 0;

        return bytesRead;
    }

    return -1;
}

void Connection::readyReadHandler()
{
    while (true) {
        size_t fileDescriptorsCount = 0;
        size_t bytesToRead = m_readBuffer.size() - m_readBufferSize;
        ssize_t bytesRead = readBytesFromSocket(m_socketDescriptor, m_readBuffer.data() + m_readBufferSize, bytesToRead,
                                                m_fileDescriptors.data() + m_fileDescriptorsSize, &fileDescriptorsCount);

        if (bytesRead < 0) {
            // EINTR was already handled by readBytesFromSocket.
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;

            WTFLogAlways("Error receiving IPC message on socket %d in process %d: %s", m_socketDescriptor, getpid(), strerror(errno));
            connectionDidClose();
            return;
        }

        m_readBufferSize += bytesRead;
        m_fileDescriptorsSize += fileDescriptorsCount;

        if (!bytesRead) {
            connectionDidClose();
            return;
        }

        // Process messages from data received.
        while (true) {
            if (!processMessage())
                break;
        }
    }
}

bool Connection::open()
{
    int flags = fcntl(m_socketDescriptor, F_GETFL, 0);
    while (fcntl(m_socketDescriptor, F_SETFL, flags | O_NONBLOCK) == -1) {
        if (errno != EINTR) {
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    RefPtr<Connection> protectedThis(this);
    m_isConnected = true;
#if PLATFORM(GTK)
    m_connectionQueue->registerSocketEventHandler(m_socketDescriptor,
        [protectedThis] {
            protectedThis->readyReadHandler();
        },
        [protectedThis] {
            protectedThis->connectionDidClose();
        });
#elif PLATFORM(EFL)
    m_connectionQueue->registerSocketEventHandler(m_socketDescriptor,
        [protectedThis] {
            protectedThis->readyReadHandler();
        });
#endif

    // Schedule a call to readyReadHandler. Data may have arrived before installation of the signal handler.
    m_connectionQueue->dispatch([protectedThis] {
        protectedThis->readyReadHandler();
    });

    return true;
}

bool Connection::platformCanSendOutgoingMessages() const
{
    return m_isConnected;
}

bool Connection::sendOutgoingMessage(std::unique_ptr<MessageEncoder> encoder)
{
    COMPILE_ASSERT(sizeof(MessageInfo) + attachmentMaxAmount * sizeof(size_t) <= messageMaxSize, AttachmentsFitToMessageInline);

    Vector<Attachment> attachments = encoder->releaseAttachments();
    if (attachments.size() > (attachmentMaxAmount - 1)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    MessageInfo messageInfo(encoder->bufferSize(), attachments.size());
    size_t messageSizeWithBodyInline = sizeof(messageInfo) + (attachments.size() * sizeof(AttachmentInfo)) + encoder->bufferSize();
    if (messageSizeWithBodyInline > messageMaxSize && encoder->bufferSize()) {
        RefPtr<WebKit::SharedMemory> oolMessageBody = WebKit::SharedMemory::allocate(encoder->bufferSize());
        if (!oolMessageBody)
            return false;

        WebKit::SharedMemory::Handle handle;
        if (!oolMessageBody->createHandle(handle, WebKit::SharedMemory::Protection::ReadOnly))
            return false;

        messageInfo.setMessageBodyIsOutOfLine();

        memcpy(oolMessageBody->data(), encoder->buffer(), encoder->bufferSize());

        attachments.append(handle.releaseAttachment());
    }

    struct msghdr message;
    memset(&message, 0, sizeof(message));

    struct iovec iov[3];
    memset(&iov, 0, sizeof(iov));

    message.msg_iov = iov;
    int iovLength = 1;

    iov[0].iov_base = reinterpret_cast<void*>(&messageInfo);
    iov[0].iov_len = sizeof(messageInfo);

    std::unique_ptr<AttachmentInfo[]> attachmentInfo;
    MallocPtr<char> attachmentFDBuffer;

    if (!attachments.isEmpty()) {
        int* fdPtr = 0;

        size_t attachmentFDBufferLength = std::count_if(attachments.begin(), attachments.end(),
            [](const Attachment& attachment) {
                return attachment.fileDescriptor() != -1;
            });

        if (attachmentFDBufferLength) {
            attachmentFDBuffer = MallocPtr<char>::malloc(sizeof(char) * CMSG_SPACE(sizeof(int) * attachmentFDBufferLength));

            message.msg_control = attachmentFDBuffer.get();
            message.msg_controllen = CMSG_SPACE(sizeof(int) * attachmentFDBufferLength);
            memset(message.msg_control, 0, message.msg_controllen);

            struct cmsghdr* cmsg = CMSG_FIRSTHDR(&message);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int) * attachmentFDBufferLength);

            fdPtr = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        }

        attachmentInfo = std::make_unique<AttachmentInfo[]>(attachments.size());
        int fdIndex = 0;
        for (size_t i = 0; i < attachments.size(); ++i) {
            attachmentInfo[i].setType(attachments[i].type());

            switch (attachments[i].type()) {
            case Attachment::MappedMemoryType:
                attachmentInfo[i].setSize(attachments[i].size());
                // Fall trhough, set file descriptor or null.
            case Attachment::SocketType:
                if (attachments[i].fileDescriptor() != -1) {
                    ASSERT(fdPtr);
                    fdPtr[fdIndex++] = attachments[i].fileDescriptor();
                } else
                    attachmentInfo[i].setNull();
                break;
            case Attachment::Uninitialized:
            default:
                break;
            }
        }

        iov[iovLength].iov_base = attachmentInfo.get();
        iov[iovLength].iov_len = sizeof(AttachmentInfo) * attachments.size();
        ++iovLength;
    }

    if (!messageInfo.isMessageBodyIsOutOfLine() && encoder->bufferSize()) {
        iov[iovLength].iov_base = reinterpret_cast<void*>(encoder->buffer());
        iov[iovLength].iov_len = encoder->bufferSize();
        ++iovLength;
    }

    message.msg_iovlen = iovLength;

    while (sendmsg(m_socketDescriptor, &message, 0) == -1) {
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            struct pollfd pollfd;

            pollfd.fd = m_socketDescriptor;
            pollfd.events = POLLOUT;
            pollfd.revents = 0;
            poll(&pollfd, 1, -1);
            continue;
        }

        WTFLogAlways("Error sending IPC message: %s", strerror(errno));
        return false;
    }
    return true;
}

Connection::SocketPair Connection::createPlatformConnection(unsigned options)
{
    int sockets[2];
    RELEASE_ASSERT(socketpair(AF_UNIX, SOCKET_TYPE, 0, sockets) != -1);

    if (options & SetCloexecOnServer) {
        // Don't expose the child socket to the parent process.
        while (fcntl(sockets[1], F_SETFD, FD_CLOEXEC)  == -1)
            RELEASE_ASSERT(errno != EINTR);
    }

    if (options & SetCloexecOnClient) {
        // Don't expose the parent socket to potential future children.
        while (fcntl(sockets[0], F_SETFD, FD_CLOEXEC) == -1)
            RELEASE_ASSERT(errno != EINTR);
    }

    SocketPair socketPair = { sockets[0], sockets[1] };
    return socketPair;
}
    
void Connection::willSendSyncMessage(unsigned flags)
{
    UNUSED_PARAM(flags);
}
    
void Connection::didReceiveSyncReply(unsigned flags)
{
    UNUSED_PARAM(flags);    
}

} // namespace IPC
