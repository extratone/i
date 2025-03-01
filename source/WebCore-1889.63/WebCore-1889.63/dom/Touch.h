/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
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

#ifndef Touch_h
#define Touch_h

#include <wtf/Platform.h>

#if ENABLE(TOUCH_EVENTS)

#include "DOMWindow.h"
#include "EventTarget.h"
#include "LayoutPoint.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DOMWindow;

class Touch : public RefCounted<Touch> {
public:
    static PassRefPtr<Touch> create()
    {
        return adoptRef(new Touch());
    }
    static PassRefPtr<Touch> create(DOMWindow* view, EventTarget* target, unsigned identifier, int pageX, int pageY, int screenX, int screenY)
    {
        return adoptRef(new Touch(view, target, identifier, pageX, pageY, screenX, screenY));
    }

    EventTarget* target() const { return m_target.get(); }

    bool updateLocation(int pageX, int pageY, int screenX, int screenY);
    
    unsigned identifier() const { return m_identifier; }
    
    int clientX() const { return m_clientX; }
    int clientY() const { return m_clientY; }
    int pageX() const { return m_pageX; }
    int pageY() const { return m_pageY; }
    int screenX() const { return m_screenX; }
    int screenY() const { return m_screenY; }

#if !PLATFORM(IOS)
    const LayoutPoint& absoluteLocation() const { return m_absoluteLocation; }
#endif

private:
    Touch() { }
    Touch(DOMWindow* view, EventTarget* target, unsigned identifier, int pageX, int pageY, int screenX, int screenY);

    DOMWindow* view() const { return m_view.get(); }

    RefPtr<DOMWindow> m_view;
    RefPtr<EventTarget> m_target;

    unsigned m_identifier;
    int m_clientX;
    int m_clientY;
    int m_pageX;
    int m_pageY;
    int m_screenX;
    int m_screenY;
#if !PLATFORM(IOS)
    LayoutPoint m_absoluteLocation;
#endif
};

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)

#endif /* Touch_h */
