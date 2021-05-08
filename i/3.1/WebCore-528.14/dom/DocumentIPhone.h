/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 * No license or rights are granted by Apple expressly or by implication,
 * estoppel, or otherwise, to Apple copyrights, patents, trademarks, trade
 * secrets or other rights.
 */

#if ENABLE(TOUCH_EVENTS)
public:
    PassRefPtr<Touch> createTouch(DOMWindow* view, EventTarget* target, long identifier, long pageX, long pageY, long screenX, long screenY, ExceptionCode&);
    PassRefPtr<TouchList> createTouchList(ExceptionCode&);

    typedef HashMap< RefPtr<Node>, unsigned > TouchListenerMap;

    void setInTouchEventHandling(bool handling);

    void addTouchEventListener(Node*);
    void removeTouchEventListener(Node*, bool removeAll = false);
    void setTouchEventListenersDirty(bool);
    void touchEventsChangedTimerFired(Timer<Document>*);
    const TouchListenerMap& touchEventListeners() const { return m_touchEventListeners; }
private:
    bool m_inTouchEventHandling;
    bool m_touchEventRegionsDirty;
    TouchListenerMap m_touchEventListeners;
    Timer<Document> m_touchEventsChangedTimer;
public:
#endif
