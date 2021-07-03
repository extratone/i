/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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
#include "WebInspector.h"

#include "WebFrame.h"
#include "WebInspectorMessages.h"
#include "WebInspectorProxyMessages.h"
#include "WebInspectorUIMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Chrome.h>
#include <WebCore/Document.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameView.h>
#include <WebCore/InspectorController.h>
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/InspectorPageAgent.h>
#include <WebCore/MainFrame.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/ScriptController.h>
#include <WebCore/WindowFeatures.h>

using namespace WebCore;

static const float minimumAttachedHeight = 250;
static const float maximumAttachedHeightRatio = 0.75;
static const float minimumAttachedWidth = 750;

namespace WebKit {

Ref<WebInspector> WebInspector::create(WebPage* page)
{
    return adoptRef(*new WebInspector(page));
}

WebInspector::WebInspector(WebPage* page)
    : m_page(page)
    , m_attached(false)
    , m_previousCanAttach(false)
#if ENABLE(INSPECTOR_SERVER)
    , m_remoteFrontendConnected(false)
#endif
{
}

WebInspector::~WebInspector()
{
    if (m_frontendConnection)
        m_frontendConnection->invalidate();
}

// Called from WebInspectorClient
void WebInspector::createInspectorPage(bool underTest)
{
#if OS(DARWIN)
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    IPC::Connection::Identifier connectionIdentifier(listeningPort);
    IPC::Attachment connectionClientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
#elif USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection();
    IPC::Connection::Identifier connectionIdentifier(socketPair.server);
    IPC::Attachment connectionClientPort(socketPair.client);
#else
    notImplemented();
    return;
#endif

    m_frontendConnection = IPC::Connection::createServerConnection(connectionIdentifier, *this);
    m_frontendConnection->open();

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::CreateInspectorPage(connectionClientPort, canAttachWindow(), underTest), m_page->pageID());
}

void WebInspector::closeFrontend()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::DidClose(), m_page->pageID());

    m_frontendConnection->invalidate();
    m_frontendConnection = nullptr;

    m_attached = false;
    m_previousCanAttach = false;
}

void WebInspector::bringToFront()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::BringToFront(), m_page->pageID());
}

// Called by WebInspector messages
void WebInspector::show()
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();
}

void WebInspector::close()
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().close();
}

void WebInspector::openInNewTab(const String& urlString)
{
    Page* inspectedPage = m_page->corePage();
    if (!inspectedPage)
        return;

    Frame& inspectedMainFrame = inspectedPage->mainFrame();
    FrameLoadRequest request(inspectedMainFrame.document()->securityOrigin(), ResourceRequest(urlString), "_blank", LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, ReplaceDocumentIfJavaScriptURL, ShouldOpenExternalURLsPolicy::ShouldNotAllow);

    Page* newPage = inspectedPage->chrome().createWindow(&inspectedMainFrame, request, WindowFeatures(), NavigationAction(request.resourceRequest(), NavigationType::LinkClicked));
    if (!newPage)
        return;

    newPage->mainFrame().loader().load(request);
}

void WebInspector::evaluateScriptForTest(const String& script)
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().evaluateForTestInFrontend(script);
}

void WebInspector::showConsole()
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();
    m_frontendConnection->send(Messages::WebInspectorUI::ShowConsole(), 0);
}

void WebInspector::showResources()
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();
    m_frontendConnection->send(Messages::WebInspectorUI::ShowResources(), 0);
}

void WebInspector::showMainResourceForFrame(uint64_t frameIdentifier)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameIdentifier);
    if (!frame)
        return;

    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();

    String inspectorFrameIdentifier = m_page->corePage()->inspectorController().pageAgent()->frameId(frame->coreFrame());
    m_frontendConnection->send(Messages::WebInspectorUI::ShowMainResourceForFrame(inspectorFrameIdentifier), 0);
}

void WebInspector::startPageProfiling()
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();
    m_frontendConnection->send(Messages::WebInspectorUI::StartPageProfiling(), 0);
}

void WebInspector::stopPageProfiling()
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();
    m_frontendConnection->send(Messages::WebInspectorUI::StopPageProfiling(), 0);
}

bool WebInspector::canAttachWindow()
{
    if (!m_page->corePage())
        return false;

    // Don't allow attaching to another inspector -- two inspectors in one window is too much!
    if (m_page->isInspectorPage())
        return false;

    // If we are already attached, allow attaching again to allow switching sides.
    if (m_attached)
        return true;

    // Don't allow the attach if the window would be too small to accommodate the minimum inspector size.
    unsigned inspectedPageHeight = m_page->corePage()->mainFrame().view()->visibleHeight();
    unsigned inspectedPageWidth = m_page->corePage()->mainFrame().view()->visibleWidth();
    unsigned maximumAttachedHeight = inspectedPageHeight * maximumAttachedHeightRatio;
    return minimumAttachedHeight <= maximumAttachedHeight && minimumAttachedWidth <= inspectedPageWidth;
}

void WebInspector::updateDockingAvailability()
{
    if (m_attached)
        return;

    bool canAttachWindow = this->canAttachWindow();
    if (m_previousCanAttach == canAttachWindow)
        return;

    m_previousCanAttach = canAttachWindow;

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::AttachAvailabilityChanged(canAttachWindow), m_page->pageID());
}

void WebInspector::sendMessageToBackend(const String& message)
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().dispatchMessageFromFrontend(message);
}

bool WebInspector::sendMessageToFrontend(const String& message)
{
#if ENABLE(INSPECTOR_SERVER)
    if (m_remoteFrontendConnected)
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SendMessageToRemoteFrontend(message), m_page->pageID());
    else
#endif
        m_frontendConnection->send(Messages::WebInspectorUI::SendMessageToFrontend(message), 0);
    return true;
}

#if ENABLE(INSPECTOR_SERVER)
void WebInspector::remoteFrontendConnected()
{
    if (m_page->corePage()) {
        m_remoteFrontendConnected = true;
        bool isAutomaticInspection = false;
        m_page->corePage()->inspectorController().connectFrontend(this, isAutomaticInspection);
    }
}

void WebInspector::remoteFrontendDisconnected()
{
    m_remoteFrontendConnected = false;

    if (m_page->corePage())
        m_page->corePage()->inspectorController().disconnectFrontend(Inspector::DisconnectReason::InspectorDestroyed);
}
#endif

} // namespace WebKit
