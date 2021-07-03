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

#ifndef MessageEncoder_h
#define MessageEncoder_h

#include "ArgumentEncoder.h"
#include "StringReference.h"
#include <wtf/Forward.h>

#if HAVE(DTRACE)
#include <uuid/uuid.h>
#endif

namespace IPC {

class StringReference;

class MessageEncoder : public ArgumentEncoder {
public:
    MessageEncoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID);
#if HAVE(DTRACE)
    MessageEncoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, const uuid_t&);
#endif
    virtual ~MessageEncoder();

    StringReference messageReceiverName() const { return m_messageReceiverName; }
    StringReference messageName() const { return m_messageName; }
    uint64_t destinationID() const { return m_destinationID; }

    void setIsSyncMessage(bool);
    bool isSyncMessage() const;

    void setShouldDispatchMessageWhenWaitingForSyncReply(bool);
    bool shouldDispatchMessageWhenWaitingForSyncReply() const;

#if HAVE(DTRACE)
    const uuid_t& UUID() const { return m_UUID; }
#endif

private:
    void encodeHeader();

    StringReference m_messageReceiverName;
    StringReference m_messageName;
    uint64_t m_destinationID;
#if HAVE(DTRACE)
    uuid_t m_UUID;
#endif
};

} // namespace IPC

#endif // MessageEncoder_h
