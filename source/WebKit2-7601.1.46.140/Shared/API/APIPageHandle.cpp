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
#include "APIPageHandle.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"

namespace API {

Ref<PageHandle> PageHandle::create(uint64_t pageID)
{
    return adoptRef(*new PageHandle(pageID, false));
}

Ref<PageHandle> PageHandle::createAutoconverting(uint64_t pageID)
{
    return adoptRef(*new PageHandle(pageID, true));
}

PageHandle::PageHandle(uint64_t pageID, bool isAutoconverting)
    : m_pageID(pageID)
    , m_isAutoconverting(isAutoconverting)
{
}

PageHandle::~PageHandle()
{
}

void PageHandle::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << m_pageID;
    encoder << m_isAutoconverting;
}

bool PageHandle::decode(IPC::ArgumentDecoder& decoder, RefPtr<Object>& result)
{
    uint64_t pageID;
    if (!decoder.decode(pageID))
        return false;

    bool isAutoconverting;
    if (!decoder.decode(isAutoconverting))
        return false;

    result = isAutoconverting ? createAutoconverting(pageID) : create(pageID);
    return true;
}

} // namespace API
