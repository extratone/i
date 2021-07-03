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

#include "config.h"
#include "InjectedBundlePageDiagnosticLoggingClient.h"

#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebPage.h"

namespace WebKit {

void InjectedBundlePageDiagnosticLoggingClient::logDiagnosticMessageDeprecated(WebPage* page, const String& message, const String& description, const String& success)
{
    if (!m_client.logDiagnosticMessageDeprecated)
        return;
    m_client.logDiagnosticMessageDeprecated(toAPI(page), toAPI(message.impl()), toAPI(description.impl()), toAPI(success.impl()), m_client.base.clientInfo);
}

void InjectedBundlePageDiagnosticLoggingClient::logDiagnosticMessage(WebPage* page, const String& message, const String& description)
{
    if (!m_client.logDiagnosticMessage)
        return;
    m_client.logDiagnosticMessage(toAPI(page), toAPI(message.impl()), toAPI(description.impl()), m_client.base.clientInfo);
}

void InjectedBundlePageDiagnosticLoggingClient::logDiagnosticMessageWithResult(WebPage* page, const String& message, const String& description, WebCore::DiagnosticLoggingResultType result)
{
    if (!m_client.logDiagnosticMessageWithResult)
        return;
    m_client.logDiagnosticMessageWithResult(toAPI(page), toAPI(message.impl()), toAPI(description.impl()), toAPI(result), m_client.base.clientInfo);
}

void InjectedBundlePageDiagnosticLoggingClient::logDiagnosticMessageWithValue(WebPage* page, const String& message, const String& description, const String& value)
{
    if (!m_client.logDiagnosticMessageWithValue)
        return;
    m_client.logDiagnosticMessageWithValue(toAPI(page), toAPI(message.impl()), toAPI(description.impl()), toAPI(value.impl()), m_client.base.clientInfo);
}

}
