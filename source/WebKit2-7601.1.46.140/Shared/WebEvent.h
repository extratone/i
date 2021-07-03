/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef WebEvent_h
#define WebEvent_h

// FIXME: We should probably move to makeing the WebCore/PlatformFooEvents trivial classes so that
// we can use them as the event type.

#include <WebCore/FloatPoint.h>
#include <WebCore/FloatSize.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>
#include <wtf/text/WTFString.h>

namespace IPC {
    class ArgumentDecoder;
    class ArgumentEncoder;
}

#if USE(APPKIT)
namespace WebCore {
struct KeypressCommand;
}
#endif

namespace WebKit {

class WebEvent {
public:
    enum Type {
        NoType = -1,
        
        // WebMouseEvent
        MouseDown,
        MouseUp,
        MouseMove,
        MouseForceChanged,
        MouseForceDown,
        MouseForceUp,

        // WebWheelEvent
        Wheel,

        // WebKeyboardEvent
        KeyDown,
        KeyUp,
        RawKeyDown,
        Char,

#if ENABLE(TOUCH_EVENTS)
        // WebTouchEvent
        TouchStart,
        TouchMove,
        TouchEnd,
        TouchCancel,
#endif
    };

    enum Modifiers {
        ShiftKey    = 1 << 0,
        ControlKey  = 1 << 1,
        AltKey      = 1 << 2,
        MetaKey     = 1 << 3,
        CapsLockKey = 1 << 4,
    };

    Type type() const { return static_cast<Type>(m_type); }

    bool shiftKey() const { return m_modifiers & ShiftKey; }
    bool controlKey() const { return m_modifiers & ControlKey; }
    bool altKey() const { return m_modifiers & AltKey; }
    bool metaKey() const { return m_modifiers & MetaKey; }
    bool capsLockKey() const { return m_modifiers & CapsLockKey; }

    Modifiers modifiers() const { return static_cast<Modifiers>(m_modifiers); }

    double timestamp() const { return m_timestamp; }

protected:
    WebEvent();

    WebEvent(Type, Modifiers, double timestamp);

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, WebEvent&);

private:
    uint32_t m_type; // Type
    uint32_t m_modifiers; // Modifiers
    double m_timestamp;
};

// FIXME: Move this class to its own header file.
class WebMouseEvent : public WebEvent {
public:
    enum Button {
        NoButton = -1,
        LeftButton,
        MiddleButton,
        RightButton
    };

    WebMouseEvent();

#if PLATFORM(MAC)
    WebMouseEvent(Type, Button, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, Modifiers, double timestamp, double force, int eventNumber = -1, int menuType = 0);
#else
    WebMouseEvent(Type, Button, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, Modifiers, double timestamp, double force = 0);
#endif

    Button button() const { return static_cast<Button>(m_button); }
    const WebCore::IntPoint& position() const { return m_position; }
    const WebCore::IntPoint& globalPosition() const { return m_globalPosition; }
    float deltaX() const { return m_deltaX; }
    float deltaY() const { return m_deltaY; }
    float deltaZ() const { return m_deltaZ; }
    int32_t clickCount() const { return m_clickCount; }
#if PLATFORM(MAC)
    int32_t eventNumber() const { return m_eventNumber; }
    int32_t menuTypeForEvent() const { return m_menuTypeForEvent; }
#endif
    double force() const { return m_force; }

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, WebMouseEvent&);

private:
    static bool isMouseEventType(Type);

    uint32_t m_button;
    WebCore::IntPoint m_position;
    WebCore::IntPoint m_globalPosition;
    float m_deltaX;
    float m_deltaY;
    float m_deltaZ;
    int32_t m_clickCount;
#if PLATFORM(MAC)
    int32_t m_eventNumber;
    int32_t m_menuTypeForEvent;
#endif
    double m_force { 0 };
};

// FIXME: Move this class to its own header file.
class WebWheelEvent : public WebEvent {
public:
    enum Granularity {
        ScrollByPageWheelEvent,
        ScrollByPixelWheelEvent
    };

#if PLATFORM(COCOA)
    enum Phase {
        PhaseNone        = 0,
        PhaseBegan       = 1 << 0,
        PhaseStationary  = 1 << 1,
        PhaseChanged     = 1 << 2,
        PhaseEnded       = 1 << 3,
        PhaseCancelled   = 1 << 4,
        PhaseMayBegin    = 1 << 5,
    };
#endif

    WebWheelEvent() { }

    WebWheelEvent(Type, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Granularity, Modifiers, double timestamp);
#if PLATFORM(COCOA)
    WebWheelEvent(Type, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Granularity, bool directionInvertedFromDevice, Phase, Phase momentumPhase, bool hasPreciseScrollingDeltas, uint32_t scrollCount, const WebCore::FloatSize& unacceleratedScrollingDelta, Modifiers, double timestamp);
#endif

