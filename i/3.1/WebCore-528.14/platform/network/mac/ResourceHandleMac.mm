/*
 * Copyright (C) 2004, 2006 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "ResourceHandleInternal.h"

#import "AuthenticationChallenge.h"
#import "AuthenticationMac.h"
#import "BlockExceptions.h"
#import "DocLoader.h"
#import "FormDataStreamMac.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "Page.h"
#import "ResourceError.h"
#import "ResourceResponse.h"
#import "SchedulePair.h"
#import "SharedBuffer.h"
#import "SubresourceLoader.h"
#import "WebCoreSystemInterface.h"
#import <wtf/UnusedParam.h>

#import "NSURLConnectionIPhone.h"
#import "Settings.h"
#import "SoftLinking.h"
#import "WebCoreURLResponseIPhone.h"
#import <MobileQuickLook/QLPreviewConverter.h>
#import <objc/objc-runtime.h>

#ifdef BUILDING_ON_TIGER
typedef int NSInteger;
#endif

SOFT_LINK_PRIVATE_FRAMEWORK(MobileQuickLook)

SOFT_LINK_CLASS(MobileQuickLook, QLPreviewConverter)
#define QLPreviewConverter getQLPreviewConverterClass()

SOFT_LINK(MobileQuickLook, QLPreviewGetSupportedMIMETypes, NSSet *, (), ())

using namespace WebCore;

@interface WebCoreResourceHandleAsDelegate : NSObject <NSURLAuthenticationChallengeSender>
{
    ResourceHandle* m_handle;
#ifndef BUILDING_ON_TIGER
    NSURL *m_url;
#endif
    id m_converter;
    BOOL m_finishedLoadingDataIntoConverter;
}
- (id)initWithHandle:(ResourceHandle*)handle;
- (void)detachHandle;
@end

@interface NSURLConnection (NSURLConnectionTigerPrivate)
- (NSData *)_bufferedData;
@end

#ifndef BUILDING_ON_TIGER

@interface WebCoreSynchronousLoader : NSObject {
    NSURL *m_url;
    NSURLResponse *m_response;
    NSMutableData *m_data;
    NSError *m_error;
    BOOL m_isDone;
}
+ (NSData *)loadRequest:(NSURLRequest *)request returningResponse:(NSURLResponse **)response error:(NSError **)error;
@end

static NSString *WebCoreSynchronousLoaderRunLoopMode = @"WebCoreSynchronousLoaderRunLoopMode";

#endif

namespace WebCore {

#ifdef BUILDING_ON_TIGER
static unsigned inNSURLConnectionCallback;
#endif

#ifndef NDEBUG
static bool isInitializingConnection;
#endif

static NSSet *QLPreviewGetSupportedMIMETypesSet()
{
    DEFINE_STATIC_LOCAL(RetainPtr<NSSet>, set, (QLPreviewGetSupportedMIMETypes()));
    return set.get();
}

static NSMutableDictionary *QLPreviewConverterDictionary()
{
    DEFINE_STATIC_LOCAL(RetainPtr<NSMutableDictionary>, dictionary, ([NSMutableDictionary dictionary]));
    return dictionary.get();
}

void addQLPreviewConverterForURL(NSURL *url, id converter)
{
    NSMutableDictionary *dictionary = QLPreviewConverterDictionary();
    @synchronized(dictionary) {
         [dictionary setObject:converter forKey:url];
    }
}

void removeQLPreviewConverterForURL(NSURL *url)
{
    NSMutableDictionary *dictionary = QLPreviewConverterDictionary();
    @synchronized(dictionary) {
        [dictionary removeObjectForKey:url];
    }
}

const KURL safeQLURLForDocumentURLAndResourceURL(const KURL& documentURL, const String& resourceURL)
{
    NSMutableDictionary *dictionary = QLPreviewConverterDictionary();
    id converter = nil;
    NSURL *nsDocumentURL = documentURL;
    @synchronized(dictionary) {
        converter = [dictionary objectForKey:nsDocumentURL];
    }

    if (!converter)
        return KURL(resourceURL);

    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[NSURL URLWithString:resourceURL]];
    NSURLRequest *safeRequest = [converter safeRequestForRequest:request];
    [request release];

    return [safeRequest URL];
}

class CallbackGuard {
public:
    CallbackGuard()
    {
#ifdef BUILDING_ON_TIGER
        ++inNSURLConnectionCallback;
#endif
    }
    ~CallbackGuard()
    {
#ifdef BUILDING_ON_TIGER
        ASSERT(inNSURLConnectionCallback > 0);
        --inNSURLConnectionCallback;
#endif
    }
};

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
    releaseDelegate();
}

static const double MaxFoundationVersionWithoutdidSendBodyDataDelegate = 677.21;
bool ResourceHandle::didSendBodyDataDelegateExists()
{
    return NSFoundationVersionNumber > MaxFoundationVersionWithoutdidSendBodyDataDelegate;
}

bool ResourceHandle::start(Frame* frame)
{
    if (!frame)
        return false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    Page* page = frame->page();
    if (!page)
        return false;

#ifndef NDEBUG
    isInitializingConnection = YES;
#endif

    id delegate;
    
    if (d->m_mightDownloadFromHandle) {
        ASSERT(!d->m_proxy);
        d->m_proxy = wkCreateNSURLConnectionDelegateProxy();
        [d->m_proxy.get() setDelegate:ResourceHandle::delegate()];
        [d->m_proxy.get() release];
        
        delegate = d->m_proxy.get();
    } else 
        delegate = ResourceHandle::delegate();

    if (!ResourceHandle::didSendBodyDataDelegateExists())
        associateStreamWithResourceHandle([d->m_request.nsURLRequest() HTTPBodyStream], this);

    NSURLConnection *connection;
    
    long long maxContentLength = 10485760; 
    if (frame->settings()) //webcore settings can be nil
        maxContentLength = frame->settings()->maximumResourceDataLength();

    if (d->m_shouldContentSniff) 
        connection = [[NSURLConnectionIPhone alloc] _initWithRequest:d->m_request.nsURLRequest() delegate:delegate usesCache:NO maxContentLength:maxContentLength];
    else {
        NSMutableURLRequest *request = [d->m_request.nsURLRequest() mutableCopy];
        wkSetNSURLRequestShouldContentSniff(request, NO);
        connection = [[NSURLConnectionIPhone alloc] _initWithRequest:request delegate:delegate usesCache:NO maxContentLength:maxContentLength];
        [request release];
    }

    if (WebThreadNotCurrent()) {
        NSInvocation *invocation = WebThreadCreateNSInvocation(connection, @selector(start));
        WebThreadCallAPI(invocation);
    } else
        [(NSURLConnectionIPhone *)connection start];

#ifndef NDEBUG
    isInitializingConnection = NO;
#endif
    
    d->m_connection = connection;

    if (d->m_connection) {
        [connection release];

        if (d->m_defersLoading)
            wkSetNSURLConnectionDefersCallbacks(d->m_connection.get(), YES);

        return true;
    }

    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

void ResourceHandle::cancel()
{
    if (!ResourceHandle::didSendBodyDataDelegateExists())
        disassociateStreamWithResourceHandle([d->m_request.nsURLRequest() HTTPBodyStream]);
    [d->m_connection.get() cancel];
}

void ResourceHandle::setDefersLoading(bool defers)
{
    d->m_defersLoading = defers;
    if (d->m_connection)
        wkSetNSURLConnectionDefersCallbacks(d->m_connection.get(), defers);
}

void ResourceHandle::schedule(SchedulePair* pair)
{
    UNUSED_PARAM(pair);
}

void ResourceHandle::unschedule(SchedulePair* pair)
{
    UNUSED_PARAM(pair);
}

WebCoreResourceHandleAsDelegate *ResourceHandle::delegate()
{
    if (!d->m_delegate) {
        WebCoreResourceHandleAsDelegate *delegate = [[WebCoreResourceHandleAsDelegate alloc] initWithHandle:this];
        d->m_delegate = delegate;
        [delegate release];
    }
    return d->m_delegate.get();
}

void ResourceHandle::releaseDelegate()
{
    if (!d->m_delegate)
        return;
    if (d->m_proxy)
        [d->m_proxy.get() setDelegate:nil];
    [d->m_delegate.get() detachHandle];
    d->m_delegate = nil;
}

bool ResourceHandle::supportsBufferedData()
{
    static bool supportsBufferedData = [NSURLConnection instancesRespondToSelector:@selector(_bufferedData)];
    return supportsBufferedData;
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    if (ResourceHandle::supportsBufferedData())
        return SharedBuffer::wrapNSData([d->m_connection.get() _bufferedData]);

    return 0;
}

id ResourceHandle::releaseProxy()
{
    id proxy = [[d->m_proxy.get() retain] autorelease];
    d->m_proxy = nil;
    [proxy setDelegate:nil];
    return proxy;
}

NSURLConnection *ResourceHandle::connection() const
{
    return d->m_connection.get();
}

bool ResourceHandle::loadsBlocked()
{
#ifndef BUILDING_ON_TIGER
    return false;
#else
    // On Tiger, if we're in an NSURLConnection callback, that blocks all other NSURLConnection callbacks.
    // On Leopard and newer, it blocks only callbacks on that same NSURLConnection object, which is not
    // a problem in practice.
    return inNSURLConnectionCallback != 0;
#endif
}

bool ResourceHandle::willLoadFromCache(ResourceRequest& request)
{
    request.setCachePolicy(ReturnCacheDataDontLoad);
    NSURLResponse *nsURLResponse = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
   [NSURLConnection sendSynchronousRequest:request.nsURLRequest() returningResponse:&nsURLResponse error:nil];
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return nsURLResponse;
}

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<char>& data, Frame*)
{
    NSError *nsError = nil;
    
    NSURLResponse *nsURLResponse = nil;
    NSData *result = nil;

    ASSERT(!request.isEmpty());
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
#ifndef BUILDING_ON_TIGER
    result = [WebCoreSynchronousLoader loadRequest:request.nsURLRequest() returningResponse:&nsURLResponse error:&nsError];
#else
    result = [NSURLConnection sendSynchronousRequest:request.nsURLRequest() returningResponse:&nsURLResponse error:&nsError];
#endif
    END_BLOCK_OBJC_EXCEPTIONS;

    if (nsError == nil)
        response = nsURLResponse;
    else {
        response = ResourceResponse(request.url(), String(), 0, String(), String());
        if ([nsError domain] == NSURLErrorDomain)
            switch ([nsError code]) {
                case NSURLErrorUserCancelledAuthentication:
                    // FIXME: we should really return the actual HTTP response, but sendSynchronousRequest doesn't provide us with one.
                    response.setHTTPStatusCode(401);
                    break;
                default:
                    response.setHTTPStatusCode([nsError code]);
            }
        else
            response.setHTTPStatusCode(404);
    }
    
    data.resize([result length]);
    memcpy(data.data(), [result bytes], [result length]);
    
    error = nsError;
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    if (client())
        return client()->shouldUseCredentialStorage(this);

    return false;
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    ASSERT(!d->m_currentMacChallenge);
    ASSERT(d->m_currentWebChallenge.isNull());
    // Since NSURLConnection networking relies on keeping a reference to the original NSURLAuthenticationChallenge,
    // we make sure that is actually present
    ASSERT(challenge.nsURLAuthenticationChallenge());
        
    d->m_currentMacChallenge = challenge.nsURLAuthenticationChallenge();
    NSURLAuthenticationChallenge *webChallenge = [[NSURLAuthenticationChallenge alloc] initWithAuthenticationChallenge:d->m_currentMacChallenge 
                                                                                       sender:(id<NSURLAuthenticationChallengeSender>)delegate()];
    d->m_currentWebChallenge = core(webChallenge);
    [webChallenge release];

    if (client())
        client()->didReceiveAuthenticationChallenge(this, d->m_currentWebChallenge);
}

void ResourceHandle::didCancelAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    ASSERT(d->m_currentMacChallenge);
    ASSERT(!d->m_currentWebChallenge.isNull());
    ASSERT(d->m_currentWebChallenge == challenge);

    if (client())
        client()->didCancelAuthenticationChallenge(this, challenge);
}

bool ResourceHandle::canAuthenticateAgainstProtectionSpace(const ProtectionSpace& protectionSpace)
{
    if (client())
        return client()->canAuthenticateAgainstProtectionSpace(this, protectionSpace);
        
    return false;
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    ASSERT(!challenge.isNull());
    if (challenge != d->m_currentWebChallenge)
        return;

    [[d->m_currentMacChallenge sender] useCredential:mac(credential) forAuthenticationChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    ASSERT(!challenge.isNull());
    if (challenge != d->m_currentWebChallenge)
        return;

    [[d->m_currentMacChallenge sender] continueWithoutCredentialForAuthenticationChallenge:d->m_currentMacChallenge];

    clearAuthentication();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    if (challenge != d->m_currentWebChallenge)
        return;

    if (client())
        client()->receivedCancellation(this, challenge);
}

} // namespace WebCore

@implementation WebCoreResourceHandleAsDelegate

- (id)initWithHandle:(ResourceHandle*)handle
{
    self = [self init];
    if (!self)
        return nil;
    m_handle = handle;
    m_converter = nil;
    m_finishedLoadingDataIntoConverter = NO;
    return self;
}

#ifndef BUILDING_ON_TIGER
- (void)dealloc
{
    [m_converter release];
    m_converter = nil;
    [m_url release];
    [super dealloc];
}
#endif

- (void)detachHandle
{
    m_handle = 0;
}

- (NSURLRequest *)connection:(NSURLConnection *)unusedConnection willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    UNUSED_PARAM(unusedConnection);

    // the willSendRequest call may cancel this load, in which case self could be deallocated
    RetainPtr<WebCoreResourceHandleAsDelegate> protect(self);

    if (!m_handle || !m_handle->client())
        return nil;
    
    // See <rdar://problem/5380697> .  This is a workaround for a behavior change in CFNetwork where willSendRequest gets called more often.
    if (!redirectResponse)
        return newRequest;
    
    CallbackGuard guard;
    ResourceRequest request = newRequest;
    m_handle->client()->willSendRequest(m_handle, request, redirectResponse);
#ifndef BUILDING_ON_TIGER
    NSURL *copy = [[request.nsURLRequest() URL] copy];
    [m_url release];
    m_url = copy;
#endif

    if (!ResourceHandle::didSendBodyDataDelegateExists()) {
        // The client may change the request's body stream, in which case we have to re-associate
        // the handle with the new stream so upload progress callbacks continue to work correctly.
        NSInputStream* oldBodyStream = [newRequest HTTPBodyStream];
        NSInputStream* newBodyStream = [request.nsURLRequest() HTTPBodyStream];
        if (oldBodyStream != newBodyStream) {
            disassociateStreamWithResourceHandle(oldBodyStream);
            associateStreamWithResourceHandle(newBodyStream, m_handle);
        }
    }

    return request.nsURLRequest();
}

- (BOOL)connectionShouldUseCredentialStorage:(NSURLConnection *)unusedConnection
{
    UNUSED_PARAM(unusedConnection);

    if (!m_handle)
        return NO;

    CallbackGuard guard;
    return m_handle->shouldUseCredentialStorage();
}

- (void)connection:(NSURLConnection *)unusedConnection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    UNUSED_PARAM(unusedConnection);

#ifndef BUILDING_ON_TIGER
    if ([challenge previousFailureCount] == 0) {
        NSString *user = [m_url user];
        NSString *password = [m_url password];

        if (user && password) {
            NSURLCredential *credential = [[NSURLCredential alloc] initWithUser:user
                                                                     password:password
                                                                  persistence:NSURLCredentialPersistenceForSession];
            [[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];
            [credential release];
            return;
        }
    }
#endif
    
    if (!m_handle)
        return;
    CallbackGuard guard;
    m_handle->didReceiveAuthenticationChallenge(core(challenge));
}

- (void)connection:(NSURLConnection *)unusedConnection didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    UNUSED_PARAM(unusedConnection);

    if (!m_handle)
        return;
    CallbackGuard guard;
    m_handle->didCancelAuthenticationChallenge(core(challenge));
}

- (BOOL)connection:(NSURLConnection *)unusedConnection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
{
    UNUSED_PARAM(unusedConnection);
    
    if (!m_handle)
        return NO;
        
    CallbackGuard guard;
    return m_handle->canAuthenticateAgainstProtectionSpace(core(protectionSpace));
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)r
{

    if (!m_handle || !m_handle->client())
        return;
    CallbackGuard guard;

    if (shouldUseQuickLookForMIMEType([r MIMEType])) {
        if ([QLPreviewGetSupportedMIMETypesSet() containsObject:[r _webcore_MIMEType]]) {
            m_converter = [[QLPreviewConverter alloc] initWithConnection:connection delegate:self response:r options:nil];
            r = [m_converter previewResponse];
            // We must use the generated URL from the new NSURLResponse object
            // so that it matches the URL removed from -[WebDataSource dealloc].
            addQLPreviewConverterForURL([r URL], m_converter);
        }
    }

    m_handle->client()->didReceiveResponse(m_handle, r);
}

- (void)connection:(NSURLConnection *)unusedConnection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    UNUSED_PARAM(unusedConnection);

    if ([data length] == 0) // <rdar://problem/5532931>
        return;

    if (!m_handle || !m_handle->client())
        return;
    // FIXME: If we get more than 2B bytes in a single chunk, this code won't do the right thing.
    // However, with today's computers and networking speeds, this won't happen in practice.
    // Could be an issue with a giant local file.
    CallbackGuard guard;

    if (m_converter && !m_finishedLoadingDataIntoConverter) {
        [m_converter appendData:data];
        return;
    }

    m_handle->client()->didReceiveData(m_handle, (const char*)[data bytes], [data length], static_cast<int>(lengthReceived));
}

- (void)connection:(NSURLConnection *)unusedConnection willStopBufferingData:(NSData *)data
{
    UNUSED_PARAM(unusedConnection);

    if (!m_handle || !m_handle->client())
        return;
    // FIXME: If we get a resource with more than 2B bytes, this code won't do the right thing.
    // However, with today's computers and networking speeds, this won't happen in practice.
    // Could be an issue with a giant local file.
    CallbackGuard guard;
    m_handle->client()->willStopBufferingData(m_handle, (const char*)[data bytes], static_cast<int>([data length]));
}

- (void)connection:(NSURLConnection *)unusedConnection didSendBodyData:(NSInteger)unusedBytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
    UNUSED_PARAM(unusedConnection);
    UNUSED_PARAM(unusedBytesWritten);

    if (!m_handle || !m_handle->client())
        return;
    CallbackGuard guard;
    m_handle->client()->didSendData(m_handle, totalBytesWritten, totalBytesExpectedToWrite);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)unusedConnection
{
    UNUSED_PARAM(unusedConnection);

    if (!m_handle || !m_handle->client())
        return;
    CallbackGuard guard;

    if (m_converter && !m_finishedLoadingDataIntoConverter) {
        m_finishedLoadingDataIntoConverter = YES;
        [m_converter finishedAppendingData];
        return;
    }

    if (!ResourceHandle::didSendBodyDataDelegateExists())
        disassociateStreamWithResourceHandle([m_handle->request().nsURLRequest() HTTPBodyStream]);

    m_handle->client()->didFinishLoading(m_handle);
}

- (void)connection:(NSURLConnection *)unusedConnection didFailWithError:(NSError *)error
{
    UNUSED_PARAM(unusedConnection);

    if (!m_handle || !m_handle->client())
        return;
    CallbackGuard guard;

    if (m_converter) {
        removeQLPreviewConverterForURL(m_url);
        [m_converter finishConverting];
        [m_converter release];
        m_converter = nil;
    }

    if (!ResourceHandle::didSendBodyDataDelegateExists())
        disassociateStreamWithResourceHandle([m_handle->request().nsURLRequest() HTTPBodyStream]);

    m_handle->client()->didFail(m_handle, error);
}

#ifdef BUILDING_ON_TIGER
- (void)_callConnectionWillCacheResponseWithInfo:(NSMutableDictionary *)info
{
    NSURLConnection *connection = [info objectForKey:@"connection"];
    NSCachedURLResponse *cachedResponse = [info objectForKey:@"cachedResponse"];
    NSCachedURLResponse *result = [self connection:connection willCacheResponse:cachedResponse];
    if (result)
        [info setObject:result forKey:@"result"];
}
#endif

- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
#ifdef BUILDING_ON_TIGER
    // On Tiger CFURLConnection can sometimes call the connection:willCacheResponse: delegate method on
    // a secondary thread instead of the main thread. If this happens perform the work on the main thread.
    if (!pthread_main_np()) {
        NSMutableDictionary *info = [[NSMutableDictionary alloc] init];
        if (connection)
            [info setObject:connection forKey:@"connection"];
        if (cachedResponse)
            [info setObject:cachedResponse forKey:@"cachedResponse"];

        // Include synchronous url connection's mode as an acceptable run loopmode
        // <rdar://problem/5511842>
        NSArray *modes = [[NSArray alloc] initWithObjects:(NSString *)kCFRunLoopCommonModes, @"NSSynchronousURLConnection_PrivateMode", nil];        
        [self performSelectorOnMainThread:@selector(_callConnectionWillCacheResponseWithInfo:) withObject:info waitUntilDone:YES modes:modes];
        [modes release];

        NSCachedURLResponse *result = [[info valueForKey:@"result"] retain];
        [info release];

        return [result autorelease];
    }
#else
    UNUSED_PARAM(connection);
#endif

#ifndef NDEBUG
    if (isInitializingConnection)
        LOG_ERROR("connection:willCacheResponse: was called inside of [NSURLConnection initWithRequest:delegate:] (4067625)");
#endif

    if (!m_handle || !m_handle->client())
        return nil;

    CallbackGuard guard;
    
    NSCachedURLResponse *newResponse = m_handle->client()->willCacheResponse(m_handle, cachedResponse);
    if (newResponse != cachedResponse)
        return newResponse;
    
    CacheStoragePolicy policy = static_cast<CacheStoragePolicy>([newResponse storagePolicy]);
        
    m_handle->client()->willCacheResponse(m_handle, policy);

    if (static_cast<NSURLCacheStoragePolicy>(policy) != [newResponse storagePolicy])
        newResponse = [[[NSCachedURLResponse alloc] initWithResponse:[newResponse response]
                                                                data:[newResponse data]
                                                            userInfo:[newResponse userInfo]
                                                       storagePolicy:static_cast<NSURLCacheStoragePolicy>(policy)] autorelease];

    return newResponse;
}

- (void)useCredential:(NSURLCredential *)credential forAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    m_handle->receivedCredential(core(challenge), core(credential));
}

- (void)continueWithoutCredentialForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    m_handle->receivedRequestToContinueWithoutCredential(core(challenge));
}

- (void)cancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    m_handle->receivedCancellation(core(challenge));
}

@end

#ifndef BUILDING_ON_TIGER

@implementation WebCoreSynchronousLoader

- (BOOL)_isDone
{
    return m_isDone;
}

- (void)dealloc
{
    [m_url release];
    [m_response release];
    [m_data release];
    [m_error release];
    
    [super dealloc];
}

- (NSURLRequest *)connection:(NSURLConnection *)unusedConnection willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)unusedRedirectResponse
{
    UNUSED_PARAM(unusedConnection);
    UNUSED_PARAM(unusedRedirectResponse);

    // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
    if (m_url && !protocolHostAndPortAreEqual(m_url, [newRequest URL])) {
        m_error = [[NSError alloc] initWithDomain:NSURLErrorDomain code:NSURLErrorBadServerResponse userInfo:nil];
        m_isDone = YES;
        return nil;
    }

    NSURL *copy = [[newRequest URL] copy];
    [m_url release];
    m_url = copy;

    return newRequest;
}

- (void)connection:(NSURLConnection *)unusedConnection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    UNUSED_PARAM(unusedConnection);

    if ([challenge previousFailureCount] == 0) {
        NSString *user = [m_url user];
        NSString *password = [m_url password];
        
        if (user && password) {
            NSURLCredential *credential = [[NSURLCredential alloc] initWithUser:user
                                                                     password:password
                                                                  persistence:NSURLCredentialPersistenceForSession];
            [[challenge sender] useCredential:credential forAuthenticationChallenge:challenge];
            [credential release];
            return;
        }
    }
    
    [[challenge sender] continueWithoutCredentialForAuthenticationChallenge:challenge];
}

- (void)connection:(NSURLConnection *)unusedConnection didReceiveResponse:(NSURLResponse *)response
{
    UNUSED_PARAM(unusedConnection);

    NSURLResponse *r = [response copy];
    
    [m_response release];
    m_response = r;
}

- (void)connection:(NSURLConnection *)unusedConnection didReceiveData:(NSData *)data
{
    UNUSED_PARAM(unusedConnection);

    if (!m_data)
        m_data = [[NSMutableData alloc] init];
    
    [m_data appendData:data];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)unusedConnection
{
    UNUSED_PARAM(unusedConnection);

    m_isDone = YES;
}

- (void)connection:(NSURLConnection *)unusedConnection didFailWithError:(NSError *)error
{
    UNUSED_PARAM(unusedConnection);

    ASSERT(!m_error);
    
    m_error = [error retain];
    m_isDone = YES;
}

- (NSData *)_data
{
    return [[m_data retain] autorelease];
}

- (NSURLResponse *)_response
{
    return [[m_response retain] autorelease];
}

- (NSError *)_error
{
    return [[m_error retain] autorelease];
}

+ (NSData *)loadRequest:(NSURLRequest *)request returningResponse:(NSURLResponse **)response error:(NSError **)error
{
    WebCoreSynchronousLoader *delegate = [[WebCoreSynchronousLoader alloc] init];
    
    NSURLConnection *connection = [[NSURLConnection alloc] initWithRequest:request delegate:delegate startImmediately:NO];
    [connection scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:WebCoreSynchronousLoaderRunLoopMode];
    [connection start];
    
    while (![delegate _isDone])
        [[NSRunLoop currentRunLoop] runMode:WebCoreSynchronousLoaderRunLoopMode beforeDate:[NSDate distantFuture]];

    NSData *data = [delegate _data];
    *response = [delegate _response];
    *error = [delegate _error];
    
    [connection cancel];
    
    [connection release];
    [delegate release];
    
    return data;
}

@end

#endif
