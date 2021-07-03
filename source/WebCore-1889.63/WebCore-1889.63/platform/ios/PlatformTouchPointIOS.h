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

#ifndef PlatformTouchPointIOS_h
#define PlatformTouchPointIOS_h

#include "IntPoint.h"

#if ENABLE(TOUCH_EVENTS)

namespace WebCore {

class PlatformTouchPoint
{
public:
    enum TouchPhaseType {
        TouchPhaseBegan,
        TouchPhaseMoved,
        TouchPhaseStationary,
        TouchPhaseEnded,
        TouchPhaseCancelled
    };

    unsigned identifier() const { return m_identifier; }
    IntPoint location() const { return m_location; }
    TouchPhaseType phase() const { return m_phase; }

protected:
    PlatformTouchPoint(unsigned identifier, const IntPoint& location, TouchPhaseType phase)
        : m_identifier(identifier)
        , m_location(location)
        , m_phase(phase)
    {
    }

private:
    unsigned m_identifier;
    IntPoint m_location;
    TouchPhaseType m_phase;
};

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)

#endif // PlatformTouchPointIOS_h
