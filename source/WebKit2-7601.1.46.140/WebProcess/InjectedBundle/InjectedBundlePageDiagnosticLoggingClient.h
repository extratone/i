/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef InjectedBundlePageDiagnosticLoggingClient_h
#define InjectedBundlePageDiagnosticLoggingClient_h

#include "APIClient.h"
#include "WKBundlePage.h"
#include <JavaScriptCore/JSBase.h>
#include <WebCore/DiagnosticLoggingResultType.h>
#include <wtf/Forward.h>

namespace API {
template<> struct ClientTraits<WKBundlePageDiagnosticLoggingClientBase> {
    typedef std::tuple<WKBundlePageDiagnosticLoggingClientV0, WKBundlePageDiagnosticLoggingClientV1> Versions;
};
}

namespace WebKit {

class InjectedBundleHitTestResult;
class WebContextMenuItemData;
class WebPage;

class InjectedBundlePageDiagnosticLoggingClient : public API::Client<WKBundlePageDiagnosticLoggingClientBase> {
public:
    void logDiagnosticMessageDeprecated(WebPage*, const String& message, const String& description, const String& success);
    void logDiagnosticMessage(WebPage*, const String& message, const String& description);
    void logDiagnosticMessageWithResult(WebPage*, const String& message, const String& description, WebCore::DiagnosticLoggingResultType);
    void logDiagnosticMessageWithValue(WebPage*, const String& message, const String& description, const String& value);
};

} // namespace WebKit

#endif // InjectedBundlePageDiagnosticLoggingClient_h
