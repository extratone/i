/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 * No license or rights are granted by Apple expressly or by implication,
 * estoppel, or otherwise, to Apple copyrights, patents, trademarks, trade
 * secrets or other rights.
 */

#ifndef PlatformTouchEvent_h
#define PlatformTouchEvent_h

#include <JavaScriptCore/Platform.h>

#if ENABLE(TOUCH_EVENTS)

#include <GraphicsServices/GSEvent.h>
#include <wtf/Vector.h>

#include "IntPoint.h"

namespace WebCore {
    
    enum TouchEventType { TouchEventBegin, TouchEventChange, TouchEventEnd, TouchEventCancel };
    
    class PlatformTouchEvent {
    public:
        PlatformTouchEvent(GSEventRef);
        
        TouchEventType eventType() const;
        double timestamp() const;
        unsigned touchCount() const;
        IntPoint touchLocationAtIndex(unsigned) const;
        IntPoint globalTouchLocationAtIndex(unsigned) const;
        unsigned touchIdentifierAtIndex(unsigned) const;
        
        bool gestureChanged() const;
        
        float scaleAbsolute() const;
        float rotationAbsolute() const;

        GSEventRef platformEvent() const { return m_gsEvent; }
    private:
        GSEventRef m_gsEvent;
        unsigned m_fingerCount;
        Vector<GSEventPathInfo> m_pathData;
    };
} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)

#endif // PlatformTouchEvent_h
