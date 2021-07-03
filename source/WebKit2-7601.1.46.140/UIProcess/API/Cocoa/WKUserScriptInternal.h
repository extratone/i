/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WKUserScript.h"

#if WK_API_ENABLED

#import "APIUserScript.h"
#import <wtf/RetainPtr.h>

namespace API {

inline WKUserScript *wrapper(UserScript& userScript)
{
    ASSERT([userScript.wrapper() isKindOfClass:[WKUserScript class]]);
    return (WKUserScript *)userScript.wrapper();
}

inline WebCore::UserScriptInjectionTime toWebCoreUserScriptInjectionTime(WKUserScriptInjectionTime injectionTime)
{
    switch (injectionTime) {
    case WKUserScriptInjectionTimeAtDocumentStart:
        return WebCore::InjectAtDocumentStart;

    case WKUserScriptInjectionTimeAtDocumentEnd:
        return WebCore::InjectAtDocumentEnd;
    }

    ASSERT_NOT_REACHED();
    return WebCore::InjectAtDocumentEnd;
}

inline WKUserScriptInjectionTime toWKUserScriptInjectionTime(WebCore::UserScriptInjectionTime injectionTime)
{
    switch (injectionTime) {
    case WebCore::InjectAtDocumentStart:
        return WKUserScriptInjectionTimeAtDocumentStart;

    case WebCore::InjectAtDocumentEnd:
        return WKUserScriptInjectionTimeAtDocumentEnd;
    }

    ASSERT_NOT_REACHED();
    return WKUserScriptInjectionTimeAtDocumentEnd;
}

}

@interface WKUserScript () <WKObject> {
@package
    API::ObjectStorage<API::UserScript> _userScript;
}
@end

#endif
