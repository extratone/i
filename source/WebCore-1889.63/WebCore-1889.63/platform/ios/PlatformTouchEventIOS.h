/*
 * Copyright (C) 2008, 2012 Apple Inc. All rights reserved.
 *
 * Permission is granted by Apple to use this file to the extent
 * necessary to relink with LGPL WebKit files.
 *
 * No license or rights are granted by Apple expressly or by
 * implication, estoppel, or otherwise, to Apple patents and
 * trademarks. For the sake of clarity, no license or rights are
 * granted by Apple expressly or by implication, estoppel, or otherwise,
 * under any Apple patents, copyrights and trademarks to underlying
 * implementations of any application programming interfaces (APIs)
 * or to any functionality that is invoked by calling any API.
 */

#ifndef PlatformTouchEventIOS_h
#define PlatformTouchEventIOS_h

#include "PlatformEvent.h"

#include <wtf/CurrentTime.h>
#include <wtf/Platform.h>

#if ENABLE(TOUCH_EVENTS)

#include <wtf/Vector.h>
#include "IntPoint.h"
#include "PlatformTouchPointIOS.h"

namespace WebCore {

class PlatformTouchEvent : public PlatformEvent {
public:
    PlatformTouchEvent()
    {
    }

    PlatformTouchEvent(const PlatformTouchEvent& other)
        : PlatformEvent(other.type(), other.shiftKey(), other.ctrlKey(), other.altKey(), other.metaKey(), currentTime())
        , m_touchPoints(other.m_touchPoints)
        , m_gestureScale(other.m_gestureScale)
        , m_gestureRotation(other.m_gestureRotation)
        , m_isGesture(other.m_isGesture)
        , m_position(other.m_position)
        , m_globalPosition(other.m_globalPosition)
    {
    }

    unsigned touchCount() const { return m_touchPoints.size(); }
    IntPoint touchLocationAtIndex(unsigned i) const { return m_touchPoints[i].location(); }
    unsigned touchIdentifierAtIndex(unsigned i) const { return m_touchPoints[i].identifier(); }
    PlatformTouchPoint::TouchPhaseType touchPhaseAtIndex(unsigned i) const { return m_touchPoints[i].phase(); }

    bool isGesture() const { return m_isGesture; }

    float scale() const { return m_gestureScale; }
    float rotation() const { return m_gestureRotation; }

    const IntPoint& pos() const { return m_position; }
    int x() const { return m_position.x(); }
    int y() const { return m_position.y(); }
    int globalX() const { return m_globalPosition.x(); }
    int globalY() const { return m_globalPosition.y(); }

protected:
    Vector<PlatformTouchPoint> m_touchPoints;

    float m_gestureScale;
    float m_gestureRotation;
    bool m_isGesture;
    IntPoint m_position;
    IntPoint m_globalPosition;
};

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)

#endif // PlatformTouchEventIOS_h
