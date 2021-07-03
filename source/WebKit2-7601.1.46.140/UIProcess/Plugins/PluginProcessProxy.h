/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef PluginProcessProxy_h
#define PluginProcessProxy_h

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "ChildProcessProxy.h"
#include "Connection.h"
#include "PluginModuleInfo.h"
#include "PluginProcess.h"
#include "PluginProcessAttributes.h"
#include "ProcessLauncher.h"
#include "WebProcessProxyMessages.h"
#include <wtf/Deque.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSObject;
OBJC_CLASS WKPlaceholderModalWindow;
#endif

namespace WebKit {

class PluginProcessManager;
class WebProcessProxy;
struct PluginProcessCreationParameters;

#if PLUGIN_ARCHITECTURE(X11)
struct RawPluginMetaData {
    String name;
    String description;
    String mimeDescription;

#if PLATFORM(GTK)
    bool requiresGtk2;
#endif
};
#endif

#if PLATFORM(COCOA)
int pluginProcessLatencyQOS();
int pluginProcessThroughputQOS();
#endif

class PluginProcessProxy : public ChildProcessProxy {
public:
    static Ref<PluginProcessProxy> create(PluginProcessManager*, const PluginProcessAttributes&, uint64_t pluginProcessToken);
    ~PluginProcessProxy();

    const PluginProcessAttributes& pluginProcessAttributes() const { return m_pluginProcessAttributes; }
    uint64_t pluginProcessToken() const { return m_pluginProcessToken; }

    // Asks the plug-in process to create a new connection to a web process. The connection identifier will be
    // encoded in the given argument encoder and sent back to the connection of the given web process.
    void getPluginProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply>);

    void fetchWebsiteData(std::function<void (Vector<String>)> completionHandler);
    void deleteWebsiteData(std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler);
    void deleteWebsiteDataForHostNames(const Vector<String>& hostNames, std::function<void ()> completionHandler);

    bool isValid() const { return m_connection; }

#if PLATFORM(COCOA)
    void setProcessSuppressionEnabled(bool);

    // Returns whether the plug-in needs the heap to be marked executable.
    static bool pluginNeedsExecutableHeap(const PluginModuleInfo&);

#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 101000
    // Creates a property list in ~/Library/Preferences that contains all the MIME types supported by the plug-in.
    static bool createPropertyListFile(const PluginModuleInfo&);
#endif

#endif

#if PLUGIN_ARCHITECTURE(X11)
    static bool scanPlugin(const String& pluginPath, RawPluginMetaData& result);
#endif

private:
    PluginProcessProxy(PluginProcessManager*, const PluginProcessAttributes&, uint64_t pluginProcessToken);

    virtual void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void platformGetLaunchOptions(ProcessLauncher::LaunchOptions&, const PluginProcessAttributes&);
    virtual void processWillShutDown(IPC::Connection&) override;

    void pluginProcessCrashedOrFailedToLaunch();

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;

    virtual void didClose(IPC::Connection&) override;
    virtual void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;
    virtual IPC::ProcessType localProcessType() override { return IPC::ProcessType::UI; }
    virtual IPC::ProcessType remoteProcessType() override { return IPC::ProcessType::Plugin; }

    // ProcessLauncher::Client
    virtual void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    // Message handlers
    void didCreateWebProcessConnection(const IPC::Attachment&, bool supportsAsynchronousPluginInitialization);
    void didGetSitesWithData(const Vector<String>& sites, uint64_t callbackID);
    void didDeleteWebsiteData(uint64_t callbackID);
    void didDeleteWebsiteDataForHostNames(uint64_t callbackID);

#if PLATFORM(COCOA)
    bool getPluginProcessSerialNumber(ProcessSerialNumber&);
    void makePluginProcessTheFrontProcess();
    void makeUIProcessTheFrontProcess();

    void setFullscreenWindowIsShowing(bool);
    void enterFullscreen();
    void exitFullscreen();

    void setModalWindowIsShowing(bool);
    void beginModal();
    void endModal();

    void applicationDidBecomeActive();
    void openPluginPreferencePane();
    void launchProcess(const String& launchPath, const Vector<String>& arguments, bool& result);
    void launchApplicationAtURL(const String& urlString, const Vector<String>& arguments, bool& result);
    void openURL(const String& url, bool& result, int32_t& status, String& launchedURLString);
    void openFile(const String& fullPath, bool& result);
#endif

    void platformInitializePluginProcess(PluginProcessCreationParameters& parameters);

    // The plug-in host process manager.
    PluginProcessManager* m_pluginProcessManager;

    PluginProcessAttributes m_pluginProcessAttributes;
    uint64_t m_pluginProcessToken;

    // The connection to the plug-in host process.
    RefPtr<IPC::Connection> m_connection;

    Deque<RefPtr<Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply>> m_pendingConnectionReplies;

    Vector<uint64_t> m_pendingFetchWebsiteDataRequests;
    HashMap<uint64_t, std::function<void (Vector<String>)>> m_pendingFetchWebsiteDataCallbacks;

    struct DeleteWebsiteDataRequest {
        std::chrono::system_clock::time_point modifiedSince;
        uint64_t callbackID;
    };
    Vector<DeleteWebsiteDataRequest> m_pendingDeleteWebsiteDataRequests;
    HashMap<uint64_t, std::function<void ()>> m_pendingDeleteWebsiteDataCallbacks;

    struct DeleteWebsiteDataForHostNamesRequest {
        Vector<String> hostNames;
        uint64_t callbackID;
    };
    Vector<DeleteWebsiteDataForHostNamesRequest> m_pendingDeleteWebsiteDataForHostNamesRequests;
    HashMap<uint64_t, std::function<void ()>> m_pendingDeleteWebsiteDataForHostNamesCallbacks;

    // If createPluginConnection is called while the process is still launching we'll keep count of it and send a bunch of requests
    // when the process finishes launching.
    unsigned m_numPendingConnectionRequests;

#if PLATFORM(COCOA)
    RetainPtr<NSObject> m_activationObserver;
    RetainPtr<WKPlaceholderModalWindow *> m_placeholderWindow;
    bool m_modalWindowIsShowing;
    bool m_fullscreenWindowIsShowing;
    unsigned m_preFullscreenAppPresentationOptions;
#endif
};

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#endif // PluginProcessProxy_h
