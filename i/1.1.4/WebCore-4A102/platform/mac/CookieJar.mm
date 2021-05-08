/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "CookieJar.h"

#import "KURL.h"
#import "BlockExceptions.h"
#import "PlatformString.h"

namespace WebCore {

String cookies(const KURL& url)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSURL *URL = url.getNSURL();
    NSArray *cookiesForURL = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookiesForURL:URL];
    NSDictionary *header = [NSHTTPCookie requestHeaderFieldsWithCookies:cookiesForURL];
    return [header objectForKey:@"Cookie"];

    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

void setCookies(const KURL& url, const KURL& policyBaseURL, const String& cookieStr)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSURL *URL = url.getNSURL();
    
    // <http://bugzilla.opendarwin.org/show_bug.cgi?id=6531>, <rdar://4409034>
    // cookiesWithResponseHeaderFields doesn't parse cookies without a value
    String cookieString = cookieStr.contains('=') ? cookieStr : cookieStr + "=";
    
    NSArray *cookies = [NSHTTPCookie cookiesWithResponseHeaderFields:[NSDictionary dictionaryWithObject:cookieString forKey:@"Set-Cookie"] forURL:URL];
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookies:cookies forURL:URL mainDocumentURL:policyBaseURL.getNSURL()];    

    END_BLOCK_OBJC_EXCEPTIONS;
}

bool cookiesEnabled()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSHTTPCookieAcceptPolicy cookieAcceptPolicy = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];
    return cookieAcceptPolicy == NSHTTPCookieAcceptPolicyAlways || cookieAcceptPolicy == NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;

    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

}
