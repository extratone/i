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

#include "config.h"
#include "WebDiagnosticLoggingClient.h"

#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Settings.h>

namespace WebKit {

WebDiagnosticLoggingClient::WebDiagnosticLoggingClient(WebPage& page)
    : m_page(page)
{
}

WebDiagnosticLoggingClient::~WebDiagnosticLoggingClient()
{
}

void WebDiagnosticLoggingClient::logDiagnosticMessage(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    ASSERT(!m_page.corePage() || m_page.corePage()->settings().diagnosticLoggingEnabled());

    if (!shouldLogAfterSampling(shouldSample))
        return;

    // FIXME: Remove this injected bundle API.
    m_page.injectedBundleDiagnosticLoggingClient().logDiagnosticMessage(&m_page, message, description);
    m_page.send(Messages::WebPageProxy::LogSampledDiagnosticMessage(message, description));
}

void WebDiagnosticLoggingClient::logDiagnosticMessageWithResult(const String& message, const String& description, WebCore::DiagnosticLoggingResultType result, WebCore::ShouldSample shouldSample)
{
    ASSERT(!m_page.corePage() || m_page.corePage()->settings().diagnosticLoggingEnabled());

    if (!shouldLogAfterSampling(shouldSample))
        return;

    // FIXME: Remove this injected bundle API.
    m_page.injectedBundleDiagnosticLoggingClient().logDiagnosticMessageWithResult(&m_page, message, description, result);
    m_page.send(Messages::WebPageProxy::LogSampledDiagnosticMessageWithResult(message, description, result));
}

void WebDiagnosticLoggingClient::logDiagnosticMessageWithValue(const String& message, const String& description, const String& value, WebCore::ShouldSample shouldSample)
{
    ASSERT(!m_page.corePage() || m_page.corePage()->settings().diagnosticLoggingEnabled());

    if (!shouldLogAfterSampling(shouldSample))
        return;

    // FIXME: Remove this injected bundle API.
    m_page.injectedBundleDiagnosticLoggingClient().logDiagnosticMessageWithValue(&m_page, message, description, value);
    m_page.send(Messages::WebPageProxy::LogSampledDiagnosticMessageWithValue(message, description, value));
}

void WebDiagnosticLoggingClient::mainFrameDestroyed()
{
    delete this;
}

} // namespace WebKit
