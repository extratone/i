/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
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

#ifndef NetscapePlugin_h
#define NetscapePlugin_h

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "NetscapePluginModule.h"
#include "Plugin.h"
#include <WebCore/AffineTransform.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IntRect.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include "WebHitTestResult.h"
#endif

#if PLUGIN_ARCHITECTURE(X11)
#include <WebCore/XUniqueResource.h>
#endif

namespace WebCore {
class MachSendRight;
class HTTPHeaderMap;
class ProtectionSpace;
class SharedBuffer;
}

OBJC_CLASS WKNPAPIPlugInContainer;

namespace WebKit {

class NetscapePluginStream;
    
class NetscapePlugin : public Plugin {
public:
    static RefPtr<NetscapePlugin> create(PassRefPtr<NetscapePluginModule>);
    virtual ~NetscapePlugin();

    static PassRefPtr<NetscapePlugin> fromNPP(NPP);

    // In-process NetscapePlugins don't support asynchronous initialization.
    virtual bool isBeingAsynchronouslyInitialized() const override { return false; }

#if PLATFORM(COCOA)
    NPError setDrawingModel(NPDrawingModel);
    NPError setEventModel(NPEventModel);
    NPBool convertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double& destX, double& destY, NPCoordinateSpace destSpace);
    NPError popUpContextMenu(NPMenu*);

    void setPluginReturnsNonretainedLayer(bool pluginReturnsNonretainedLayer) { m_pluginReturnsNonretainedLayer = pluginReturnsNonretainedLayer; }
    void setPluginWantsLegacyCocoaTextInput(bool pluginWantsLegacyCocoaTextInput) { m_pluginWantsLegacyCocoaTextInput = pluginWantsLegacyCocoaTextInput; }

    bool hasHandledAKeyDownEvent() const { return m_hasHandledAKeyDownEvent; }

    const WebCore::MachSendRight& compositingRenderServerPort();
    void openPluginPreferencePane();

    // Computes an affine transform from the given coordinate space to the screen coordinate space.
    bool getScreenTransform(NPCoordinateSpace sourceSpace, WebCore::AffineTransform&);

    WKNPAPIPlugInContainer* plugInContainer();

#ifndef NP_NO_CARBON
    WindowRef windowRef() const;
    bool isWindowActive() const { return m_windowHasFocus; }
    void updateFakeWindowBounds();

    static NetscapePlugin* netscapePluginFromWindow(WindowRef);
    static unsigned buttonState();
#endif

#endif

    PluginQuirks quirks() const { return m_pluginModule->pluginQuirks(); }

    void invalidate(const NPRect*);
    static const char* userAgent(NPP);
    void loadURL(const String& method, const String& urlString, const String& target, const WebCore::HTTPHeaderMap& headerFields,
                 const Vector<uint8_t>& httpBody, bool sendNotification, void* notificationData);
    NPError destroyStream(NPStream*, NPReason);
    void setIsWindowed(bool);
    void setIsTransparent(bool);
    void setStatusbarText(const String&);
    static void setException(const String&);
    bool evaluate(NPObject*, const String&scriptString, NPVariant* result);
    bool isPrivateBrowsingEnabled();
    bool isMuted() const;

    static void setSetExceptionFunction(void (*)(const String&));

    // These return retained objects.
    NPObject* windowScriptNPObject();
    NPObject* pluginElementNPObject();

    void cancelStreamLoad(NetscapePluginStream*);
    void removePluginStream(NetscapePluginStream*);

    bool isAcceleratedCompositingEnabled();

    void pushPopupsEnabledState(bool enabled);
    void popPopupsEnabledState();

    void pluginThreadAsyncCall(void (*function)(void*), void* userData);

    unsigned scheduleTimer(unsigned interval, bool repeat, void (*timerFunc)(NPP, unsigned timerID));
    void unscheduleTimer(unsigned timerID);

    double contentsScaleFactor();
    String proxiesForURL(const String& urlString);
    String cookiesForURL(const String& urlString);
    void setCookiesForURL(const String& urlString, const String& cookieString);
    bool getAuthenticationInfo(const WebCore::ProtectionSpace&, String& username, String& password);

    void setIsPlayingAudio(bool);

    // Member functions for calling into the plug-in.
    NPError NPP_New(NPMIMEType pluginType, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData*);
    NPError NPP_Destroy(NPSavedData**);
    NPError NPP_SetWindow(NPWindow*);
    NPError NPP_NewStream(NPMIMEType, NPStream*, NPBool seekable, uint16_t* stype);
    NPError NPP_DestroyStream(NPStream*, NPReason);
    void NPP_StreamAsFile(NPStream*, const char* filename);
    int32_t NPP_WriteReady(NPStream*);
    int32_t NPP_Write(NPStream*, int32_t offset, int32_t len, void* buffer);
    int16_t NPP_HandleEvent(void* event);
    void NPP_URLNotify(const char* url, NPReason, void* notifyData);
    NPError NPP_GetValue(NPPVariable, void *value);
    NPError NPP_SetValue(NPNVariable, void *value);

private:
    NetscapePlugin(PassRefPtr<NetscapePluginModule> pluginModule);

