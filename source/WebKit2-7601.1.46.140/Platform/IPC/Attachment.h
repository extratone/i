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

#ifndef Attachment_h
#define Attachment_h

#if OS(DARWIN)
#include <mach/mach_init.h>
#include <mach/mach_traps.h>
#endif

namespace IPC {

class ArgumentDecoder;
class ArgumentEncoder;

class Attachment {
public:
    Attachment();

    enum Type {
        Uninitialized,
#if OS(DARWIN)
        MachPortType,
#elif USE(UNIX_DOMAIN_SOCKETS)
        SocketType,
        MappedMemoryType
#endif
    };

#if OS(DARWIN)
    Attachment(mach_port_name_t port, mach_msg_type_name_t disposition);
#elif USE(UNIX_DOMAIN_SOCKETS)
    Attachment(Attachment&&);
    Attachment& operator=(Attachment&&);
    Attachment(int fileDescriptor, size_t);
    Attachment(int fileDescriptor);
    ~Attachment();
#endif

    Type type() const { return m_type; }

#if OS(DARWIN)
    void release();

    // MachPortType
    mach_port_name_t port() const { return m_port; }
    mach_msg_type_name_t disposition() const { return m_disposition; }

#elif USE(UNIX_DOMAIN_SOCKETS)
    size_t size() const { return m_size; }

    int releaseFileDescriptor() { int temp = m_fileDescriptor; m_fileDescriptor = -1; return temp; }
    int fileDescriptor() const { return m_fileDescriptor; }
#endif

    void encode(ArgumentEncoder&) const;
    static bool decode(ArgumentDecoder&, Attachment&);
    
private:
    Type m_type;

#if OS(DARWIN)
    mach_port_name_t m_port;
    mach_msg_type_name_t m_disposition;
#elif USE(UNIX_DOMAIN_SOCKETS)
    int m_fileDescriptor { -1 };
    size_t m_size;
#endif
};

} // namespace IPC

#endif // Attachment_h
