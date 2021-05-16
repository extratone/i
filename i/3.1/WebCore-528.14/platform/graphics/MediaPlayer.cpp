/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)
#include "MediaPlayer.h"
#include "MediaPlayerPrivate.h"

#include "ContentType.h"
#include "IntRect.h"
#include "MIMETypeRegistry.h"
#include "FrameView.h"
#include "Frame.h"
#include "Document.h"

#include "MediaPlayerPrivateIPhone.h"

namespace WebCore {

// a null player to make MediaPlayer logic simpler

class NullMediaPlayerPrivate : public MediaPlayerPrivateInterface {
public:
    NullMediaPlayerPrivate(MediaPlayer*) { }

    virtual void load(const String&) { }
    virtual void cancelLoad() { }
    
    virtual void play() { }
    virtual void pause() { }    

    virtual IntSize naturalSize() const { return IntSize(0, 0); }

    virtual bool hasVideo() const { return false; }

    virtual void setVisible(bool) { }

    virtual float duration() const { return 0; }

    virtual float currentTime() const { return 0; }
    virtual void seek(float) { }
    virtual bool seeking() const { return false; }

    virtual void setEndTime(float) { }

    virtual void setRate(float) { }
    virtual bool paused() const { return false; }

    virtual void setVolume(float) { }

    virtual MediaPlayer::NetworkState networkState() const { return MediaPlayer::Empty; }
    virtual MediaPlayer::ReadyState readyState() const { return MediaPlayer::HaveNothing; }

    virtual float maxTimeSeekable() const { return 0; }
    virtual float maxTimeBuffered() const { return 0; }

    virtual int dataRate() const { return 0; }

    virtual bool totalBytesKnown() const { return false; }
    virtual unsigned totalBytes() const { return 0; }
    virtual unsigned bytesLoaded() const { return 0; }

    virtual void setSize(const IntSize&) { }

    virtual void paint(GraphicsContext*, const IntRect&) { }

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    virtual void setPoster(const String& /*url*/) { }
    virtual void deliverNotification(MediaPlayerProxyNotificationType) { }
    virtual void setMediaPlayerProxy(WebMediaPlayerProxy*) { }
#endif
};

static MediaPlayerPrivateInterface* createNullMediaPlayer(MediaPlayer* player) 
{ 
    return new NullMediaPlayerPrivate(player); 
}


// engine support

struct MediaPlayerFactory {
    MediaPlayerFactory(CreateMediaEnginePlayer constructor, MediaEngineSupportedTypes getSupportedTypes, MediaEngineSupportsType supportsTypeAndCodecs) 
        : constructor(constructor)
        , getSupportedTypes(getSupportedTypes)
        , supportsTypeAndCodecs(supportsTypeAndCodecs)  
    { 
    }

    CreateMediaEnginePlayer constructor;
    MediaEngineSupportedTypes getSupportedTypes;
    MediaEngineSupportsType supportsTypeAndCodecs;
};

static void addMediaEngine(CreateMediaEnginePlayer, MediaEngineSupportedTypes, MediaEngineSupportsType);
static MediaPlayerFactory* chooseBestEngineForTypeAndCodecs(const String& type, const String& codecs);

static Vector<MediaPlayerFactory*>& installedMediaEngines() 
{
    DEFINE_STATIC_LOCAL(Vector<MediaPlayerFactory*>, installedEngines, ());
    static bool enginesQueried = false;

    if (!enginesQueried) {
        enginesQueried = true;

        MediaPlayerPrivateiPhone::registerMediaEngine(addMediaEngine);
        // register additional engines here
    }
    
    return installedEngines;
}

static void addMediaEngine(CreateMediaEnginePlayer constructor, MediaEngineSupportedTypes getSupportedTypes, MediaEngineSupportsType supportsType)
{
    ASSERT(constructor);
    ASSERT(getSupportedTypes);
    ASSERT(supportsType);
    installedMediaEngines().append(new MediaPlayerFactory(constructor, getSupportedTypes, supportsType));
}

static MediaPlayerFactory* chooseBestEngineForTypeAndCodecs(const String& type, const String& codecs)
{
    Vector<MediaPlayerFactory*>& engines = installedMediaEngines();

    if (engines.isEmpty())
        return 0;

    MediaPlayerFactory* engine = 0;
    MediaPlayer::SupportsType supported = MediaPlayer::IsNotSupported;

    unsigned count = engines.size();
    for (unsigned ndx = 0; ndx < count; ndx++) {
        MediaPlayer::SupportsType engineSupport = engines[ndx]->supportsTypeAndCodecs(type, codecs);
        if (engineSupport > supported) {
            supported = engineSupport;
            engine = engines[ndx];
        }
    }

    return engine;
}

// media player

MediaPlayer::MediaPlayer(MediaPlayerClient* client)
    : m_mediaPlayerClient(client)
    , m_private(createNullMediaPlayer(this))
    , m_currentMediaEngine(0)
    , m_frameView(0)
    , m_visible(false)
    , m_rate(1.0f)
    , m_volume(1.0f)
    , m_autobuffer(false)
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    , m_playerProxy(0)
#endif
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    Vector<MediaPlayerFactory*>& engines = installedMediaEngines();
    if (!engines.isEmpty()) {
        m_currentMediaEngine = engines[0];
        m_private.clear();
        m_private.set(engines[0]->constructor(this));
    }
#endif
}

MediaPlayer::~MediaPlayer()
{
}