    const WebCore::IntPoint position() const { return m_position; }
    const WebCore::IntPoint globalPosition() const { return m_globalPosition; }
    const WebCore::FloatSize delta() const { return m_delta; }
    const WebCore::FloatSize wheelTicks() const { return m_wheelTicks; }
    Granularity granularity() const { return static_cast<Granularity>(m_granularity); }
    bool directionInvertedFromDevice() const { return m_directionInvertedFromDevice; }
#if PLATFORM(COCOA)
    Phase phase() const { return static_cast<Phase>(m_phase); }
    Phase momentumPhase() const { return static_cast<Phase>(m_momentumPhase); }
    bool hasPreciseScrollingDeltas() const { return m_hasPreciseScrollingDeltas; }
    uint32_t scrollCount() const { return m_scrollCount; }
    const WebCore::FloatSize& unacceleratedScrollingDelta() const { return m_unacceleratedScrollingDelta; }
#endif

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, WebWheelEvent&);

private:
    static bool isWheelEventType(Type);

    WebCore::IntPoint m_position;
    WebCore::IntPoint m_globalPosition;
    WebCore::FloatSize m_delta;
    WebCore::FloatSize m_wheelTicks;
    uint32_t m_granularity; // Granularity
    bool m_directionInvertedFromDevice;
#if PLATFORM(COCOA)
    uint32_t m_phase; // Phase
    uint32_t m_momentumPhase; // Phase
    bool m_hasPreciseScrollingDeltas;
    uint32_t m_scrollCount;
    WebCore::FloatSize m_unacceleratedScrollingDelta;
#endif
};

// FIXME: Move this class to its own header file.
class WebKeyboardEvent : public WebEvent {
public:
    WebKeyboardEvent();
    ~WebKeyboardEvent();

#if USE(APPKIT)
    WebKeyboardEvent(Type, const String& text, const String& unmodifiedText, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool handledByInputMethod, const Vector<WebCore::KeypressCommand>&, bool isAutoRepeat, bool isKeypad, bool isSystemKey, Modifiers, double timestamp);
#elif PLATFORM(GTK)
    WebKeyboardEvent(Type, const String& text, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, bool handledByInputMethod, Vector<String>&& commands, bool isKeypad, Modifiers, double timestamp);
#else
    WebKeyboardEvent(Type, const String& text, const String& unmodifiedText, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool isAutoRepeat, bool isKeypad, bool isSystemKey, Modifiers, double timestamp);
#endif

    const String& text() const { return m_text; }
    const String& unmodifiedText() const { return m_unmodifiedText; }
    const String& keyIdentifier() const { return m_keyIdentifier; }
    int32_t windowsVirtualKeyCode() const { return m_windowsVirtualKeyCode; }
    int32_t nativeVirtualKeyCode() const { return m_nativeVirtualKeyCode; }
    int32_t macCharCode() const { return m_macCharCode; }
#if USE(APPKIT) || PLATFORM(GTK)
    bool handledByInputMethod() const { return m_handledByInputMethod; }
#endif
#if USE(APPKIT)
    const Vector<WebCore::KeypressCommand>& commands() const { return m_commands; }
#elif PLATFORM(GTK)
    const Vector<String>& commands() const { return m_commands; }
#endif
    bool isAutoRepeat() const { return m_isAutoRepeat; }
    bool isKeypad() const { return m_isKeypad; }
    bool isSystemKey() const { return m_isSystemKey; }

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, WebKeyboardEvent&);

    static bool isKeyboardEventType(Type);

private:
    String m_text;
    String m_unmodifiedText;
    String m_keyIdentifier;
    int32_t m_windowsVirtualKeyCode;
    int32_t m_nativeVirtualKeyCode;
    int32_t m_macCharCode;
#if USE(APPKIT) || PLATFORM(GTK)
    bool m_handledByInputMethod;
#endif
#if USE(APPKIT)
    Vector<WebCore::KeypressCommand> m_commands;
#elif PLATFORM(GTK)
    Vector<String> m_commands;
#endif
    bool m_isAutoRepeat;
    bool m_isKeypad;
    bool m_isSystemKey;
};

#if ENABLE(TOUCH_EVENTS)
#if PLATFORM(IOS)
class WebPlatformTouchPoint {
public:
    enum TouchPointState {
        TouchReleased,
        TouchPressed,
        TouchMoved,
        TouchStationary,
        TouchCancelled
    };

    WebPlatformTouchPoint() { }
    WebPlatformTouchPoint(unsigned identifier, WebCore::IntPoint location, TouchPointState phase)
        : m_identifier(identifier)
        , m_location(location)
        , m_phase(phase)
    {
    }