    void callSetWindow();
    void callSetWindowInvisible();
    bool shouldLoadSrcURL();
    NetscapePluginStream* streamFromID(uint64_t streamID);
    void stopAllStreams();
    bool allowPopups() const;

    const char* userAgent();

    void platformPreInitialize();
    bool platformPostInitialize();
    void platformDestroy();
    bool platformInvalidate(const WebCore::IntRect&);
    void platformGeometryDidChange();
    void platformVisibilityDidChange();
    void platformPaint(WebCore::GraphicsContext*, const WebCore::IntRect& dirtyRect, bool isSnapshot = false);

    bool platformHandleMouseEvent(const WebMouseEvent&);
    bool platformHandleWheelEvent(const WebWheelEvent&);
    bool platformHandleMouseEnterEvent(const WebMouseEvent&);
    bool platformHandleMouseLeaveEvent(const WebMouseEvent&);
    bool platformHandleKeyboardEvent(const WebKeyboardEvent&);
    void platformSetFocus(bool);

    static bool wantsPluginRelativeNPWindowCoordinates();

    // Plugin
    virtual bool initialize(const Parameters&) override;
    virtual void destroy() override;
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect& dirtyRect) override;
    virtual PassRefPtr<ShareableBitmap> snapshot() override;
#if PLATFORM(COCOA)
    virtual PlatformLayer* pluginLayer() override;
#endif
    virtual bool isTransparent() override;
    virtual bool wantsWheelEvents() override;
    virtual void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::IntRect& clipRect, const WebCore::AffineTransform& pluginToRootViewTransform) override;
    virtual void visibilityDidChange(bool isVisible) override;
    virtual void frameDidFinishLoading(uint64_t requestID) override;
    virtual void frameDidFail(uint64_t requestID, bool wasCancelled) override;
    virtual void didEvaluateJavaScript(uint64_t requestID, const String& result) override;
    virtual void streamDidReceiveResponse(uint64_t streamID, const WebCore::URL& responseURL, uint32_t streamLength, 
                                          uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) override;
    virtual void streamDidReceiveData(uint64_t streamID, const char* bytes, int length) override;
    virtual void streamDidFinishLoading(uint64_t streamID) override;
    virtual void streamDidFail(uint64_t streamID, bool wasCancelled) override;
    virtual void manualStreamDidReceiveResponse(const WebCore::URL& responseURL, uint32_t streamLength, 
                                                uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) override;
    virtual void manualStreamDidReceiveData(const char* bytes, int length) override;
    virtual void manualStreamDidFinishLoading() override;
    virtual void manualStreamDidFail(bool wasCancelled) override;
    
    virtual bool handleMouseEvent(const WebMouseEvent&) override;
    virtual bool handleWheelEvent(const WebWheelEvent&) override;
    virtual bool handleMouseEnterEvent(const WebMouseEvent&) override;
    virtual bool handleMouseLeaveEvent(const WebMouseEvent&) override;
    virtual bool handleContextMenuEvent(const WebMouseEvent&) override;
    virtual bool handleKeyboardEvent(const WebKeyboardEvent&) override;
    virtual void setFocus(bool) override;

    virtual bool handleEditingCommand(const String& commandName, const String& argument) override;
    virtual bool isEditingCommandEnabled(const String&) override;

    virtual bool shouldAllowScripting() override;
    virtual bool shouldAllowNavigationFromDrags() override;
    
    virtual bool handlesPageScaleFactor() override;

    virtual NPObject* pluginScriptableNPObject() override;
    
    virtual unsigned countFindMatches(const String&, WebCore::FindOptions, unsigned maxMatchCount) override;
    virtual bool findString(const String&, WebCore::FindOptions, unsigned maxMatchCount) override;

    virtual void windowFocusChanged(bool) override;
    virtual void windowVisibilityChanged(bool) override;

#if PLATFORM(COCOA)
    virtual void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates) override;

    virtual uint64_t pluginComplexTextInputIdentifier() const override;
    virtual void sendComplexTextInput(const String& textInput) override;
    virtual void setLayerHostingMode(LayerHostingMode) override;

    void pluginFocusOrWindowFocusChanged();
    void setComplexTextInputEnabled(bool);

    void updatePluginLayer();
#endif

    virtual void contentsScaleFactorChanged(float) override;
    virtual void storageBlockingStateChanged(bool) override;
    virtual void privateBrowsingStateChanged(bool) override;
    virtual bool getFormValue(String& formValue) override;
    virtual bool handleScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) override;
    virtual WebCore::Scrollbar* horizontalScrollbar() override;
    virtual WebCore::Scrollbar* verticalScrollbar() override;

    virtual bool supportsSnapshotting() const override;

    // Convert the given point from plug-in coordinates to root view coordinates.
    virtual WebCore::IntPoint convertToRootView(const WebCore::IntPoint&) const override;

    // Convert the given point from root view coordinates to plug-in coordinates. Returns false if the point can't be
    // converted (if the transformation matrix isn't invertible).
    bool convertFromRootView(const WebCore::IntPoint& pointInRootViewCoordinates, WebCore::IntPoint& pointInPluginCoordinates);

    virtual PassRefPtr<WebCore::SharedBuffer> liveResourceData() const override;

    virtual bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override { return false; }

    String getSelectionString() const override { return String(); }
    String getSelectionForWordAtPoint(const WebCore::FloatPoint&) const override { return String(); }
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const override { return false; }

    virtual void mutedStateChanged(bool) override;

    void updateNPNPrivateMode();

