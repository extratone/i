/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef MediaPlayer_h
#define MediaPlayer_h

#if ENABLE(VIDEO)

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "MediaPlayerProxy.h"
#endif

#include "IntRect.h"
#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class ContentType;
class FrameView;
class GraphicsContext;
class IntRect;
class IntSize;
class MediaPlayer;
class MediaPlayerPrivateInterface;
class String;

class MediaPlayerClient {
public:
    virtual ~MediaPlayerClient() { }

    // the network state has changed
    virtual void mediaPlayerNetworkStateChanged(MediaPlayer*) { }

    // the ready state has changed
    virtual void mediaPlayerReadyStateChanged(MediaPlayer*) { }

    // the volume or muted state has changed
    virtual void mediaPlayerVolumeChanged(MediaPlayer*) { }

    // time has jumped, eg. not as a result of normal playback
    virtual void mediaPlayerTimeChanged(MediaPlayer*) { }
    
    // a new frame of video is available
    virtual void mediaPlayerRepaint(MediaPlayer*) { }

    // the media file duration has changed, or is now known
    virtual void mediaPlayerDurationChanged(MediaPlayer*) { }
    
    // the playback rate has changed
    virtual void mediaPlayerRateChanged(MediaPlayer*) { }

    // the movie size has changed
    virtual void mediaPlayerSizeChanged(MediaPlayer*) { }
};

class MediaPlayer : Noncopyable {
public:
    MediaPlayer(MediaPlayerClient*);
    virtual ~MediaPlayer();

    // media engine support
    enum SupportsType { IsNotSupported, IsSupported, MayBeSupported };
    static MediaPlayer::SupportsType supportsType(ContentType contentType);
    static void getSupportedTypes(HashSet<String>&);
    static bool isAvailable();

    IntSize naturalSize();
    bool hasVideo();
    
    void setFrameView(FrameView* frameView) { m_frameView = frameView; }
    FrameView* frameView() { return m_frameView; }
    bool inMediaDocument();
    
    IntSize size() const { return m_size; }
    void setSize(const IntSize& size);
    
    void load(const String& url, const ContentType& contentType);
    void cancelLoad();
    
    bool visible() const;
    void setVisible(bool);
    
    void play();
    void pause();    
    
    bool paused() const;
    bool seeking() const;
    
    float duration() const;
    float currentTime() const;
    void seek(float time);
    
    void setEndTime(float time);
    
    float rate() const;
    void setRate(float);
    
    float maxTimeBuffered();
    float maxTimeSeekable();

    unsigned bytesLoaded();
    bool totalBytesKnown();
    unsigned totalBytes();
    
    float volume() const;
    void setVolume(float);
    
    int dataRate() const;

    bool autobuffer() const;    
    void setAutobuffer(bool);

    void paint(GraphicsContext*, const IntRect&);
    
    enum NetworkState { Empty, Idle, Loading, Loaded, FormatError, NetworkError, DecodeError };
    NetworkState networkState();

    enum ReadyState  { HaveNothing, HaveMetadata, HaveCurrentData, HaveFutureData, HaveEnoughData };
    ReadyState readyState();
    
    void networkStateChanged();
    void readyStateChanged();
    void volumeChanged();
    void timeChanged();
    void sizeChanged();
    void rateChanged();
    void durationChanged();

    void repaint();

    MediaPlayerClient* mediaPlayerClient() const { return m_mediaPlayerClient; }

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    void setPoster(const String& url);
    void deliverNotification(MediaPlayerProxyNotificationType notification);
    void setMediaPlayerProxy(WebMediaPlayerProxy* proxy);
#endif

private:
    static void initializeMediaEngines();

    MediaPlayerClient* m_mediaPlayerClient;
    OwnPtr<MediaPlayerPrivateInterface*> m_private;
    void* m_currentMediaEngine;
    FrameView* m_frameView;
    IntSize m_size;
    bool m_visible;
    float m_rate;
    float m_volume;
    bool m_autobuffer;
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    WebMediaPlayerProxy* m_playerProxy;    // not owned or used, passed to m_private
#endif
};

typedef MediaPlayerPrivateInterface* (*CreateMediaEnginePlayer)(MediaPlayer*);
typedef void (*MediaEngineSupportedTypes)(HashSet<String>& types);
typedef MediaPlayer::SupportsType (*MediaEngineSupportsType)(const String& type, const String& codecs);

typedef void (*MediaEngineRegistrar)(CreateMediaEnginePlayer, MediaEngineSupportedTypes, MediaEngineSupportsType); 


}

#endif // ENABLE(VIDEO)

#endif
