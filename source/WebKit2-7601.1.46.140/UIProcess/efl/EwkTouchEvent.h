/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef EwkTouchEvent_h
#define EwkTouchEvent_h

#if ENABLE(TOUCH_EVENTS)

#include "APIObject.h"
#include "WKArray.h"
#include "WKEventEfl.h"
#include "WKRetainPtr.h"
#include <wtf/PassRefPtr.h>

namespace WebKit {

class EwkTouchEvent : public API::Object {
public:
    static const API::Object::Type APIType = API::Object::Type::TouchEvent;

    static PassRefPtr<EwkTouchEvent> create(WKEventType type, WKArrayRef touchPoints, WKEventModifiers modifiers, double timestamp)
    {
        return adoptRef(new EwkTouchEvent(type, touchPoints, modifiers, timestamp));
    }

    WKEventType eventType() const { return m_eventType; }
    WKArrayRef touchPoints() const { return m_touchPoints.get(); }
    WKEventModifiers modifiers() const { return m_modifiers; }
    double timestamp() const { return m_timestamp; }

private:
    EwkTouchEvent(WKEventType, WKArrayRef, WKEventModifiers, double timestamp);

    virtual API::Object::Type type() const { return APIType; }

    WKEventType m_eventType;
    WKRetainPtr<WKArrayRef> m_touchPoints;
    WKEventModifiers m_modifiers;
    double m_timestamp;
};

} // namespace WebKit

#endif // ENABLE(TOUCH_EVENTS)

#endif /* EwkTouchEvent_h */
