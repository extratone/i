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

#include "config.h"
#include "WebUserContentControllerProxy.h"

#include "APIArray.h"
#include "APIUserScript.h"
#include "DataReference.h"
#include "WebProcessProxy.h"
#include "WebScriptMessageHandler.h"
#include "WebUserContentControllerMessages.h"
#include "WebUserContentControllerProxyMessages.h"
#include <WebCore/SerializedScriptValue.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "APIUserContentExtension.h"
#include "WebCompiledContentExtension.h"
#endif

namespace WebKit {

static uint64_t generateIdentifier()
{
    static uint64_t identifier;

    return ++identifier;
}

WebUserContentControllerProxy::WebUserContentControllerProxy()
    : m_identifier(generateIdentifier())
    , m_userScripts(API::Array::create())
{
}

WebUserContentControllerProxy::~WebUserContentControllerProxy()
{
    for (WebProcessProxy* process : m_processes) {
        process->removeMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), m_identifier);
        process->didDestroyWebUserContentControllerProxy(*this);
    }
}

void WebUserContentControllerProxy::addProcess(WebProcessProxy& webProcessProxy)
{
    ASSERT(webProcessProxy.state() == WebProcessProxy::State::Running);

    if (!m_processes.add(&webProcessProxy).isNewEntry)
        return;

    webProcessProxy.addMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), m_identifier, *this);

    Vector<WebCore::UserScript> userScripts;
    for (const auto& userScript : m_userScripts->elementsOfType<API::UserScript>())
        userScripts.append(userScript->userScript());
    webProcessProxy.connection()->send(Messages::WebUserContentController::AddUserScripts(userScripts), m_identifier);

    webProcessProxy.connection()->send(Messages::WebUserContentController::AddUserStyleSheets(m_userStyleSheets), m_identifier);

    Vector<WebScriptMessageHandlerHandle> messageHandlerHandles;
    for (auto& handler : m_scriptMessageHandlers.values())
        messageHandlerHandles.append(handler->handle());
    webProcessProxy.connection()->send(Messages::WebUserContentController::AddUserScriptMessageHandlers(messageHandlerHandles), m_identifier);

#if ENABLE(CONTENT_EXTENSIONS)
    Vector<std::pair<String, WebCompiledContentExtensionData>> userContentExtensions;
    for (const auto& userContentExtension : m_userContentExtensions.values())
        userContentExtensions.append(std::make_pair(userContentExtension->name(), userContentExtension->compiledExtension().data()));
    webProcessProxy.connection()->send(Messages::WebUserContentController::AddUserContentExtensions(userContentExtensions), m_identifier);
#endif
}

void WebUserContentControllerProxy::removeProcess(WebProcessProxy& webProcessProxy)
{
    ASSERT(m_processes.contains(&webProcessProxy));

    m_processes.remove(&webProcessProxy);
    webProcessProxy.removeMessageReceiver(Messages::WebUserContentControllerProxy::messageReceiverName(), m_identifier);
}

void WebUserContentControllerProxy::addUserScript(API::UserScript& userScript)
{
    m_userScripts->elements().append(&userScript);

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::AddUserScripts({ userScript.userScript() }), m_identifier);
}

void WebUserContentControllerProxy::removeAllUserScripts()
{
    m_userScripts->elements().clear();

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveAllUserScripts(), m_identifier);
}

void WebUserContentControllerProxy::addUserStyleSheet(WebCore::UserStyleSheet userStyleSheet)
{
    m_userStyleSheets.append(WTF::move(userStyleSheet));

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::AddUserStyleSheets({ m_userStyleSheets.last() }), m_identifier);
}

void WebUserContentControllerProxy::removeAllUserStyleSheets()
{
    m_userStyleSheets.clear();

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveAllUserStyleSheets(), m_identifier);
}

bool WebUserContentControllerProxy::addUserScriptMessageHandler(WebScriptMessageHandler* handler)
{
    for (auto& existingHandler : m_scriptMessageHandlers.values()) {
        if (existingHandler->name() == handler->name())
            return false;
    }

    m_scriptMessageHandlers.add(handler->identifier(), handler);

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::AddUserScriptMessageHandlers({ handler->handle() }), m_identifier);
    
    return true;
}

void WebUserContentControllerProxy::removeUserMessageHandlerForName(const String& name)
{
    for (auto it = m_scriptMessageHandlers.begin(), end = m_scriptMessageHandlers.end(); it != end; ++it) {
        if (it->value->name() == name) {
            for (WebProcessProxy* process : m_processes)
                process->connection()->send(Messages::WebUserContentController::RemoveUserScriptMessageHandler(it->value->identifier()), m_identifier);
            m_scriptMessageHandlers.remove(it);
            return;
        }
    }
}

void WebUserContentControllerProxy::didPostMessage(IPC::Connection& connection, uint64_t pageID, uint64_t frameID, const SecurityOriginData& securityOrigin, uint64_t messageHandlerID, const IPC::DataReference& dataReference)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    if (!page)
        return;

    WebProcessProxy* webProcess = WebProcessProxy::fromConnection(&connection);
    WebFrameProxy* frame = webProcess->webFrame(frameID);
    if (!frame)
        return;

    if (!HashMap<uint64_t, RefPtr<WebScriptMessageHandler>>::isValidKey(messageHandlerID))
        return;

    RefPtr<WebScriptMessageHandler> handler = m_scriptMessageHandlers.get(messageHandlerID);
    if (!handler)
        return;

    auto buffer = dataReference.vector();
    RefPtr<WebCore::SerializedScriptValue> value = WebCore::SerializedScriptValue::adopt(buffer);

    handler->client().didPostMessage(*page, *frame, securityOrigin, *value);
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebUserContentControllerProxy::addUserContentExtension(API::UserContentExtension& userContentExtension)
{
    m_userContentExtensions.set(userContentExtension.name(), &userContentExtension);

    auto pair = std::make_pair(userContentExtension.name(), userContentExtension.compiledExtension().data());

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::AddUserContentExtensions({ pair }), m_identifier);
}

void WebUserContentControllerProxy::removeUserContentExtension(const String& name)
{
    m_userContentExtensions.remove(name);

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveUserContentExtension(name), m_identifier);
}

void WebUserContentControllerProxy::removeAllUserContentExtensions()
{
    m_userContentExtensions.clear();

    for (WebProcessProxy* process : m_processes)
        process->connection()->send(Messages::WebUserContentController::RemoveAllUserContentExtensions(), m_identifier);
}
#endif

} // namespace WebKit