void MediaPlayer::load(const String& url, const ContentType& contentType)
{
    String type = contentType.type();
    String codecs = contentType.parameter("codecs");

    // if we don't know the MIME type, see if the path can help
    if (type.isEmpty()) 
        type = MIMETypeRegistry::getMIMETypeForPath(url);

    MediaPlayerFactory* engine = chooseBestEngineForTypeAndCodecs(type, codecs);

    // if we didn't find an engine that claims the MIME type, just use the first engine
    if (!engine)
        engine = installedMediaEngines()[0];
    
    // don't delete and recreate the player unless it comes from a different engine
    if (engine && m_currentMediaEngine != engine) {
        m_currentMediaEngine = engine;
        m_private.clear();
        m_private.set(engine->constructor(this));
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
        m_private->setMediaPlayerProxy(m_playerProxy);
#endif

    }

    if (m_private)
        m_private->load(url);
    else
        m_private.set(createNullMediaPlayer(this));
}    

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
void MediaPlayer::setPoster(const String& url)
{
    m_private->setPoster(url);
}    
#endif

void MediaPlayer::cancelLoad()
{
    m_private->cancelLoad();
}    

void MediaPlayer::play()
{
    m_private->play();
}

void MediaPlayer::pause()
{
    m_private->pause();
}

float MediaPlayer::duration() const
{
    return m_private->duration();
}

float MediaPlayer::currentTime() const
{
    return m_private->currentTime();
}

void MediaPlayer::seek(float time)
{
    m_private->seek(time);
}

bool MediaPlayer::paused() const
{
    return m_private->paused();
}

bool MediaPlayer::seeking() const
{
    return m_private->seeking();
}

IntSize MediaPlayer::naturalSize()
{
    return m_private->naturalSize();
}

bool MediaPlayer::hasVideo()
{
    return m_private->hasVideo();
}

bool MediaPlayer::inMediaDocument()
{
    Frame* frame = m_frameView ? m_frameView->frame() : 0;
    Document* document = frame ? frame->document() : 0;
    
    return document && document->isMediaDocument();
}

MediaPlayer::NetworkState MediaPlayer::networkState()
{
    return m_private->networkState();
}

MediaPlayer::ReadyState MediaPlayer::readyState()
{
    return m_private->readyState();
}

float MediaPlayer::volume() const
{
    return m_volume;
}

void MediaPlayer::setVolume(float volume)
{
    m_volume = volume;
    m_private->setVolume(volume);   
}

float MediaPlayer::rate() const
{
    return m_rate;
}

void MediaPlayer::setRate(float rate)
{
    m_rate = rate;
    m_private->setRate(rate);   
}

int MediaPlayer::dataRate() const
{
    return m_private->dataRate();
}

void MediaPlayer::setEndTime(float time)
{
    m_private->setEndTime(time);
}

float MediaPlayer::maxTimeBuffered()
{
    return m_private->maxTimeBuffered();
}

float MediaPlayer::maxTimeSeekable()
{
    return m_private->maxTimeSeekable();
}

unsigned MediaPlayer::bytesLoaded()
{
    return m_private->bytesLoaded();
}

bool MediaPlayer::totalBytesKnown()
{
    return m_private->totalBytesKnown();
}

unsigned MediaPlayer::totalBytes()
{
    return m_private->totalBytes();
}

void MediaPlayer::setSize(const IntSize& size)
{ 
    m_size = size;
    m_private->setSize(size);
}

bool MediaPlayer::visible() const
{
    return m_visible;
}

void MediaPlayer::setVisible(bool b)
{
    m_visible = b;
    m_private->setVisible(b);
}

bool MediaPlayer::autobuffer() const
{
    return m_autobuffer;
}

void MediaPlayer::setAutobuffer(bool b)
{
    if (m_autobuffer != b) {
        m_autobuffer = b;
        m_private->setAutobuffer(b);
    }
}

void MediaPlayer::paint(GraphicsContext* p, const IntRect& r)
{
    m_private->paint(p, r);
}

MediaPlayer::SupportsType MediaPlayer::supportsType(ContentType contentType)
{
    String type = contentType.type();
    String codecs = contentType.parameter("codecs");
    MediaPlayerFactory* engine = chooseBestEngineForTypeAndCodecs(type, codecs);

    if (!engine)
        return IsNotSupported;

    return engine->supportsTypeAndCodecs(type, codecs);
}

void MediaPlayer::getSupportedTypes(HashSet<String>& types)
{
    Vector<MediaPlayerFactory*>& engines = installedMediaEngines();
    if (engines.isEmpty())
        return;

    unsigned count = engines.size();
    for (unsigned ndx = 0; ndx < count; ndx++)
        engines[ndx]->getSupportedTypes(types);
} 

bool MediaPlayer::isAvailable()
{
    return !installedMediaEngines().isEmpty();
} 

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
void MediaPlayer::deliverNotification(MediaPlayerProxyNotificationType notification)
{
    m_private->deliverNotification(notification);
}

void MediaPlayer::setMediaPlayerProxy(WebMediaPlayerProxy* proxy)
{
    m_playerProxy = proxy;
    m_private->setMediaPlayerProxy(proxy);
}
#endif

void MediaPlayer::networkStateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerNetworkStateChanged(this);
}

void MediaPlayer::readyStateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerReadyStateChanged(this);
}

void MediaPlayer::volumeChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerVolumeChanged(this);
}

void MediaPlayer::timeChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerTimeChanged(this);
}

void MediaPlayer::sizeChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerSizeChanged(this);
}

void MediaPlayer::repaint()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerRepaint(this);
}

void MediaPlayer::durationChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerDurationChanged(this);
}

void MediaPlayer::rateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerRateChanged(this);
}

}
#endif
