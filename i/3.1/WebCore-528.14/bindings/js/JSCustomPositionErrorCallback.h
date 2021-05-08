/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSCustomPositionErrorCallback_h
#define JSCustomPositionErrorCallback_h

#include "PositionErrorCallback.h"
#include <runtime/JSObject.h>
#include <runtime/Protect.h>
#include <wtf/Forward.h>

namespace JSC {
    class JSObject;
}

namespace WebCore {
    
class Frame;
class PositionError;

class JSCustomPositionErrorCallback : public PositionErrorCallback {
public:
    static PassRefPtr<JSCustomPositionErrorCallback> create(JSC::JSObject* callback, Frame* frame) { return adoptRef(new JSCustomPositionErrorCallback(callback, frame)); }
    
    virtual void handleEvent(PositionError*);

private:
    JSCustomPositionErrorCallback(JSC::JSObject* callback, Frame*);

    JSC::ProtectedPtr<JSC::JSObject> m_callback;
    RefPtr<Frame> m_frame;
};
    
} // namespace WebCore

#endif // JSCustomPositionErrorCallback_h
