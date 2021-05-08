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
#include "HTMLMediaElement.h"

#include "ContentType.h"
#include "CSSHelper.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "DocLoader.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLSourceElement.h"
#include "HTMLVideoElement.h"
#include <limits>
#include "MediaError.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayer.h"
#include "Page.h"
#include "ProgressEvent.h"
#include "RenderVideo.h"
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "RenderPartObject.h"
#endif
#include "TimeRanges.h"
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "Widget.h"
#endif
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <limits>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

HTMLMediaElement::HTMLMediaElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
    , m_loadTimer(this, &HTMLMediaElement::loadTimerFired)
    , m_asyncEventTimer(this, &HTMLMediaElement::asyncEventTimerFired)
    , m_progressEventTimer(this, &HTMLMediaElement::progressEventTimerFired)
    , m_playbackProgressTimer(this, &HTMLMediaElement::playbackProgressTimerFired)
    , m_playbackRate(1.0f)
    , m_defaultPlaybackRate(1.0f)
    , m_networkState(NETWORK_EMPTY)
    , m_readyState(HAVE_NOTHING)
    , m_volume(1.0f)
    , m_currentTimeDuringSeek(0)
    , m_previousProgress(0)
    , m_previousProgressTime(numeric_limits<double>::max())
    , m_lastTimeUpdateEventWallTime(0)
    , m_lastTimeUpdateEventMovieTime(numeric_limits<float>::max())
    , m_loadState(WaitingForSource)
    , m_currentSourceNode(0)
    , m_player(0)
    , m_restrictions(NoRestrictions)
    , m_processingMediaPlayerCallback(0)
    , m_processingLoad(false)
    , m_delayingTheLoadEvent(false)
    , m_haveFiredLoadedData(false)
    , m_inActiveDocument(true)
    , m_autoplaying(true)
    , m_muted(false)
    , m_paused(true)
    , m_seeking(false)
    , m_sentStalledEvent(false)
    , m_sentEndEvent(false)
    , m_pausedInternal(false)
    , m_sendProgressEvents(true)
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    , m_needWidgetUpdate(false)
#endif
    , m_inFullScreen(false)
    , m_requestingPlay(false)
{
    m_sendProgressEvents = false;
    m_restrictions = RequireUserGestureForRateChangeRestriction;
    document()->registerForDocumentActivationCallbacks(this);
    document()->registerForMediaVolumeCallbacks(this);
}

HTMLMediaElement::~HTMLMediaElement()
{
    document()->unregisterForDocumentActivationCallbacks(this);
    document()->unregisterForMediaVolumeCallbacks(this);
}

bool HTMLMediaElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(sourceTag) || HTMLElement::checkDTD(newChild);
}

void HTMLMediaElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    HTMLElement::attributeChanged(attr, preserveDecls);

    const QualifiedName& attrName = attr->name();
    if (attrName == srcAttr) {
        // don't have a src or any <source> children, trigger load
        if (inDocument() && m_loadState == WaitingForSource) {
            scheduleLoad();
        }
    } 
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    else if (attrName == controlsAttr) {
        if (!isVideo() && attached() && (controls() != (renderer() != 0))) {
            detach();
            attach();
        }
        if (renderer())
            renderer()->updateFromElement();
    }
#endif
}

void HTMLMediaElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == autobufferAttr) {
        if (m_player)
            m_player->setAutobuffer(!attr->isNull());
    } else
        HTMLElement::parseMappedAttribute(attr);
}

bool HTMLMediaElement::rendererIsNeeded(RenderStyle* style)
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    UNUSED_PARAM(style);
    Frame* frame = document()->frame();
    if (!frame)
        return false;

    return true;
#else
    return controls() ? HTMLElement::rendererIsNeeded(style) : false;
#endif
}

RenderObject* HTMLMediaElement::createRenderer(RenderArena* arena, RenderStyle*)
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    return new (arena) RenderPartObject(this);
#else
    return new (arena) RenderMedia(this);
#endif
}
 
void HTMLMediaElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
}

void HTMLMediaElement::removedFromDocument()
{
    if (m_networkState > NETWORK_EMPTY)
        pause();
    HTMLElement::removedFromDocument();
}

