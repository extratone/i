/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 * No license or rights are granted by Apple expressly or by implication,
 * estoppel, or otherwise, to Apple copyrights, patents, trademarks, trade
 * secrets or other rights.
 */

#ifndef GestureEvent_h
#define GestureEvent_h

#include <wtf/Platform.h>

#if ENABLE(TOUCH_EVENTS)

#include <wtf/RefPtr.h>
#include "MouseRelatedEvent.h"
#include "EventTarget.h"

namespace WebCore {
    
    class GestureEvent : public MouseRelatedEvent {
    public:
        static PassRefPtr<GestureEvent> create()
        {
            return adoptRef(new GestureEvent);
        }
        static PassRefPtr<GestureEvent> create(
                     const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view, int detail,
                     int screenX, int screenY, int pageX, int pageY,
                     bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
                     EventTarget* target, float scale, float rotation, bool isSimulated = false)
        {
            return adoptRef(new GestureEvent(
                     type, canBubble, cancelable, view, detail,
                     screenX, screenY, pageX, pageY,
                     ctrlKey, altKey, shiftKey, metaKey,
                     target, scale, rotation, isSimulated));
        }
        virtual ~GestureEvent() {}

        void initGestureEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view, int detail,
            int screenX, int screenY, int clientX, int clientY,
            bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
            EventTarget* target, float scale, float rotation);

        virtual bool isGestureEvent() const { return true; }

        float scale() const { return m_scale; }
        float rotation() const { return m_rotation; }

    private:
        GestureEvent() { }
        GestureEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view, int detail,
                     int screenX, int screenY, int pageX, int pageY,
                     bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
                     EventTarget* target, float scale, float rotation, bool isSimulated = false);

        float m_scale;
        float m_rotation;
    };

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)

#endif // GestureEvent_h
