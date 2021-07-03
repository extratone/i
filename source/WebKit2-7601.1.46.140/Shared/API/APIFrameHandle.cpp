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
#include "APIFrameHandle.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"

namespace API {

Ref<FrameHandle> FrameHandle::create(uint64_t frameID)
{
    return adoptRef(*new FrameHandle(frameID, false));
}

Ref<FrameHandle> FrameHandle::createAutoconverting(uint64_t frameID)
{
    return adoptRef(*new FrameHandle(frameID, true));
}

FrameHandle::FrameHandle(uint64_t frameID, bool isAutoconverting)
    : m_frameID(frameID)
    , m_isAutoconverting(isAutoconverting)
{
}

FrameHandle::~FrameHandle()
{
}

void FrameHandle::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << m_frameID;
    encoder << m_isAutoconverting;
}

bool FrameHandle::decode(IPC::ArgumentDecoder& decoder, RefPtr<Object>& result)
{
    uint64_t frameID;
    if (!decoder.decode(frameID))
        return false;

    bool isAutoconverting;
    if (!decoder.decode(isAutoconverting))
        return false;

    result = isAutoconverting ? createAutoconverting(frameID) : create(frameID);
    return true;
}

} // namespace API
