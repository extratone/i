/*
 * Copyright (C) 2006, 2007, 2008 Apple, Inc.  All rights reserved.
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
#import "ResourceRequest.h"
#import "WebCoreSystemInterface.h"

#import "FormDataStreamMac.h"

#import <Foundation/Foundation.h>

#ifdef BUILDING_ON_TIGER
typedef unsigned NSUInteger;
#endif

@interface NSURLRequest (WebCoreContentDispositionEncoding)
- (NSArray *)contentDispositionEncodingFallbackArray;
@end

@interface NSMutableURLRequest (WebCoreContentDispositionEncoding)
- (void)setContentDispositionEncodingFallbackArray:(NSArray *)theEncodingFallbackArray;
@end

namespace WebCore {

NSURLRequest* ResourceRequest::nsURLRequest() const
{ 
    updatePlatformRequest();
    
    return [[m_nsRequest.get() retain] autorelease]; 
}

void ResourceRequest::doUpdateResourceRequest()
{
    m_url = [m_nsRequest.get() URL];
    m_cachePolicy = (ResourceRequestCachePolicy)[m_nsRequest.get() cachePolicy];
    m_timeoutInterval = [m_nsRequest.get() timeoutInterval];
    m_mainDocumentURL = [m_nsRequest.get() mainDocumentURL];
    
    if (NSString* method = [m_nsRequest.get() HTTPMethod])
        m_httpMethod = method;
    m_allowHTTPCookies = [m_nsRequest.get() HTTPShouldHandleCookies];
    
    NSDictionary *headers = [m_nsRequest.get() allHTTPHeaderFields];
    NSEnumerator *e = [headers keyEnumerator];
    NSString *name;
    while ((name = [e nextObject]))
        m_httpHeaderFields.set(name, [headers objectForKey:name]);

    // The below check can be removed once we require a version of Foundation with -[NSURLRequest contentDispositionEncodingFallbackArray] method.
    static bool supportsContentDispositionEncodingFallbackArray = [NSURLRequest instancesRespondToSelector:@selector(contentDispositionEncodingFallbackArray)];
    if (supportsContentDispositionEncodingFallbackArray) {
        m_responseContentDispositionEncodingFallbackArray.clear();
        NSArray *encodingFallbacks = [m_nsRequest.get() contentDispositionEncodingFallbackArray];
        NSUInteger count = [encodingFallbacks count];
        for (NSUInteger i = 0; i < count; ++i) {
            CFStringEncoding encoding = CFStringConvertNSStringEncodingToEncoding([(NSNumber *)[encodingFallbacks objectAtIndex:i] unsignedLongValue]);
            if (encoding != kCFStringEncodingInvalidId)
                m_responseContentDispositionEncodingFallbackArray.append(CFStringGetNameOfEncoding(encoding));
        }
    }

    if (NSData* bodyData = [m_nsRequest.get() HTTPBody])
        m_httpBody = FormData::create([bodyData bytes], [bodyData length]);
    else if (NSInputStream* bodyStream = [m_nsRequest.get() HTTPBodyStream])
        if (FormData* formData = httpBodyFromStream(bodyStream))
            m_httpBody = formData;
}

void ResourceRequest::doUpdatePlatformRequest()
{
    if (isNull()) {
        m_nsRequest = nil;
        return;
    }
    
    NSMutableURLRequest* nsRequest = [m_nsRequest.get() mutableCopy];

    if (nsRequest)
        [nsRequest setURL:url()];
    else
        nsRequest = [[NSMutableURLRequest alloc] initWithURL:url()];

#ifdef BUILDING_ON_TIGER
    wkSupportsMultipartXMixedReplace(nsRequest);
#endif

    [nsRequest setCachePolicy:(NSURLRequestCachePolicy)cachePolicy()];
    if (timeoutInterval() != unspecifiedTimeoutInterval)
        [nsRequest setTimeoutInterval:timeoutInterval()];
    [nsRequest setMainDocumentURL:mainDocumentURL()];
    if (!httpMethod().isEmpty())
        [nsRequest setHTTPMethod:httpMethod()];
    [nsRequest setHTTPShouldHandleCookies:allowHTTPCookies()];
    
    HTTPHeaderMap::const_iterator end = httpHeaderFields().end();
    for (HTTPHeaderMap::const_iterator it = httpHeaderFields().begin(); it != end; ++it)
        [nsRequest setValue:it->second forHTTPHeaderField:it->first];

    // The below check can be removed once we require a version of Foundation with -[NSMutableURLRequest setContentDispositionEncodingFallbackArray] method.
    static bool supportsContentDispositionEncodingFallbackArray = [NSMutableURLRequest instancesRespondToSelector:@selector(setContentDispositionEncodingFallbackArray)];
    if (supportsContentDispositionEncodingFallbackArray) {
        NSMutableArray *encodingFallbacks = [NSMutableArray array];
        unsigned count = m_responseContentDispositionEncodingFallbackArray.size();
        for (unsigned i = 0; i != count; ++i) {
            CFStringRef encodingName = m_responseContentDispositionEncodingFallbackArray[i].createCFString();
            unsigned long nsEncoding = CFStringConvertEncodingToNSStringEncoding(CFStringConvertIANACharSetNameToEncoding(encodingName));
            CFRelease(encodingName);
            if (nsEncoding != kCFStringEncodingInvalidId)
                [encodingFallbacks addObject:[NSNumber numberWithUnsignedLong:nsEncoding]];
        }
        [nsRequest setContentDispositionEncodingFallbackArray:encodingFallbacks];
    }

    RefPtr<FormData> formData = httpBody();
    if (formData && !formData->isEmpty())
        WebCore::setHTTPBody(nsRequest, formData);
    
    m_nsRequest.adoptNS(nsRequest);
}

void ResourceRequest::applyWebArchiveHackForMail()
{
    // Hack because Mail checks for this property to detect data / archive loads
    [NSURLProtocol setProperty:@"" forKey:@"WebDataRequest" inRequest:(NSMutableURLRequest *)nsURLRequest()];
}
    
unsigned initializeMaximumHTTPConnectionCountPerHost()
{
    static const unsigned preferredConnectionCount = 6;
    return wkInitializeMaximumHTTPConnectionCountPerHost(preferredConnectionCount);
}

}
