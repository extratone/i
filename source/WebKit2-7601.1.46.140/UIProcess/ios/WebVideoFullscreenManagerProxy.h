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

#ifndef WebVideoFullscreenManagerProxy_h
#define WebVideoFullscreenManagerProxy_h

#if PLATFORM(IOS)

#include "MessageReceiver.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/WebVideoFullscreenInterfaceAVKit.h>
#include <WebCore/WebVideoFullscreenModel.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

OBJC_CLASS UIView;

namespace WebKit {

class WebPageProxy;
class WebVideoFullscreenManagerProxy;

class WebVideoFullscreenModelContext final: public RefCounted<WebVideoFullscreenModelContext>, public WebCore::WebVideoFullscreenModel, public WebCore::WebVideoFullscreenChangeObserver  {
public:
    static Ref<WebVideoFullscreenModelContext> create(WebVideoFullscreenManagerProxy& manager, uint64_t contextId)
    {
        return adoptRef(*new WebVideoFullscreenModelContext(manager, contextId));
    }
    virtual ~WebVideoFullscreenModelContext() { }

    void invalidate() { m_manager = nullptr; }

    UIView *layerHostView() const { return m_layerHostView.get(); }
    void setLayerHostView(RetainPtr<UIView>&& layerHostView) { m_layerHostView = WTF::move(layerHostView); }

private:
    WebVideoFullscreenModelContext(WebVideoFullscreenManagerProxy& manager, uint64_t contextId)
        : m_manager(&manager)
        , m_contextId(contextId)
    {
    }

    // WebVideoFullscreenModel
    virtual void play() override;
    virtual void pause() override;
    virtual void togglePlayState() override;
    virtual void beginScrubbing() override;
    virtual void endScrubbing() override;
    virtual void seekToTime(double) override;
    virtual void fastSeek(double time) override;
    virtual void beginScanningForward() override;
    virtual void beginScanningBackward() override;
    virtual void endScanning() override;
    virtual void requestExitFullscreen() override;
    virtual void setVideoLayerFrame(WebCore::FloatRect) override;
    virtual void setVideoLayerGravity(VideoGravity) override;
    virtual void selectAudioMediaOption(uint64_t) override;
    virtual void selectLegibleMediaOption(uint64_t) override;
    virtual void fullscreenModeChanged(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) override;

    // WebVideoFullscreenChangeObserver
    virtual void didSetupFullscreen() override;
    virtual void didEnterFullscreen() override;
    virtual void didExitFullscreen() override;
    virtual void didCleanupFullscreen() override;
    virtual void fullscreenMayReturnToInline() override;

    WebVideoFullscreenManagerProxy* m_manager;
    uint64_t m_contextId;
    RetainPtr<UIView *> m_layerHostView;
};

class WebVideoFullscreenManagerProxy : public RefCounted<WebVideoFullscreenManagerProxy>, private IPC::MessageReceiver {
public:
    static RefPtr<WebVideoFullscreenManagerProxy> create(WebPageProxy&);
    virtual ~WebVideoFullscreenManagerProxy();

    void invalidate();

    void requestHideAndExitFullscreen();
    bool hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) const;
    bool mayAutomaticallyShowVideoPictureInPicture() const;

private:
    friend class WebVideoFullscreenModelContext;

    explicit WebVideoFullscreenManagerProxy(WebPageProxy&);
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    typedef std::tuple<RefPtr<WebVideoFullscreenModelContext>, RefPtr<WebCore::WebVideoFullscreenInterfaceAVKit>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(uint64_t contextId);
    ModelInterfaceTuple& ensureModelAndInterface(uint64_t contextId);
    WebVideoFullscreenModelContext& ensureModel(uint64_t contextId);
    WebCore::WebVideoFullscreenInterfaceAVKit& ensureInterface(uint64_t contextId);

    // Messages from WebVideoFullscreenManager
    void setupFullscreenWithID(uint64_t contextId, uint32_t videoLayerID, const WebCore::IntRect& initialRect, float hostingScaleFactor, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool allowsPictureInPicture);
    void resetMediaState(uint64_t contextId);
    void setCurrentTime(uint64_t contextId, double currentTime, double hostTime);
    void setBufferedTime(uint64_t contextId, double bufferedTime);
    void setVideoDimensions(uint64_t contextId, bool hasVideo, unsigned width, unsigned height);
    void setSeekableRangesVector(uint64_t contextId, Vector<std::pair<double, double>> ranges);
    void setCanPlayFastReverse(uint64_t contextId, bool value);
    void setAudioMediaSelectionOptions(uint64_t contextId, Vector<String> options, uint64_t selectedIndex);
    void setLegibleMediaSelectionOptions(uint64_t contextId, Vector<String> options, uint64_t selectedIndex);
    void setExternalPlaybackProperties(uint64_t contextId, bool enabled, uint32_t targetType, String localizedDeviceName);
    void setWirelessVideoPlaybackDisabled(uint64_t contextId, bool);
    void setDuration(uint64_t contextId, double duration);
    void setRate(uint64_t contextId, bool isPlaying, double rate);
    void enterFullscreen(uint64_t contextId);
    void exitFullscreen(uint64_t contextId, WebCore::IntRect finalRect);
    void cleanupFullscreen(uint64_t contextId);
    void preparedToReturnToInline(uint64_t contextId, bool visible, WebCore::IntRect inlineRect);

    // Messages to WebVideoFullscreenManager
    void play(uint64_t contextId);
    void pause(uint64_t contextId);
    void togglePlayState(uint64_t contextId);
    void beginScrubbing(uint64_t contextId);
    void endScrubbing(uint64_t contextId);
    void seekToTime(uint64_t contextId, double time);
    void fastSeek(uint64_t contextId, double time);
    void beginScanningForward(uint64_t contextId);
    void beginScanningBackward(uint64_t contextId);
    void endScanning(uint64_t contextId);
    void requestExitFullscreen(uint64_t contextId);
    void didSetupFullscreen(uint64_t contextId);
    void didExitFullscreen(uint64_t contextId);
    void didEnterFullscreen(uint64_t contextId);
    void didCleanupFullscreen(uint64_t contextId);
    void setVideoLayerFrame(uint64_t contextId, WebCore::FloatRect);
    void setVideoLayerGravity(uint64_t contextId, WebCore::WebVideoFullscreenModel::VideoGravity);
    void selectAudioMediaOption(uint64_t contextId, uint64_t index);
    void selectLegibleMediaOption(uint64_t contextId, uint64_t index);
    void fullscreenModeChanged(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void fullscreenMayReturnToInline(uint64_t contextId);

    WebPageProxy* m_page;
    HashMap<uint64_t, ModelInterfaceTuple> m_contextMap;

};
    
} // namespace WebKit

#endif // PLATFORM(IOS)

#endif // WebVideoFullscreenManagerProxy_h
