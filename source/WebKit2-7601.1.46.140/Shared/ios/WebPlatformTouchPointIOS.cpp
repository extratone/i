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
#include "WebEvent.h"

#if ENABLE(TOUCH_EVENTS)

#include "Arguments.h"
#include "WebCoreArgumentCoders.h"

using namespace WebCore;

namespace WebKit {

void WebPlatformTouchPoint::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << m_identifier;
    encoder << m_location;
    encoder << m_phase;
#if ENABLE(IOS_TOUCH_EVENTS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 90000
    encoder << m_force;
#endif
}

bool WebPlatformTouchPoint::decode(IPC::ArgumentDecoder& decoder, WebPlatformTouchPoint& result)
{
    if (!decoder.decode(result.m_identifier))
        return false;
    if (!decoder.decode(result.m_location))
        return false;
    if (!decoder.decode(result.m_phase))
        return false;
#if ENABLE(IOS_TOUCH_EVENTS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 90000
    if (!decoder.decode(result.m_force))
        return false;
#endif
    return true;
}

} // namespace WebKit

#endif // ENABLE(TOUCH_EVENTS)