    unsigned identifier() const { return m_identifier; }
    WebCore::IntPoint location() const { return m_location; }
    TouchPointState phase() const { return static_cast<TouchPointState>(m_phase); }
    TouchPointState state() const { return phase(); }

#if ENABLE(IOS_TOUCH_EVENTS)
    void setForce(double force) { m_force = force; }
    double force() const { return m_force; }
#endif

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, WebPlatformTouchPoint&);

private:
    unsigned m_identifier;
    WebCore::IntPoint m_location;
    uint32_t m_phase;
#if ENABLE(IOS_TOUCH_EVENTS)
    double m_force { 0 };
#endif
};

class WebTouchEvent : public WebEvent {
public:
    WebTouchEvent() { }
    WebTouchEvent(WebEvent::Type type, Modifiers modifiers, double timestamp, const Vector<WebPlatformTouchPoint>& touchPoints, WebCore::IntPoint position, bool isPotentialTap, bool isGesture, float gestureScale, float gestureRotation)
        : WebEvent(type, modifiers, timestamp)
        , m_touchPoints(touchPoints)
        , m_position(position)
        , m_canPreventNativeGestures(true)
        , m_isPotentialTap(isPotentialTap)
        , m_isGesture(isGesture)
        , m_gestureScale(gestureScale)
        , m_gestureRotation(gestureRotation)
    {
        ASSERT(type == TouchStart || type == TouchMove || type == TouchEnd || type == TouchCancel);
    }

    const Vector<WebPlatformTouchPoint>& touchPoints() const { return m_touchPoints; }

    WebCore::IntPoint position() const { return m_position; }

    bool isPotentialTap() const { return m_isPotentialTap; }

    bool isGesture() const { return m_isGesture; }
    float gestureScale() const { return m_gestureScale; }
    float gestureRotation() const { return m_gestureRotation; }

    bool canPreventNativeGestures() const { return m_canPreventNativeGestures; }
    void setCanPreventNativeGestures(bool canPreventNativeGestures) { m_canPreventNativeGestures = canPreventNativeGestures; }

    bool allTouchPointsAreReleased() const;

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, WebTouchEvent&);
    
private:
    Vector<WebPlatformTouchPoint> m_touchPoints;
    
    WebCore::IntPoint m_position;
    bool m_canPreventNativeGestures;
    bool m_isPotentialTap;
    bool m_isGesture;
    float m_gestureScale;
    float m_gestureRotation;
};
#else
// FIXME: Move this class to its own header file.
// FIXME: Having "Platform" in the name makes it sound like this event is platform-specific or low-
// level in some way. That doesn't seem to be the case.
class WebPlatformTouchPoint {
public:
    enum TouchPointState {
        TouchReleased,
        TouchPressed,
        TouchMoved,
        TouchStationary,
        TouchCancelled
    };

    WebPlatformTouchPoint() : m_rotationAngle(0.0), m_force(0.0) { }

    WebPlatformTouchPoint(uint32_t id, TouchPointState, const WebCore::IntPoint& screenPosition, const WebCore::IntPoint& position);

    WebPlatformTouchPoint(uint32_t id, TouchPointState, const WebCore::IntPoint& screenPosition, const WebCore::IntPoint& position, const WebCore::IntSize& radius, float rotationAngle = 0.0, float force = 0.0);
    
    uint32_t id() const { return m_id; }
    TouchPointState state() const { return static_cast<TouchPointState>(m_state); }

    const WebCore::IntPoint& screenPosition() const { return m_screenPosition; }
    const WebCore::IntPoint& position() const { return m_position; }
    const WebCore::IntSize& radius() const { return m_radius; }
    float rotationAngle() const { return m_rotationAngle; }
    float force() const { return m_force; }

    void setState(TouchPointState state) { m_state = state; }

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, WebPlatformTouchPoint&);

private:
    uint32_t m_id;
    uint32_t m_state;
    WebCore::IntPoint m_screenPosition;
    WebCore::IntPoint m_position;
    WebCore::IntSize m_radius;
    float m_rotationAngle;
    float m_force;
};

// FIXME: Move this class to its own header file.
class WebTouchEvent : public WebEvent {
public:
    WebTouchEvent() { }
    WebTouchEvent(Type, Vector<WebPlatformTouchPoint>&&, Modifiers, double timestamp);

    const Vector<WebPlatformTouchPoint>& touchPoints() const { return m_touchPoints; }

    bool allTouchPointsAreReleased() const;

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, WebTouchEvent&);
  
private:
    static bool isTouchEventType(Type);

    Vector<WebPlatformTouchPoint> m_touchPoints;
};

#endif // PLATFORM(IOS)
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebKit

#endif // WebEvent_h
