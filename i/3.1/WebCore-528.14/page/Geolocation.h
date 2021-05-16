/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#ifndef Geolocation_h
#define Geolocation_h

#include "GeolocationService.h"
#include "PositionCallback.h"
#include "PositionErrorCallback.h"
#include "PositionOptions.h"
#include "Timer.h"
#include <wtf/Platform.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Frame;
class Geoposition;

class Geolocation : public RefCounted<Geolocation>, public GeolocationServiceClient {
public:
    static PassRefPtr<Geolocation> create(Frame* frame) { return adoptRef(new Geolocation(frame)); }

    virtual ~Geolocation() {}

    void disconnectFrame();
    
    Geoposition* lastPosition() const { return m_service->lastPosition(); }

    void getCurrentPosition(PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);
    int watchPosition(PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);
    void clearWatch(int watchId);

    void suspend();
    void resume();
    
    void setIsAllowed(bool);
    bool isAllowed() const { return m_allowGeolocation == Yes; }

    void setShouldClearCache(bool shouldClearCache) { m_shouldClearCache = shouldClearCache; }
    bool shouldClearCache() const { return m_shouldClearCache; }
    
private:
    Geolocation(Frame*);

    class GeoNotifier : public RefCounted<GeoNotifier> {
    public:
        static PassRefPtr<GeoNotifier> create(PassRefPtr<PositionCallback> positionCallback, PassRefPtr<PositionErrorCallback> positionErrorCallback, PassRefPtr<PositionOptions> options) { return adoptRef(new GeoNotifier(positionCallback, positionErrorCallback, options)); }
        
        void startTimer();
        void timerFired(Timer<GeoNotifier>*);
        
        RefPtr<PositionCallback> m_successCallback;
        RefPtr<PositionErrorCallback> m_errorCallback;
        RefPtr<PositionOptions> m_options;
        Timer<GeoNotifier> m_timer;

    private:
        GeoNotifier(PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);
    };

    bool hasListeners() const { return !m_oneShots.isEmpty() || !m_watchers.isEmpty(); }

    void sendError(Vector<RefPtr<GeoNotifier> >&, PositionError*);
    void sendErrorToOneShots(PositionError*);
    void sendErrorToWatchers(PositionError*);
    
    void sendPosition(Vector<RefPtr<GeoNotifier> >&, Geoposition*);
    void sendPositionToOneShots(Geoposition*);
    void sendPositionToWatchers(Geoposition*);
    
    static void startTimer(Vector<RefPtr<GeoNotifier> >&);
    void startTimersForOneShots();
    void startTimersForWatchers();
    void startTimers();
    
    void handleError(PositionError*);
    
    void requestPermission();
    
    virtual void geolocationServicePositionChanged(GeolocationService*);
    virtual void geolocationServiceErrorOccurred(GeolocationService*);
    virtual void geolocationServiceCachePolicyChanged(GeolocationService*);
    
    typedef HashSet<RefPtr<GeoNotifier> > GeoNotifierSet;
    typedef HashMap<int, RefPtr<GeoNotifier> > GeoNotifierMap;
    
    GeoNotifierSet m_oneShots;
    GeoNotifierMap m_watchers;
    Frame* m_frame;
    OwnPtr<GeolocationService> m_service;

    enum {
        Unknown,
        Yes,
        No
    } m_allowGeolocation;
    bool m_shouldClearCache;
};
    
} // namespace WebCore

#endif // Geolocation_h
