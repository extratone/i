/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 */

#import "config.h"
#import "GeolocationServiceCoreLocation.h"

#import "Geoposition.h"
#import "PositionError.h"
#import "PositionOptions.h"
#import "SoftLinking.h"
#import "WebCoreThread.h"
#import <CoreLocation/CoreLocation.h>
#import <CoreLocation/CoreLocationPriv.h>
#import <objc/objc-runtime.h>
#import <wtf/RefPtr.h>
#import <wtf/UnusedParam.h>

SOFT_LINK_FRAMEWORK(CoreLocation)

SOFT_LINK_CLASS(CoreLocation, CLLocationManager)
SOFT_LINK_CLASS(CoreLocation, CLLocation)

SOFT_LINK_CONSTANT(CoreLocation, kCLLocationAccuracyBest, double)
SOFT_LINK_CONSTANT(CoreLocation, kCLLocationAccuracyHundredMeters, double)

#define kCLLocationAccuracyBest getkCLLocationAccuracyBest()
#define kCLLocationAccuracyHundredMeters getkCLLocationAccuracyHundredMeters()

using namespace WebCore;

@interface GeoLocationManager : NSObject<CLLocationManagerDelegate>
{
    CLLocationAccuracy m_accuracy;
    GeolocationServiceCoreLocation* m_callback;
    CLLocationManager *m_locationMgr;
}

- (id)initWithAccuracy:(CLLocationAccuracy)accuracy withCallback:(GeolocationServiceCoreLocation *)callback;
- (void)createOnMainThread;
- (void)start;
- (void)stop;
- (void)suspend;
- (void)resume;

- (void)sendLocation:(CLLocation *)newLocation;
- (void)sendError:(NSNumber *)code withString:(NSString *)string;

- (void)locationManager:(CLLocationManager *)manager didUpdateToLocation:(CLLocation *)newLocation fromLocation:(CLLocation *)oldLocation;
- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error;
@end

namespace WebCore {

GeolocationService* GeolocationService::create(GeolocationServiceClient* client)
{
    return new GeolocationServiceCoreLocation(client);
}

GeolocationServiceCoreLocation::GeolocationServiceCoreLocation(GeolocationServiceClient* client)
    : GeolocationService(client)
    , m_shouldClearCache(false)
{
}

GeolocationServiceCoreLocation::~GeolocationServiceCoreLocation()
{
    [m_locationManager.get() stop];
}

bool GeolocationServiceCoreLocation::startUpdating(PositionOptions* options)
{
    // CLLocationAccuracy values suggested by Ron Huang.
    CLLocationAccuracy accuracy = kCLLocationAccuracyBest;
    if (options && !options->enableHighAccuracy())
        accuracy = kCLLocationAccuracyHundredMeters;

    if (!m_locationManager.get())
        m_locationManager.adoptNS([[GeoLocationManager alloc] initWithAccuracy:accuracy withCallback:this]);
    else
        [m_locationManager.get() start];

    return true;
}

void GeolocationServiceCoreLocation::stopUpdating()
{
    [m_locationManager.get() stop];
}

void GeolocationServiceCoreLocation::suspend()
{
    [m_locationManager.get() suspend];
}

void GeolocationServiceCoreLocation::resume()
{
    [m_locationManager.get() resume];
}

void GeolocationServiceCoreLocation::positionChanged(PassRefPtr<Geoposition> position)
{
    m_lastPosition = position;
    GeolocationService::positionChanged();
}
    
void GeolocationServiceCoreLocation::errorOccurred(PassRefPtr<PositionError> error)
{
    m_lastError = error;
    GeolocationService::errorOccurred();
}

} // namespace WebCore

@implementation GeoLocationManager

- (id)initWithAccuracy:(CLLocationAccuracy)accuracy withCallback:(GeolocationServiceCoreLocation *)callback
{
    self = [super init];
    if (self) {
        m_accuracy = accuracy;
        m_callback = callback;
        [self performSelectorOnMainThread:@selector(createOnMainThread) withObject:nil waitUntilDone:NO];
    }
    return self;
}

- (void)dealloc
{
    m_locationMgr.delegate = nil;
    [m_locationMgr release];
    [super dealloc];
}

- (void)createOnMainThread
{
    ASSERT(!WebThreadIsCurrent());

#define CLLocationManager getCLLocationManagerClass()
    m_locationMgr = [[CLLocationManager alloc] init];
#undef CLLocationManager

    m_locationMgr.desiredAccuracy = m_accuracy;
    m_locationMgr.delegate = self;
    
    [self start];
}

