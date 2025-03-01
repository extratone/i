/*
 * Copyright 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DeviceMotionClient_h
#define DeviceMotionClient_h

#if PLATFORM(IOS)
#include <wtf/Noncopyable.h>
#endif
#include "DeviceClient.h"

namespace WebCore {

class DeviceMotionController;
class DeviceMotionData;
class Page;

class DeviceMotionClient : public DeviceClient {
#if PLATFORM(IOS)
    WTF_MAKE_NONCOPYABLE(DeviceMotionClient);
#endif
public:
#if PLATFORM(IOS)
    DeviceMotionClient() {}
#endif
    virtual ~DeviceMotionClient() {}
    virtual void setController(DeviceMotionController*) = 0;
    virtual DeviceMotionData* lastMotion() const = 0;
    virtual void deviceMotionControllerDestroyed() = 0;
};

void provideDeviceMotionTo(Page*, DeviceMotionClient*);

} // namespace WebCore

#endif // DeviceMotionClient_h
