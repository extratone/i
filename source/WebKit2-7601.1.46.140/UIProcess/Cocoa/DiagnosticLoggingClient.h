/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef DiagnosticLoggingClient_h
#define DiagnosticLoggingClient_h

#import "WKFoundation.h"

#if WK_API_ENABLED

#import "APIDiagnosticLoggingClient.h"
#import "WeakObjCPtr.h"
#import <WebCore/DiagnosticLoggingResultType.h>

@class WKWebView;
@protocol _WKDiagnosticLoggingDelegate;

namespace WebKit {

class DiagnosticLoggingClient final : public API::DiagnosticLoggingClient {
public:
    explicit DiagnosticLoggingClient(WKWebView *);

    RetainPtr<id <_WKDiagnosticLoggingDelegate>> delegate();
    void setDelegate(id <_WKDiagnosticLoggingDelegate>);

private:
    // From API::DiagnosticLoggingClient
    virtual void logDiagnosticMessage(WebPageProxy*, const String& message, const String& description) override;
    virtual void logDiagnosticMessageWithResult(WebPageProxy*, const String& message, const String& description, WebCore::DiagnosticLoggingResultType) override;
    virtual void logDiagnosticMessageWithValue(WebPageProxy*, const String& message, const String& description, const String& value) override;

    WKWebView *m_webView;
    WeakObjCPtr<id <_WKDiagnosticLoggingDelegate>> m_delegate;

    struct {
        unsigned webviewLogDiagnosticMessage : 1;
        unsigned webviewLogDiagnosticMessageWithResult : 1;
        unsigned webviewLogDiagnosticMessageWithValue : 1;
    } m_delegateMethods;
};

} // WebKit

#endif // WK_API_ENABLED

#endif // DiagnosticLoggingClient_h