- (void)start
{
    if (WebThreadIsCurrent()) {
        [self performSelectorOnMainThread:_cmd withObject:nil waitUntilDone:NO];
        return;
    }

    ASSERT(m_locationMgr);
    if (m_locationMgr.locationServicesEnabled)
        [m_locationMgr startUpdatingLocation];
    else
        [self sendError:[NSNumber numberWithInt:PositionError::PERMISSION_DENIED] withString:@"Unable to Start"];
}

- (void)stop
{
    if (WebThreadIsCurrent()) {
        [self performSelectorOnMainThread:_cmd withObject:nil waitUntilDone:NO];
        return;
    }

    [m_locationMgr stopUpdatingLocation];
}

- (void)suspend
{
    [self stop];
}

- (void)resume
{
    if (WebThreadIsCurrent()) {
        [self performSelectorOnMainThread:_cmd withObject:nil waitUntilDone:NO];
        return;
    }

    [m_locationMgr startUpdatingLocation];
}

- (void)sendLocation:(CLLocation *)newLocation
{
    if (!WebThreadIsCurrent()) {
        NSInvocation *invocation = WebThreadCreateNSInvocation(self, _cmd);
        [invocation setArgument:&newLocation atIndex:2];
        WebThreadCallAPI(invocation);
        return;
    }
    
    // Normalize
    bool canProvideAltitude = true;
    bool canProvideAltitudeAccuracy = true;
    double altitude = newLocation.altitude;
    double altitudeAccuracy = newLocation.verticalAccuracy;
    if (altitudeAccuracy < 0.0) {
        canProvideAltitude = false;
        canProvideAltitudeAccuracy = false;
    }
    
    bool canProvideSpeed = true;
    double speed = newLocation.speed;
    if (speed < 0.0)
        canProvideSpeed = false;
    
    bool canProvideHeading = true;
    double heading = newLocation.course;
    if (heading < 0.0)
        canProvideHeading = false;
    
    WTF::RefPtr<WebCore::Coordinates> newCoordinates = WebCore::Coordinates::create(
                                                                                    newLocation.coordinate.latitude,
                                                                                    newLocation.coordinate.longitude,
                                                                                    canProvideAltitude,
                                                                                    altitude,
                                                                                    newLocation.horizontalAccuracy,
                                                                                    canProvideAltitudeAccuracy,
                                                                                    altitudeAccuracy,
                                                                                    canProvideHeading,
                                                                                    heading,
                                                                                    canProvideSpeed,
                                                                                    speed);
    WTF::RefPtr<WebCore::Geoposition> newPosition = WebCore::Geoposition::create(
                                                                                 newCoordinates.release(),
                                                                                 [newLocation.timestamp timeIntervalSince1970] * 1000.0); // seconds -> milliseconds
    
    // FIXME: Need to figure out best way to clear web cache (<rdar://problem/6845619>)
    if (m_callback->shouldClearCache()) {
        m_callback->setShouldClearCache(YES);
        m_callback->cachePolicyChanged();
    }
    
    m_callback->positionChanged(newPosition.release());
}

- (void)sendError:(NSNumber *)code withString:(NSString *)string;
{
    if (!WebThreadIsCurrent()) {
        NSInvocation *invocation = WebThreadCreateNSInvocation(self, _cmd);
        [invocation setArgument:&code atIndex:2];
        [invocation setArgument:&string atIndex:3];
        WebThreadCallAPI(invocation);
        return;
    }
    
    m_callback->errorOccurred(PositionError::create(static_cast<PositionError::ErrorCode>([code intValue]), string));
}

- (void)locationManager:(CLLocationManager *)manager didUpdateToLocation:(CLLocation *)newLocation fromLocation:(CLLocation *)oldLocation
{
    ASSERT(m_callback);
    ASSERT(newLocation);
    UNUSED_PARAM(manager);
    UNUSED_PARAM(oldLocation);
    
    [self sendLocation:newLocation];
}

- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error
{
    ASSERT(m_callback);
    ASSERT(error);
    UNUSED_PARAM(manager);
    
    PositionError::ErrorCode code;
    switch ([error code]) {
        case kCLErrorDenied:
            code = PositionError::PERMISSION_DENIED;
            break;
        case kCLErrorLocationUnknown:
            code = PositionError::POSITION_UNAVAILABLE;
            break;
        default:
            code = PositionError::POSITION_UNAVAILABLE;
            break;
    }
    
    [self sendError:[NSNumber numberWithInt:code] withString:[error localizedDescription]];
}

@end