#if PLUGIN_ARCHITECTURE(X11)
    bool platformPostInitializeWindowed(bool needsXEmbed, uint64_t windowID);
    bool platformPostInitializeWindowless();
#endif

    uint64_t m_nextRequestID;

    typedef HashMap<uint64_t, std::pair<String, void*>> PendingURLNotifyMap;
    PendingURLNotifyMap m_pendingURLNotifications;

    typedef HashMap<uint64_t, RefPtr<NetscapePluginStream>> StreamsMap;
    StreamsMap m_streams;

    RefPtr<NetscapePluginModule> m_pluginModule;
    NPP_t m_npp;
    NPWindow m_npWindow;

    WebCore::IntSize m_pluginSize;

    // The clip rect in plug-in coordinates.
    WebCore::IntRect m_clipRect;

    // A transform that can be used to convert from root view coordinates to plug-in coordinates.
    WebCore::AffineTransform m_pluginToRootViewTransform;

#if PLUGIN_ARCHITECTURE(X11)
    WebCore::IntRect m_frameRectInWindowCoordinates;
#endif

    CString m_userAgent;

    bool m_isStarted;
    bool m_isWindowed;
    bool m_isTransparent;
    bool m_inNPPNew;
    bool m_shouldUseManualLoader;
    bool m_hasCalledSetWindow;
    bool m_isVisible;

    RefPtr<NetscapePluginStream> m_manualStream;
    Vector<bool, 8> m_popupEnabledStates;

    class Timer {
        WTF_MAKE_NONCOPYABLE(Timer);

    public:
        typedef void (*TimerFunc)(NPP, uint32_t timerID);

        Timer(NetscapePlugin*, unsigned timerID, unsigned interval, bool repeat, TimerFunc);
        ~Timer();

        void start();
        void stop();

    private:
        void timerFired();

        // This is a weak pointer since Timer objects are destroyed before the NetscapePlugin object itself is destroyed.
        NetscapePlugin* m_netscapePlugin;

        unsigned m_timerID;
        unsigned m_interval;
        bool m_repeat;
        TimerFunc m_timerFunc;

        RunLoop::Timer<Timer> m_timer;
    };
    typedef HashMap<unsigned, std::unique_ptr<Timer>> TimerMap;
    TimerMap m_timers;
    unsigned m_nextTimerID;

    bool m_privateBrowsingState;
    bool m_storageBlockingState;

#if PLUGIN_ARCHITECTURE(MAC)
    NPDrawingModel m_drawingModel;
    NPEventModel m_eventModel;

    RetainPtr<PlatformLayer> m_pluginLayer;
    bool m_pluginReturnsNonretainedLayer;
    LayerHostingMode m_layerHostingMode;

    NPCocoaEvent* m_currentMouseEvent;

    bool m_pluginHasFocus;
    bool m_windowHasFocus;

    // Whether the plug-in wants to use the legacy Cocoa text input handling that
    // existed in WebKit1, or the updated Cocoa text input handling specified on
    // https://wiki.mozilla.org/NPAPI:CocoaEventModel#Text_Input
    bool m_pluginWantsLegacyCocoaTextInput;

    // Whether complex text input is enabled.
    bool m_isComplexTextInputEnabled;

    // Whether the plug-in has handled a keydown event. This is used to determine
    // if we can tell the plug-in that we support the updated Cocoa text input specification.
    bool m_hasHandledAKeyDownEvent;

    // The number of NPCocoaEventKeyUp events that  should be ignored.
    unsigned m_ignoreNextKeyUpEventCounter;

    WebCore::IntRect m_windowFrameInScreenCoordinates;
    WebCore::IntRect m_viewFrameInWindowCoordinates;

    RetainPtr<WKNPAPIPlugInContainer> m_plugInContainer;

#ifndef NP_NO_CARBON
    void nullEventTimerFired();

    // FIXME: It's a bit wasteful to have one null event timer per plug-in.
    // We should investigate having one per window.
    RunLoop::Timer<NetscapePlugin> m_nullEventTimer;
    NP_CGContext m_npCGContext;
#endif
#elif PLUGIN_ARCHITECTURE(X11)
    WebCore::XUniquePixmap m_drawable;
    Display* m_pluginDisplay;
#if PLATFORM(GTK)
    GtkWidget* m_platformPluginWidget;
#endif

public: // Need to call it in the NPN_GetValue browser callback.
    static Display* x11HostDisplay();
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_PLUGIN(NetscapePlugin, NetscapePluginType)

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#endif // NetscapePlugin_h
