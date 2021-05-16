/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 */

#ifndef GeolocationServiceCoreLocation_h
#define GeolocationServiceCoreLocation_h

#include "GeolocationService.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
@class GeoLocationManager;
#else
class GeoLocationManager;
#endif

namespace WebCore {

class GeolocationServiceCoreLocation : public GeolocationService {
public:
    GeolocationServiceCoreLocation(GeolocationServiceClient*);
    virtual ~GeolocationServiceCoreLocation();
    
    virtual bool startUpdating(PositionOptions*);
    virtual void stopUpdating();

    virtual void suspend();
    virtual void resume();

    virtual Geoposition* lastPosition() const { return m_lastPosition.get(); }
    virtual PositionError* lastError() const { return m_lastError.get(); }

    virtual void setShouldClearCache(bool shouldClearCache) { m_shouldClearCache = shouldClearCache; }
    virtual bool shouldClearCache() const { return m_shouldClearCache; }

    void positionChanged(PassRefPtr<Geoposition>);
    void errorOccurred(PassRefPtr<PositionError>);

private:
    RetainPtr<GeoLocationManager> m_locationManager;
    
    RefPtr<Geoposition> m_lastPosition;
    RefPtr<PositionError> m_lastError;
    bool m_shouldClearCache;
};
    
} // namespace WebCore

#endif // GeolocationServiceCoreLocation_h
