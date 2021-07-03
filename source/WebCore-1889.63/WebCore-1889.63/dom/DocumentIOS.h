/*
 * Copyright (C) 2008, 2009, Apple Inc. All rights reserved.
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

#if ENABLE(TOUCH_EVENTS)
public:
    PassRefPtr<Touch> createTouch(DOMWindow* view, EventTarget* target, long identifier, long pageX, long pageY, long screenX, long screenY, ExceptionCode&);
    PassRefPtr<TouchList> createTouchList(ExceptionCode&);

    typedef HashCountedSet<Node*> TouchListenerMap;

    void setInTouchEventHandling(bool handling);

    void addTouchEventListener(Node*);
    void removeTouchEventListener(Node*, bool removeAll = false);
    void dirtyTouchEventRects();
    void clearTouchEventListeners();

    const TouchListenerMap& touchEventListeners() const;
    void getTouchRects(Vector<IntRect>&);
private:
    void setTouchEventListenersDirty(bool);
    IntRect eventRectRelativeToRoot(Node*, RenderObject*);
    void touchEventsChangedTimerFired(Timer<Document>*);
    void checkChildRenderers(RenderObject*, const IntRect& containingRect, Vector<IntRect>& nodeRects);
    void removeTouchEventListenersInDocument(Document*);
    typedef HashMap< RefPtr<Node>, Vector<IntRect> > TouchEventRectMap;

    bool m_handlingTouchEvent;
    bool m_touchEventRegionsDirty;
    TouchListenerMap m_touchEventListenersCounts;
    TouchEventRectMap m_touchEventRects;
    Mutex m_touchEventsRectMutex;
    Timer<Document> m_touchEventsChangedTimer;
public:
#endif