void HTMLMediaElement::attach()
{
    ASSERT(!attached());

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    m_needWidgetUpdate = true;
#endif

    HTMLElement::attach();

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::recalcStyle(StyleChange change)
{
    HTMLElement::recalcStyle(change);

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::scheduleLoad()
{
    m_loadTimer.startOneShot(0);
}

void HTMLMediaElement::scheduleProgressEvent(const AtomicString& eventName)
{
    if (!m_sendProgressEvents)
        return;

    // FIXME: don't schedule timeupdate or progress events unless there are registered listeners

    bool totalKnown = m_player && m_player->totalBytesKnown();
    unsigned loaded = m_player ? m_player->bytesLoaded() : 0;
    unsigned total = m_player ? m_player->totalBytes() : 0;

    RefPtr<ProgressEvent> evt = ProgressEvent::create(eventName, totalKnown, loaded, total);
    enqueueEvent(evt);

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::scheduleEvent(const AtomicString& eventName)
{
    enqueueEvent(Event::create(eventName, false, true));
}

void HTMLMediaElement::enqueueEvent(RefPtr<Event> event)
{
    m_pendingEvents.append(event);
    if (!m_asyncEventTimer.isActive())                            
        m_asyncEventTimer.startOneShot(0);
}

void HTMLMediaElement::asyncEventTimerFired(Timer<HTMLMediaElement>*)
{
    Vector<RefPtr<Event> > pendingEvents;
    ExceptionCode ec = 0;

    m_pendingEvents.swap(pendingEvents);
    unsigned count = pendingEvents.size();
    for (unsigned ndx = 0; ndx < count; ++ndx) 
        dispatchEvent(pendingEvents[ndx].release(), ec);
}

void HTMLMediaElement::loadTimerFired(Timer<HTMLMediaElement>*)
{
    if (m_loadState == LoadingFromSourceElement)
        loadNextSourceChild();
    else
        loadInternal();
}

static String serializeTimeOffset(float time)
{
    String timeString = String::number(time);
    // FIXME serialize time offset values properly (format not specified yet)
    timeString.append("s");
    return timeString;
}

static float parseTimeOffset(const String& timeString, bool* ok = 0)
{
    const UChar* characters = timeString.characters();
    unsigned length = timeString.length();
    
    if (length && characters[length - 1] == 's')
        length--;
    
    // FIXME parse time offset values (format not specified yet)
    float val = charactersToFloat(characters, length, ok);
    return val;
}

float HTMLMediaElement::getTimeOffsetAttribute(const QualifiedName& name, float valueOnError) const
{
    bool ok;
    String timeString = getAttribute(name);
    float result = parseTimeOffset(timeString, &ok);
    if (ok)
        return result;
    return valueOnError;
}

void HTMLMediaElement::setTimeOffsetAttribute(const QualifiedName& name, float value)
{
    setAttribute(name, serializeTimeOffset(value));
}

PassRefPtr<MediaError> HTMLMediaElement::error() const 
{
    return m_error;
}

KURL HTMLMediaElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLMediaElement::setSrc(const String& url)
{
    setAttribute(srcAttr, url);
}

String HTMLMediaElement::currentSrc() const
{
    return m_currentSrc;
}

HTMLMediaElement::NetworkState HTMLMediaElement::networkState() const
{
    return m_networkState;
}

String HTMLMediaElement::canPlayType(const String& mimeType) const
{
    MediaPlayer::SupportsType support = MediaPlayer::supportsType(ContentType(mimeType));
    String canPlay;

    // 4.8.10.3
    switch (support)
    {
        case MediaPlayer::IsNotSupported:
            canPlay = "no";
            break;
        case MediaPlayer::MayBeSupported:
            canPlay = "maybe";
            break;
        case MediaPlayer::IsSupported:
            canPlay = "probably";
            break;
    }
    
    return canPlay;
}

void HTMLMediaElement::load(ExceptionCode& ec)
{
    bool allowedToLoad = !(m_restrictions & RequireUserGestureForLoadRestriction && !processingUserGesture());
    if (!allowedToLoad)
        ec = INVALID_STATE_ERR;
    else
        loadInternal();
}

void HTMLMediaElement::loadInternal()
{
    // 1 - If the load() method for this element is already being invoked, then abort these steps.
    if (m_processingLoad)
        return;
    m_processingLoad = true;

    stopPeriodicTimers();
    m_loadTimer.stop();
    m_sentStalledEvent = false;
    m_haveFiredLoadedData = false;

    // 2 - Abort any already-running instance of the resource selection algorithm for this element.
    m_currentSourceNode = 0;

    // 3 - If there are any tasks from the media element's media element event task source in 
    // one of the task queues, then remove those tasks.
    m_pendingEvents.clear();
    
    // 4 - If the media element's networkState is set to NETWORK_LOADING or NETWORK_IDLE, set the 
    // error attribute to a new MediaError object whose code attribute is set to MEDIA_ERR_ABORTED, 
    // and fire a progress event called abort at the media element.
    if (m_networkState == NETWORK_LOADING || m_networkState == NETWORK_IDLE) {
        m_error = MediaError::create(MediaError::MEDIA_ERR_ABORTED);
        
        // fire synchronous 'abort'
        bool totalKnown = m_player && m_player->totalBytesKnown();
        unsigned loaded = m_player ? m_player->bytesLoaded() : 0;
        unsigned total = m_player ? m_player->totalBytes() : 0;
        dispatchProgressEvent(eventNames().abortEvent, totalKnown, loaded, total);
    }
    
    // 5 
    m_error = 0;
    m_autoplaying = true;

    // 6
    setPlaybackRate(defaultPlaybackRate());
    
    // 7
    if (m_networkState != NETWORK_EMPTY) {
        m_networkState = NETWORK_EMPTY;
        m_readyState = HAVE_NOTHING;
        m_paused = true;
        m_seeking = false;
        if (m_player) {
            m_player->pause();
            m_player->seek(0);
        }
        dispatchEventForType(eventNames().emptiedEvent, false, true);
    }
    
    selectMediaResource();
    m_processingLoad = false;
}

void HTMLMediaElement::selectMediaResource()
{
    // 1 - If the media element has neither a src attribute nor any source element children, run these substeps
    String mediaSrc = getAttribute(srcAttr);
    if (!mediaSrc && !havePotentialSourceChild()) {
        m_loadState = WaitingForSource;

        // 1 -  Set the networkState to NETWORK_NO_SOURCE
        m_networkState = NETWORK_NO_SOURCE;
        
        // 2 - While the media element has neither a src attribute nor any source element children, 
        // wait. (This steps might wait forever.)

        m_delayingTheLoadEvent = false;
        return;
    }

    // 2
    m_delayingTheLoadEvent = true;

    // 3
    m_networkState = NETWORK_LOADING;

    // 4
    scheduleProgressEvent(eventNames().loadstartEvent);

    // 5 - If the media element has a src attribute, then run these substeps
    ContentType contentType("");
    if (!mediaSrc.isEmpty()) {
        mediaSrc = document()->completeURL(mediaSrc).string();
        m_loadState = LoadingFromSrcAttr;
        loadResource(mediaSrc, contentType);
        return;
    }

    // Otherwise, the source elements will be used
    m_currentSourceNode = 0;
    loadNextSourceChild();
}

void HTMLMediaElement::loadNextSourceChild()
{
    ContentType contentType("");
    String mediaSrc;

    mediaSrc = nextSourceChild(&contentType);
    if (mediaSrc.isEmpty()) {
        noneSupported();
        return;
    }

    m_loadState = LoadingFromSourceElement;
    loadResource(mediaSrc, contentType);
}

void HTMLMediaElement::loadResource(String url, ContentType& contentType)
{
    Frame* frame = document()->frame();
    FrameLoader* loader = frame ? frame->loader() : 0;

    // don't allow remote to local urls
    if (!loader || !loader->canLoad(KURL(KURL(), url), String(), document())) {
        FrameLoader::reportLocalLoadFailed(frame, url);

        // If we rejected the url from a <source> element and there are more <source> children, schedule
        // the next one without reporting an error
        if (m_loadState == LoadingFromSourceElement && havePotentialSourceChild())
            scheduleLoad();
        else
            noneSupported();

        return;
    }

    // The resource fetch algorithm 
    m_networkState = NETWORK_LOADING;

    m_currentSrc = url;

    if (m_sendProgressEvents) 
        startProgressEventTimer();

#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    m_player.clear();
    m_player.set(new MediaPlayer(this));
#else
    if (!m_player)
        m_player.set(new MediaPlayer(this));
#endif

    updateVolume();

    m_player->load(m_currentSrc, contentType);
    
    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::startProgressEventTimer()
{
    if (m_progressEventTimer.isActive())
        return;

    m_previousProgressTime = WTF::currentTime();
    m_previousProgress = 0;
    // 350ms is not magic, it is in the spec!
    m_progressEventTimer.startRepeating(0.350);
}

void HTMLMediaElement::noneSupported()
{
    stopPeriodicTimers();
    m_loadState = WaitingForSource;
    m_currentSourceNode = 0;

    // 3 - Reaching this step indicates that either the URL failed to resolve, or the media 
    // resource failed to load. Set the error attribute to a new MediaError object whose 
    // code attribute is set to MEDIA_ERR_NONE_SUPPORTED.
    m_error = MediaError::create(MediaError::MEDIA_ERR_NONE_SUPPORTED);

    // 4- Set the element's networkState attribute to the NETWORK_NO_SOURCE value.
    m_networkState = NETWORK_NO_SOURCE;

    // 5 - Queue a task to fire a progress event called error at the media element.
    scheduleProgressEvent(eventNames().errorEvent); 

    // 6 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    m_delayingTheLoadEvent = false;

    // Abort these steps. Until the load() method is invoked, the element won't attempt to load another resource.
    
    if (isVideo())
        static_cast<HTMLVideoElement*>(this)->updatePosterImage();
    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::mediaEngineError(PassRefPtr<MediaError> err)
{
    // 1 - The user agent should cancel the fetching process.
    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    // 2 - Set the error attribute to a new MediaError object whose code attribute is 
    // set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
    m_error = err;

    // 3 - Queue a task to fire a progress event called error at the media element.
    scheduleProgressEvent(eventNames().errorEvent); 

    // 3 - Set the element's networkState attribute to the NETWORK_EMPTY value and queue a 
    // task to fire a simple event called emptied at the element.
    m_networkState = NETWORK_EMPTY;
    scheduleEvent(eventNames().emptiedEvent);

    // 4 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    m_delayingTheLoadEvent = false;

    // 5 - Abort the overall resource selection algorithm.
    m_currentSourceNode = 0;

}

void HTMLMediaElement::mediaPlayerNetworkStateChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    setNetworkState(m_player->networkState());
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::setNetworkState(MediaPlayer::NetworkState state)
{
    if (state == MediaPlayer::Empty) {
        // just update the cached state and leave, we can't do anything 
        m_networkState = NETWORK_EMPTY;
        return;
    }

    if (state == MediaPlayer::FormatError || state == MediaPlayer::NetworkError || state == MediaPlayer::DecodeError) {
        stopPeriodicTimers();

        // If we failed while trying to load a <source> element, the movie was never parsed, and there are more
        // <source> children, schedule the next one without reporting an error
        if (m_readyState < HAVE_METADATA && m_loadState == LoadingFromSourceElement && havePotentialSourceChild()) {
            scheduleLoad();
            return;
        }

        if (state == MediaPlayer::NetworkError)
            mediaEngineError(MediaError::create(MediaError::MEDIA_ERR_NETWORK));
        else if (state == MediaPlayer::DecodeError)
            mediaEngineError(MediaError::create(MediaError::MEDIA_ERR_DECODE));
        else if (state == MediaPlayer::FormatError)
            noneSupported();

        if (isVideo())
            static_cast<HTMLVideoElement*>(this)->updatePosterImage();

        return;
    }

    if (state == MediaPlayer::Idle) {
        ASSERT(static_cast<ReadyState>(m_player->readyState()) < HAVE_ENOUGH_DATA);
        if (m_networkState > NETWORK_IDLE) {
            stopPeriodicTimers();
            scheduleProgressEvent(eventNames().suspendEvent);
        }
        m_networkState = NETWORK_IDLE;
    }

    if (state == MediaPlayer::Loading) {
        ASSERT(static_cast<ReadyState>(m_player->readyState()) < HAVE_ENOUGH_DATA);
        if (m_networkState < NETWORK_LOADING || m_networkState == NETWORK_NO_SOURCE)
            startProgressEventTimer();
        m_networkState = NETWORK_LOADING;
    }

    if (state == MediaPlayer::Loaded) {
        NetworkState oldState = m_networkState;

        m_networkState = NETWORK_LOADED;
        if (oldState < NETWORK_LOADED || oldState == NETWORK_NO_SOURCE) {
            m_progressEventTimer.stop();

            // Check to see if readyState changes need to be dealt with before sending the 
            // 'load' event so we report 'canplaythrough' first. This is necessary because a
            //  media engine reports readyState and networkState changes separately
            MediaPlayer::ReadyState currentState = m_player->readyState();
            if (static_cast<ReadyState>(currentState) != m_readyState)
                setReadyState(currentState);

             scheduleProgressEvent(eventNames().loadEvent); 
        }
    }
}

void HTMLMediaElement::mediaPlayerReadyStateChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();

    setReadyState(m_player->readyState());

    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::setReadyState(MediaPlayer::ReadyState state)
{
    // Set "wasPotentiallyPlaying" BEFORE updating m_readyState, potentiallyPlaying() uses it
    bool wasPotentiallyPlaying = potentiallyPlaying();

    ReadyState oldState = m_readyState;
    m_readyState = static_cast<ReadyState>(state);

    if (m_readyState == oldState)
        return;
    
    if (m_readyState >= HAVE_CURRENT_DATA)
        m_seeking = false;
    
    if (m_networkState == NETWORK_EMPTY)
        return;

    if (m_seeking && m_readyState < HAVE_CURRENT_DATA) {
        // 4.8.10.10, step 9
        scheduleEvent(eventNames().seekingEvent);
        m_seeking = false;
    }

    if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA) {
        // 4.8.10.9
        scheduleTimeupdateEvent(false);
        scheduleEvent(eventNames().waitingEvent);
    }

    if (m_readyState >= HAVE_METADATA && oldState < HAVE_METADATA) {
        scheduleEvent(eventNames().durationchangeEvent);
        scheduleEvent(eventNames().loadedmetadataEvent);

#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
        if (renderer() && !renderer()->isImage()) {
            static_cast<RenderVideo*>(renderer())->videoSizeChanged();
        }
#endif        
        m_delayingTheLoadEvent = false;
        m_player->seek(0);
    }

    // 4.8.10.7 says loadeddata is sent only when the new state *is* HAVE_CURRENT_DATA: "If the
    // previous ready state was HAVE_METADATA and the new ready state is HAVE_CURRENT_DATA", 
    // but the event table at the end of the spec says it is sent when: "readyState newly 
    // increased to HAVE_CURRENT_DATA  or greater for the first time"
    // We go with the later because it seems useful to count on getting this event
    if (m_readyState >= HAVE_CURRENT_DATA && oldState < HAVE_CURRENT_DATA && !m_haveFiredLoadedData) {
        m_haveFiredLoadedData = true;
        scheduleEvent(eventNames().loadeddataEvent);
    }

    bool isPotentiallyPlaying = potentiallyPlaying();
    if (m_readyState == HAVE_FUTURE_DATA && oldState <= HAVE_CURRENT_DATA) {
        scheduleEvent(eventNames().canplayEvent);
        if (isPotentiallyPlaying)
            scheduleEvent(eventNames().playingEvent);

        if (isVideo())
            static_cast<HTMLVideoElement*>(this)->updatePosterImage();
    }

    if (m_readyState == HAVE_ENOUGH_DATA && oldState < HAVE_ENOUGH_DATA) {
        if (oldState <= HAVE_CURRENT_DATA)
            scheduleEvent(eventNames().canplayEvent);

        scheduleEvent(eventNames().canplaythroughEvent);

        if (isPotentiallyPlaying && oldState <= HAVE_CURRENT_DATA)
            scheduleEvent(eventNames().playingEvent);

        if (m_autoplaying && m_paused && autoplay()) {
            m_paused = false;
            scheduleEvent(eventNames().playEvent);
            scheduleEvent(eventNames().playingEvent);
        }

        if (isVideo())
            static_cast<HTMLVideoElement*>(this)->updatePosterImage();
    }

    updatePlayState();
}

void HTMLMediaElement::progressEventTimerFired(Timer<HTMLMediaElement>*)
{
    ASSERT(m_player);
    if (m_networkState == NETWORK_EMPTY || m_networkState >= NETWORK_LOADED)
        return;

    unsigned progress = m_player->bytesLoaded();
    double time = WTF::currentTime();
    double timedelta = time - m_previousProgressTime;

    if (progress == m_previousProgress) {
        if (timedelta > 3.0 && !m_sentStalledEvent) {
            scheduleProgressEvent(eventNames().stalledEvent);
            m_sentStalledEvent = true;
        }
    } else {
        scheduleProgressEvent(eventNames().progressEvent);
        m_previousProgress = progress;
        m_previousProgressTime = time;
        m_sentStalledEvent = false;
    }
}

void HTMLMediaElement::seek(float time, ExceptionCode& ec)
{
    // 4.8.10.10. Seeking
    // 1
    if (m_readyState == HAVE_NOTHING || !m_player) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 2
    time = min(time, duration());

    // 3
    time = max(time, 0.0f);

    // 4
    RefPtr<TimeRanges> seekableRanges = seekable();
    if (!seekableRanges->contain(time)) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    // avoid generating events when the time won't actually change
    float now = currentTime();
    if (time == now)
        return;

    // 5
    m_currentTimeDuringSeek = time;

    // 6 - set the seeking flag, it will be cleared when the engine tells is the time has actually changed
    m_seeking = true;

    // 7
    scheduleTimeupdateEvent(false);

    // 8 - this is covered, if necessary, when the engine signals a readystate change

    // 10
    m_player->seek(time);
    m_sentEndEvent = false;
}

HTMLMediaElement::ReadyState HTMLMediaElement::readyState() const
{
    return m_readyState;
}

bool HTMLMediaElement::seeking() const
{
    return m_seeking;
}

// playback state
float HTMLMediaElement::currentTime() const
{
    if (!m_player)
        return 0;
    if (m_seeking)
        return m_currentTimeDuringSeek;
    return m_player->currentTime();
}

void HTMLMediaElement::setCurrentTime(float time, ExceptionCode& ec)
{
    seek(time, ec);
}

float HTMLMediaElement::duration() const
{
    if (m_readyState >= HAVE_METADATA)
        return m_player->duration();

    return numeric_limits<float>::quiet_NaN();
}

bool HTMLMediaElement::paused() const
{
    return m_paused;
}

float HTMLMediaElement::defaultPlaybackRate() const
{
    return m_defaultPlaybackRate;
}

void HTMLMediaElement::setDefaultPlaybackRate(float rate)
{
    if (m_defaultPlaybackRate != rate) {
        m_defaultPlaybackRate = rate;
        scheduleEvent(eventNames().ratechangeEvent);
    }
}

float HTMLMediaElement::playbackRate() const
{
    return m_player ? m_player->rate() : 0;
}

void HTMLMediaElement::setPlaybackRate(float rate)
{
    if (m_playbackRate != rate) {
        m_playbackRate = rate;
        scheduleEvent(eventNames().ratechangeEvent);
    }
    if (m_player && potentiallyPlaying() && m_player->rate() != rate)
        m_player->setRate(rate);
}

bool HTMLMediaElement::ended() const
{
    return endedPlayback();
}

bool HTMLMediaElement::autoplay() const
{
	// the plug-in proxy always runs in fullscreen and will start playback itself so don't also try to do it here
    return false;
}

void HTMLMediaElement::setAutoplay(bool b)
{
    setBooleanAttribute(autoplayAttr, b);
}

bool HTMLMediaElement::autobuffer() const
{
    return hasAttribute(autobufferAttr);
}

void HTMLMediaElement::setAutobuffer(bool b)
{
    setBooleanAttribute(autobufferAttr, b);
}

void HTMLMediaElement::play()
{
    // Playback causes the element to go fullscreen if it isn't already, so don't allow it unless the element is in
    // the document. This isn't strictly necessary becuase a plug-in isn't instantiated until it is inserted into
    // the document, and we can't go fullscreen until there is a plug-in, but do the check anyway in case that
    // behavior changes in the future.
    if (!inDocument())
        return;

    if (m_restrictions & RequireUserGestureForRateChangeRestriction && !processingUserGesture())
        return;

    playInternal();
}

void HTMLMediaElement::playInternal()
{
    // 4.8.10.9. Playing the media resource
    if (!m_player || m_networkState == NETWORK_EMPTY)
        scheduleLoad();

    if (endedPlayback()) {
        ExceptionCode unused;
        seek(0, unused);
    }
    
    setPlaybackRate(defaultPlaybackRate());
    
    if (m_paused) {
        m_paused = false;
        scheduleEvent(eventNames().playEvent);

        if (m_readyState <= HAVE_CURRENT_DATA)
            scheduleEvent(eventNames().waitingEvent);
        else if (m_readyState >= HAVE_FUTURE_DATA)
            scheduleEvent(eventNames().playingEvent);
    }
    m_autoplaying = false;

    m_requestingPlay = true;

    updatePlayState();
}

void HTMLMediaElement::pause()
{
    if (m_restrictions & RequireUserGestureForRateChangeRestriction && !processingUserGesture())
        return;

    pauseInternal();
}


void HTMLMediaElement::pauseInternal()
{
    // 4.8.10.9. Playing the media resource
    if (!m_player || m_networkState == NETWORK_EMPTY) {

        // Don't trigger loading if a script calls pause()
        return;

        scheduleLoad();
    }

    m_autoplaying = false;
    
    if (!m_paused) {
        m_paused = true;
        scheduleTimeupdateEvent(false);
        scheduleEvent(eventNames().pauseEvent);
    }

    updatePlayState();
}

bool HTMLMediaElement::loop() const
{
    return hasAttribute(loopAttr);
}

void HTMLMediaElement::setLoop(bool b)
{
    setBooleanAttribute(loopAttr, b);
}

bool HTMLMediaElement::controls() const
{
    return hasAttribute(controlsAttr);
}

void HTMLMediaElement::setControls(bool b)
{
    setBooleanAttribute(controlsAttr, b);
}

float HTMLMediaElement::volume() const
{
    return m_volume;
}

void HTMLMediaElement::setVolume(float vol, ExceptionCode& ec)
{
    if (vol < 0.0f || vol > 1.0f) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    if (m_volume != vol) {
        m_volume = vol;
        updateVolume();
        scheduleEvent(eventNames().volumechangeEvent);
    }
}

bool HTMLMediaElement::muted() const
{
    return m_muted;
}

void HTMLMediaElement::setMuted(bool muted)
{
    if (m_muted != muted) {
        m_muted = muted;
        updateVolume();
        scheduleEvent(eventNames().volumechangeEvent);
    }
}

void HTMLMediaElement::togglePlayState()
{
    // We can safely call the internal play/pause methods, which don't check restrictions, because
    // this method is only called from the built-in media controller
    if (canPlay())
        playInternal();
    else 
        pauseInternal();
}

void HTMLMediaElement::beginScrubbing()
{
    if (!paused()) {
        if (ended()) {
            // Because a media element stays in non-paused state when it reaches end, playback resumes 
            // when the slider is dragged from the end to another position unless we pause first. Do 
            // a "hard pause" so an event is generated, since we want to stay paused after scrubbing finishes.
            pause();
        } else {
            // Not at the end but we still want to pause playback so the media engine doesn't try to
            // continue playing during scrubbing. Pause without generating an event as we will 
            // unpause after scrubbing finishes.
            setPausedInternal(true);
        }
    }
}

void HTMLMediaElement::endScrubbing()
{
    if (m_pausedInternal)
        setPausedInternal(false);
}

// The spec says to fire periodic timeupdate events (those sent while playing) every
// "15 to 250ms", we choose the slowest frequency
static const double maxTimeupdateEventFrequency = 0.25;

void HTMLMediaElement::startPlaybackProgressTimer()
{
    if (m_playbackProgressTimer.isActive())
        return;

    m_previousProgressTime = WTF::currentTime();
    m_previousProgress = 0;
    m_playbackProgressTimer.startRepeating(maxTimeupdateEventFrequency);
}

void HTMLMediaElement::playbackProgressTimerFired(Timer<HTMLMediaElement>*)
{
    ASSERT(m_player);
    if (!m_playbackRate)
        return;

    scheduleTimeupdateEvent(true);
    
    // FIXME: deal with cue ranges here
}

void HTMLMediaElement::scheduleTimeupdateEvent(bool periodicEvent)
{
    double now = WTF::currentTime();
    double timedelta = now - m_lastTimeUpdateEventWallTime;

    // throttle the periodic events
    if (periodicEvent && timedelta < maxTimeupdateEventFrequency)
        return;

    // Some media engines make multiple "time changed" callbacks at the same time, but we only want one
    // event at a given time so filter here
    float movieTime = m_player ? m_player->currentTime() : 0;
    if (movieTime != m_lastTimeUpdateEventMovieTime) {
        scheduleEvent(eventNames().timeupdateEvent);
        m_lastTimeUpdateEventWallTime = now;
        m_lastTimeUpdateEventMovieTime = movieTime;
    }
}

bool HTMLMediaElement::canPlay() const
{
    return paused() || ended() || m_readyState < HAVE_METADATA;
}

bool HTMLMediaElement::havePotentialSourceChild()
{
    // Stash the current <source> node so we can restore it after checking
    // to see there is another potential
    Node* currentSourceNode = m_currentSourceNode;
    String nextUrl = nextSourceChild();
    m_currentSourceNode = currentSourceNode;

    return !nextUrl.isEmpty();
}

String HTMLMediaElement::nextSourceChild(ContentType *contentType)
{
    String mediaSrc;
    bool lookingForPreviousNode = m_currentSourceNode;

    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (!node->hasTagName(sourceTag))
            continue;

        if (lookingForPreviousNode) {
            if (m_currentSourceNode == node)
                lookingForPreviousNode = false;
            continue;
        }
        
        HTMLSourceElement* source = static_cast<HTMLSourceElement*>(node);
        if (!source->hasAttribute(srcAttr))
            continue; 
        if (source->hasAttribute(mediaAttr)) {
            MediaQueryEvaluator screenEval("screen", document()->frame(), renderer() ? renderer()->style() : 0);
            RefPtr<MediaList> media = MediaList::createAllowingDescriptionSyntax(source->media());
            if (!screenEval.eval(media.get()))
                continue;
        }
        if (source->hasAttribute(typeAttr)) {
            ContentType type(source->type());
            if (!MediaPlayer::supportsType(type))
                continue;

            // return type with all parameters in place so the media engine can use them
            if (contentType)
                *contentType = type;
        }
        mediaSrc = source->src().string();
        m_currentSourceNode = node;
        break;
    }

    if (!mediaSrc.isEmpty())
        mediaSrc = document()->completeURL(mediaSrc).string();

    return mediaSrc;
}

void HTMLMediaElement::mediaPlayerTimeChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();

    if (m_readyState >= HAVE_CURRENT_DATA && m_seeking) {
        scheduleEvent(eventNames().seekedEvent);
        m_seeking = false;
    }
    
    float now = currentTime();
    float dur = duration();
    if (!isnan(dur) && dur && now >= dur) {
        if (loop()) {
            ExceptionCode ignoredException;
            m_sentEndEvent = false;
            seek(0, ignoredException);
        } else {
            if (!m_sentEndEvent) {
                m_sentEndEvent = true;
                scheduleTimeupdateEvent(false);
                scheduleEvent(eventNames().endedEvent);
            }
        }
    }
    else {
        // The controller changes movie time directly instead of calling through here so we need to post timeupdate events
        // in response to time changes
        scheduleTimeupdateEvent(false);
        m_sentEndEvent = false;
    }

    updatePlayState();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerRepaint(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    if (renderer())
        renderer()->repaint();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerVolumeChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    updateVolume();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerDurationChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    scheduleEvent(eventNames().durationchangeEvent);
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    if (renderer()) {
        renderer()->updateFromElement();
        if (!renderer()->isImage())
            static_cast<RenderVideo*>(renderer())->videoSizeChanged();
    }
#endif        
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerRateChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    // Stash the rate in case the one we tried to set isn't what the engine is
    // using (eg. it can't handle the rate we set)
    m_playbackRate = m_player->rate();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerSizeChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    if (renderer() && !renderer()->isImage())
        static_cast<RenderVideo*>(renderer())->videoSizeChanged();
#endif        
    endProcessingMediaPlayerCallback();
}

PassRefPtr<TimeRanges> HTMLMediaElement::buffered() const
{
    // FIXME real ranges support
    if (!m_player || !m_player->maxTimeBuffered())
        return TimeRanges::create();
    return TimeRanges::create(0, m_player->maxTimeBuffered());
}

PassRefPtr<TimeRanges> HTMLMediaElement::played() const
{
    // FIXME track played
    return TimeRanges::create();
}

PassRefPtr<TimeRanges> HTMLMediaElement::seekable() const
{
    // FIXME real ranges support
    if (!m_player || !m_player->maxTimeSeekable())
        return TimeRanges::create();
    return TimeRanges::create(0, m_player->maxTimeSeekable());
}

bool HTMLMediaElement::potentiallyPlaying() const
{
    // Don't base playability on readystate because we don't know anything about the movie until we enter fullscreen
    return !paused() && m_networkState < NETWORK_NO_SOURCE && !endedPlayback() && !stoppedDueToErrors() && !pausedForUserInteraction();
}

bool HTMLMediaElement::endedPlayback() const
{
    if (!m_player || m_readyState < HAVE_METADATA)
        return false;
    
    float dur = duration();
    return !isnan(dur) && currentTime() >= dur && !loop();
}

bool HTMLMediaElement::stoppedDueToErrors() const
{
    if (m_readyState >= HAVE_METADATA && m_error) {
        RefPtr<TimeRanges> seekableRanges = seekable();
        if (!seekableRanges->contain(currentTime()))
            return true;
    }
    
    return false;
}

bool HTMLMediaElement::pausedForUserInteraction() const
{
    // Return true when not in fullscreen unless we are in the process of beginning playback so potentiallyPlaying returns
    // false when not in fullscreeen unless when we need to start playback.
    return !m_inFullScreen && !m_requestingPlay;
}

void HTMLMediaElement::updateVolume()
{
    if (!m_player)
        return;

    // Avoid recursion when the player reports volume changes.
    if (!processingMediaPlayerCallback()) {
        Page* page = document()->page();
        float volumeMultiplier = page ? page->mediaVolume() : 1;
    
        m_player->setVolume(m_muted ? 0 : m_volume * volumeMultiplier);
    }
    
    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::updatePlayState()
{
    if (!m_player)
        return;

    if (m_pausedInternal) {
        if (!m_player->paused())
            m_player->pause();
        m_playbackProgressTimer.stop();
        return;
    }
    
    bool shouldBePlaying = potentiallyPlaying();
    bool playerPaused = m_player->paused();
    if (shouldBePlaying && playerPaused) {
        // Set rate before calling play in case the rate was set before the media engine wasn't setup.
        // The media engine should just stash the rate since it isn't already playing.
        m_player->setRate(m_playbackRate);
        m_player->play();
        startPlaybackProgressTimer();
    } else if (!shouldBePlaying && !playerPaused) {
        m_player->pause();
        m_playbackProgressTimer.stop();
    }

    m_requestingPlay = false;

    if (renderer())
        renderer()->updateFromElement();
}
    
void HTMLMediaElement::setPausedInternal(bool b)
{
    m_pausedInternal = b;
    updatePlayState();
}

void HTMLMediaElement::stopPeriodicTimers()
{
    m_progressEventTimer.stop();
    m_playbackProgressTimer.stop();
}

void HTMLMediaElement::userCancelledLoad()
{
    if (m_networkState != NETWORK_EMPTY) {

        // If the media data fetching process is aborted by the user:

        // 1 - The user agent should cancel the fetching process.
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
        m_player.clear();
#endif
        stopPeriodicTimers();

        // 2 - Set the error attribute to a new MediaError object whose code attribute is set to MEDIA_ERR_ABORT.
        m_error = MediaError::create(MediaError::MEDIA_ERR_ABORTED);

        // 3 - Queue a task to fire a progress event called abort at the media element.
        scheduleProgressEvent(eventNames().abortEvent);

        // 4 - If the media element's readyState attribute has a value equal to HAVE_NOTHING, set the 
        // element's networkState attribute to the NETWORK_EMPTY value and queue a task to fire a 
        // simple event called emptied at the element. Otherwise, set set the element's networkState 
        // attribute to the NETWORK_IDLE value.
        if (m_networkState >= NETWORK_LOADING) {
            m_networkState = NETWORK_EMPTY;
            m_readyState = HAVE_NOTHING;
            scheduleEvent(eventNames().emptiedEvent);
        }

        // 5 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
        m_delayingTheLoadEvent = false;
    }
}

void HTMLMediaElement::documentWillBecomeInactive()
{
    m_inActiveDocument = false;
    userCancelledLoad();

    // Stop the playback without generating events
    setPausedInternal(true);

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::documentDidBecomeActive()
{
    m_inActiveDocument = true;
    setPausedInternal(false);

    if (m_error && m_error->code() == MediaError::MEDIA_ERR_ABORTED) {
        // Restart the load if it was aborted in the middle by moving the document to the page cache.
        // m_error is only left at MEDIA_ERR_ABORTED when the document becomes inactive (it is set to
        //  MEDIA_ERR_ABORTED while the abortEvent is being sent, but cleared immediately afterwards).
        // This behavior is not specified but it seems like a sensible thing to do.
        ExceptionCode ec;
        load(ec);
    }
        
    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::mediaVolumeDidChange()
{
    updateVolume();
}

void HTMLMediaElement::defaultEventHandler(Event* event)
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    RenderObject* r = renderer();
    if (!r || !r->isWidget())
        return;

    Widget* widget = static_cast<RenderWidget*>(r)->widget();
    if (widget)
        widget->handleEvent(event);
#else
    if (renderer() && renderer()->isMedia())
        static_cast<RenderMedia*>(renderer())->forwardEvent(event);
    if (event->defaultHandled())
        return;
    HTMLElement::defaultEventHandler(event);
#endif
}

bool HTMLMediaElement::processingUserGesture() const
{
    Frame* frame = document()->frame();
    FrameLoader* loader = frame ? frame->loader() : 0;

    // return 'true' for safety if we don't know the answer 
    return loader ? loader->userGestureHint() : true;
}

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
void HTMLMediaElement::deliverNotification(MediaPlayerProxyNotificationType notification)
{
    if (notification == MediaPlayerNotificationPlayPauseButtonPressed) {
        togglePlayState();
        return;
    } 

    if (notification == MediaPlayerNotificationEnteredFullScreen)
        m_inFullScreen = true;
    else if  (notification == MediaPlayerNotificationExitedFullScreen) {
        // See comment below for why we don't set m_inFullScreen to false here.
        if (!m_paused)
            pauseInternal();
    } else if (notification == MediaPlayerNotificationMediaValidated || notification == MediaPlayerNotificationReadyForInspection) {
        // The media player sometimes reports an apparently spurious error just as we request playback, and then follows almost 
        // immediately with ReadyForInspection and/or MediaValidated. The spec doesn't deal with a "fatal" error followed
        // by ressurection, so if we have set an error clear it now.
        m_error = 0;
    }

    if (m_player)
        m_player->deliverNotification(notification);

    // Set m_inFullScreen to false *after* processing the notification so we post the appropriate events
    // if exited while playing.
    if  (notification == MediaPlayerNotificationExitedFullScreen)
        m_inFullScreen = false;
}

void HTMLMediaElement::setMediaPlayerProxy(WebMediaPlayerProxy* proxy)
{
    if (!m_player)
        m_player.set(new MediaPlayer(this));
    if (m_player)
        m_player->setMediaPlayerProxy(proxy);
}

String HTMLMediaElement::initialURL()
{
    String initialSrc = getAttribute(srcAttr);
    
    if (initialSrc.isEmpty())
        initialSrc = nextSourceChild();

    if (!initialSrc.isEmpty())
        initialSrc = document()->completeURL(initialSrc).string();

    m_currentSrc = initialSrc;

    return initialSrc;
}

void HTMLMediaElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    if (!m_player)
        m_player.set(new MediaPlayer(this));
    
    document()->updateRendering();
    if (m_needWidgetUpdate && renderer())
        static_cast<RenderPartObject*>(renderer())->updateWidget(true);
}
#endif

}

#endif
